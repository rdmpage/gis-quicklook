/*
 *  ReadDBF.c
 *  GISLook
 *
 *  Renders previews and thumbnails for .dbf (attribute tables) and
 *  .shx (shapefile spatial index) companion files.
 *
 *  .dbf → formatted column grid using CoreText
 *  .shx → brief info panel (shape type, record count)
 */

#include "ReadDBF.h"
#include "shapefil.h"         /* DBFOpen / DBFGetFieldInfo / DBFReadStringAttribute */

#include <CoreText/CoreText.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ── Canvas sizes ─────────────────────────────────────────────────────────── */

#define PREVIEW_W  800.0f
#define PREVIEW_H  600.0f
#define THUMB_W    256.0f
#define THUMB_H    256.0f

/* ── Private helpers ──────────────────────────────────────────────────────── */

/* Create an opaque RGB color.  Caller must CGColorRelease the result. */
static CGColorRef rgbColor(CGFloat r, CGFloat g, CGFloat b)
{
    CGColorSpaceRef cs  = CGColorSpaceCreateDeviceRGB();
    CGFloat         c[] = { r, g, b, 1.0f };
    CGColorRef      col = CGColorCreate(cs, c);
    CGColorSpaceRelease(cs);
    return col;
}

/* Draw UTF-8 text with its baseline at (x, y).
 * Falls back to Windows Latin-1 encoding if the string is not valid UTF-8
 * (common in legacy DBF files). */
static void drawText(CGContextRef ctx,
                     CGFloat x, CGFloat y,
                     const char *str,
                     CTFontRef font, CGColorRef color)
{
    if (!str || str[0] == '\0') return;

    CFStringRef cfstr = CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
    if (!cfstr)
        cfstr = CFStringCreateWithCString(NULL, str, kCFStringEncodingWindowsLatin1);
    if (!cfstr) return;

    const void *keys[]   = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    const void *values[] = { font, color };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, keys, values, 2,
                                               &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef astr = CFAttributedStringCreate(NULL, cfstr, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(astr);

    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGContextSetTextPosition(ctx, x, y);
    CTLineDraw(line, ctx);

    CFRelease(line);
    CFRelease(astr);
    CFRelease(attrs);
    CFRelease(cfstr);
}

/* Draw text clipped horizontally to the cell [cellX, cellX+cellW).
 * Baseline is placed 4 pt above the cell bottom for visual centering. */
static void drawCell(CGContextRef ctx,
                     CGFloat cellX, CGFloat cellY, CGFloat cellW, CGFloat cellH,
                     const char *text, CTFontRef font, CGColorRef color)
{
    CGContextSaveGState(ctx);
    /* Clip: leave 3 pt of inner padding on each side */
    CGContextClipToRect(ctx, CGRectMake(cellX + 3.0f, cellY, cellW - 6.0f, cellH));
    drawText(ctx, cellX + 4.0f, cellY + 4.0f, text, font, color);
    CGContextRestoreGState(ctx);
}

/* ── DBF attribute-table renderer ─────────────────────────────────────────── */

static bool renderDBFTable(CGContextRef ctx,
                            CGFloat W, CGFloat H,
                            const char *path,
                            bool thumb)
{
    DBFHandle hDBF = DBFOpen(path, "rb");
    if (!hDBF) return false;

    int nFields  = DBFGetFieldCount(hDBF);
    int nRecords = DBFGetRecordCount(hDBF);
    if (nFields <= 0) { DBFClose(hDBF); return false; }

    /* ── Layout parameters ── */
    CGFloat pad          = thumb ?  6.0f : 12.0f;
    CGFloat titleH       = thumb ? 16.0f : 24.0f;
    CGFloat hdrH         = thumb ? 14.0f : 20.0f;
    CGFloat rowH         = thumb ? 13.0f : 17.0f;
    CGFloat footerH      = thumb ?  0.0f : 16.0f;
    CGFloat colMinW      = thumb ? 40.0f : 55.0f;
    CGFloat colMaxW      = thumb ? 90.0f : 150.0f;
    int     maxCols      = thumb ?  4    : 10;
    CGFloat titleFontSz  = thumb ?  8.5f : 12.0f;
    CGFloat hdrFontSz    = thumb ?  7.5f : 10.5f;
    CGFloat bodyFontSz   = thumb ?  7.0f : 10.0f;
    CGFloat footerFontSz =                  9.0f;

    /* ── White background ── */
    CGContextSetFillColorWithColor(ctx, CGColorGetConstantColor(kCGColorWhite));
    CGContextFillRect(ctx, CGRectMake(0, 0, W, H));

    /* ── Fonts (all owned — CFRelease on exit) ── */
    CTFontRef titleFont  = CTFontCreateWithName(CFSTR("Helvetica-Bold"), titleFontSz,  NULL);
    CTFontRef hdrFont    = CTFontCreateWithName(CFSTR("Helvetica-Bold"), hdrFontSz,    NULL);
    CTFontRef bodyFont   = CTFontCreateWithName(CFSTR("Helvetica"),      bodyFontSz,   NULL);
    CTFontRef footerFont = CTFontCreateWithName(CFSTR("Helvetica"),      footerFontSz, NULL);

    /* ── Colors ── */
    /* Owned colors — must CGColorRelease */
    CGColorRef hdrBg    = rgbColor(0.22f, 0.47f, 0.74f);   /* steel blue header  */
    CGColorRef altRowBg = rgbColor(0.93f, 0.96f, 1.00f);   /* light blue alt row */
    CGColorRef gridCol  = rgbColor(0.72f, 0.79f, 0.88f);   /* grid lines         */
    CGColorRef grayCol  = rgbColor(0.50f, 0.50f, 0.50f);   /* footer text        */
    /* Constant colors — NOT owned, do not release */
    CGColorRef black    = CGColorGetConstantColor(kCGColorBlack);
    CGColorRef white    = CGColorGetConstantColor(kCGColorWhite);

    /* ── Column count and width ─────────────────────────────────────────────
     * Fit as many columns as possible without exceeding the canvas width.    */
    int     nCols     = nFields < maxCols ? nFields : maxCols;
    CGFloat maxTableW = W - 2.0f * pad;
    CGFloat colW      = maxTableW / (CGFloat)nCols;
    if (colW < colMinW) {
        nCols = (int)(maxTableW / colMinW);
        if (nCols < 1) nCols = 1;
        colW  = maxTableW / (CGFloat)nCols;
    }
    if (colW > colMaxW) colW = colMaxW;
    CGFloat tableW = colW * nCols;

    /* ── Row count: fill available vertical space ── */
    CGFloat usedH  = 2.0f * pad + footerH + titleH + hdrH;
    int     maxRows = (int)((H - usedH) / rowH);
    if (maxRows < 0) maxRows = 0;
    int     nRows  = nRecords < maxRows ? nRecords : maxRows;

    /* ── Title ── */
    {
        char title[128];
        snprintf(title, sizeof(title), "%d record%s  \xC3\x97  %d field%s",
                 nRecords, nRecords == 1 ? "" : "s",
                 nFields,  nFields  == 1 ? "" : "s");
        CGFloat titleBaseline = H - pad - titleFontSz - (titleH - titleFontSz) * 0.5f;
        drawText(ctx, pad, titleBaseline, title, titleFont, black);
    }

    /* ── Header row ── */
    CGFloat hdrBottom = H - pad - titleH - hdrH;
    CGContextSetFillColorWithColor(ctx, hdrBg);
    CGContextFillRect(ctx, CGRectMake(pad, hdrBottom, tableW, hdrH));

    char  fieldName[32];
    int   fieldWidth, fieldDecimals;
    for (int col = 0; col < nCols; col++) {
        DBFGetFieldInfo(hDBF, col, fieldName, &fieldWidth, &fieldDecimals);
        drawCell(ctx, pad + col * colW, hdrBottom, colW, hdrH,
                 fieldName, hdrFont, white);
    }

    /* ── Data rows ── */
    for (int row = 0; row < nRows; row++) {
        CGFloat rowBottom = hdrBottom - (row + 1) * rowH;

        if (row % 2 == 1) {   /* alternating background */
            CGContextSetFillColorWithColor(ctx, altRowBg);
            CGContextFillRect(ctx, CGRectMake(pad, rowBottom, tableW, rowH));
        }

        for (int col = 0; col < nCols; col++) {
            const char *val = "";
            if (!DBFIsAttributeNULL(hDBF, row, col)) {
                const char *s = DBFReadStringAttribute(hDBF, row, col);
                if (s) val = s;
            }
            drawCell(ctx, pad + col * colW, rowBottom, colW, rowH,
                     val, bodyFont, black);
        }
    }

    /* ── Grid lines ── */
    CGContextSetStrokeColorWithColor(ctx, gridCol);
    CGContextSetLineWidth(ctx, 0.5f);
    CGContextBeginPath(ctx);

    CGFloat tableTop    = hdrBottom + hdrH;
    CGFloat tableBottom = hdrBottom - nRows * rowH;

    /* Top border of header */
    CGContextMoveToPoint   (ctx, pad, tableTop);
    CGContextAddLineToPoint(ctx, pad + tableW, tableTop);

    /* Horizontal lines (header bottom + between data rows + table bottom) */
    for (int i = 0; i <= nRows; i++) {
        CGFloat y = hdrBottom - i * rowH;
        CGContextMoveToPoint   (ctx, pad, y);
        CGContextAddLineToPoint(ctx, pad + tableW, y);
    }

    /* Vertical column dividers */
    for (int col = 0; col <= nCols; col++) {
        CGFloat x = pad + col * colW;
        CGContextMoveToPoint   (ctx, x, tableTop);
        CGContextAddLineToPoint(ctx, x, tableBottom);
    }
    CGContextStrokePath(ctx);

    /* ── Truncation footer (preview only) ── */
    if (!thumb && (nRows < nRecords || nCols < nFields)) {
        char footer[160];
        if (nRows < nRecords && nCols < nFields)
            snprintf(footer, sizeof(footer),
                     "Showing %d of %d records, %d of %d fields",
                     nRows, nRecords, nCols, nFields);
        else if (nRows < nRecords)
            snprintf(footer, sizeof(footer),
                     "Showing %d of %d records", nRows, nRecords);
        else
            snprintf(footer, sizeof(footer),
                     "Showing %d of %d fields", nCols, nFields);
        drawText(ctx, pad, pad * 0.5f + 2.0f, footer, footerFont, grayCol);
    }

    /* ── Cleanup ── */
    CFRelease(titleFont);
    CFRelease(hdrFont);
    CFRelease(bodyFont);
    CFRelease(footerFont);
    CGColorRelease(hdrBg);
    CGColorRelease(altRowBg);
    CGColorRelease(gridCol);
    CGColorRelease(grayCol);
    DBFClose(hDBF);
    return true;
}

/* ── SHX info-panel renderer ─────────────────────────────────────────────── */

static const char *shapeTypeName(int t)
{
    switch (t) {
        case  0: return "Null Shape";
        case  1: return "Point";
        case  3: return "Polyline";
        case  5: return "Polygon";
        case  8: return "MultiPoint";
        case 11: return "PointZ";
        case 13: return "PolylineZ";
        case 15: return "PolygonZ";
        case 18: return "MultiPointZ";
        case 21: return "PointM";
        case 23: return "PolylineM";
        case 25: return "PolygonM";
        case 28: return "MultiPointM";
        case 31: return "MultiPatch";
        default: return "Unknown";
    }
}

static bool renderSHXInfo(CGContextRef ctx,
                           CGFloat W, CGFloat H,
                           const char *path,
                           bool thumb)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;

    unsigned char hdr[100];
    if (fread(hdr, 1, 100, fp) < 100) { fclose(fp); return false; }
    fclose(fp);

    /* Validate: SHP/SHX file code = 9994 (big-endian, bytes 0–3) */
    int fileCode = ((int)hdr[0] << 24) | ((int)hdr[1] << 16) |
                   ((int)hdr[2] << 8)  |  (int)hdr[3];
    if (fileCode != 9994) return false;

    /* File length in 16-bit words (big-endian, bytes 24–27).
     * SHX record count = (fileLength_bytes - 100) / 8  */
    int lenWords = ((int)hdr[24] << 24) | ((int)hdr[25] << 16) |
                   ((int)hdr[26] << 8)  |  (int)hdr[27];
    int nRecords = (lenWords * 2 - 100) / 8;
    if (nRecords < 0) nRecords = 0;

    /* Shape type (little-endian, bytes 32–35) */
    int shapeType = (int)hdr[32] | ((int)hdr[33] << 8) |
                    ((int)hdr[34] << 16) | ((int)hdr[35] << 24);

    /* ── White background ── */
    CGContextSetFillColorWithColor(ctx, CGColorGetConstantColor(kCGColorWhite));
    CGContextFillRect(ctx, CGRectMake(0, 0, W, H));

    CGFloat pad      = thumb ? 10.0f : 20.0f;
    CGFloat lineH    = thumb ? 14.0f : 20.0f;
    CGFloat titleSz  = thumb ? 10.0f : 15.0f;
    CGFloat bodySz   = thumb ?  8.5f : 12.0f;

    CTFontRef titleFont = CTFontCreateWithName(CFSTR("Helvetica-Bold"), titleSz, NULL);
    CTFontRef bodyFont  = CTFontCreateWithName(CFSTR("Helvetica"),      bodySz,  NULL);
    CGColorRef black    = CGColorGetConstantColor(kCGColorBlack);
    CGColorRef gray     = rgbColor(0.45f, 0.45f, 0.45f);   /* owned */

    CGFloat y = H - pad - titleSz;
    drawText(ctx, pad, y, "Shapefile Index (.shx)", titleFont, black);
    y -= lineH * 1.5f;

    char buf[128];
    snprintf(buf, sizeof(buf), "Shape type:  %s", shapeTypeName(shapeType));
    drawText(ctx, pad, y, buf, bodyFont, black);
    y -= lineH;

    snprintf(buf, sizeof(buf), "Records:     %d", nRecords);
    drawText(ctx, pad, y, buf, bodyFont, black);

    if (!thumb) {
        y -= lineH * 1.5f;
        drawText(ctx, pad, y,
                 "This file stores spatial offsets for the companion .shp file.",
                 bodyFont, gray);
    }

    CFRelease(titleFont);
    CFRelease(bodyFont);
    CGColorRelease(gray);
    return true;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

bool isDBF(CFStringRef contentTypeUTI, CFURLRef url)
{
    (void)contentTypeUTI;   /* UTI is com.esri.shape for both — use extension */
    CFStringRef ext = CFURLCopyPathExtension(url);
    if (!ext) return false;
    bool result =
        CFStringCompare(ext, CFSTR("dbf"), kCFCompareCaseInsensitive) == kCFCompareEqualTo ||
        CFStringCompare(ext, CFSTR("shx"), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
    CFRelease(ext);
    return result;
}

bool readDBF(QLPreviewRequestRef   preview,
             QLThumbnailRequestRef thumbnail,
             CFURLRef              url,
             CFStringRef           contentTypeUTI)
{
    (void)contentTypeUTI;

    unsigned char pathBuf[10240];
    if (!CFURLGetFileSystemRepresentation(url, true, pathBuf, sizeof(pathBuf)))
        return false;
    const char *path = (const char *)pathBuf;

    bool    isThumb = (thumbnail != NULL);
    CGFloat W = isThumb ? THUMB_W : PREVIEW_W;
    CGFloat H = isThumb ? THUMB_H : PREVIEW_H;

    CGContextRef ctx = NULL;
    if (preview)
        ctx = QLPreviewRequestCreateContext(preview, CGSizeMake(W, H), false, NULL);
    else if (thumbnail)
        ctx = QLThumbnailRequestCreateContext(thumbnail, CGSizeMake(W, H), false, NULL);
    if (!ctx) return false;

    bool result = false;
    CFStringRef ext = CFURLCopyPathExtension(url);
    if (ext) {
        if (CFStringCompare(ext, CFSTR("dbf"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
            result = renderDBFTable(ctx, W, H, path, isThumb);
        else if (CFStringCompare(ext, CFSTR("shx"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
            result = renderSHXInfo(ctx, W, H, path, isThumb);
        CFRelease(ext);
    }

    if (preview)
        QLPreviewRequestFlushContext(preview, ctx);
    else
        QLThumbnailRequestFlushContext(thumbnail, ctx);
    CFRelease(ctx);
    return result;
}
