/*
 *  GeoJSONShape.c
 *  GISLook
 *
 *  Renders GeoJSON (.geojson) files as map previews and thumbnails.
 *
 *  Supports the full GeoJSON geometry model (RFC 7946):
 *    Point, MultiPoint, LineString, MultiLineString,
 *    Polygon, MultiPolygon, GeometryCollection,
 *    Feature, FeatureCollection.
 *
 *  Uses cJSON for JSON parsing and follows the same drawing conventions
 *  as ESRIShape.c (same colors, circle radii, cancellation checks).
 *
 *  Files larger than MAX_GEOJSON_BYTES are silently skipped; the caller
 *  falls back to the raster renderer or a blank preview.
 */

#include "GeoJSONShape.h"
#include "ReadVector.h"   /* drawCircle, circleRadius,
                             setColorForPolygons, setColorForLines,
                             isValidQuartzCoord */
#include "cJSON.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

/* ── Constants ────────────────────────────────────────────────────────────── */

/* Skip files larger than this to avoid running out of memory in the
 * QuickLook sandbox (64 MB is plenty for most real-world GeoJSON). */
#define MAX_GEOJSON_BYTES  (64 * 1024 * 1024)

/* ── File I/O ─────────────────────────────────────────────────────────────── */

/* Read an entire file into a NUL-terminated heap buffer.
 * Returns NULL and does nothing if the file is too large or unreadable.
 * Caller must free() the returned buffer. */
static char *slurpFile(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);

    if (len <= 0 || len > MAX_GEOJSON_BYTES) { fclose(fp); return NULL; }

    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(fp); return NULL; }

    if ((long)fread(buf, 1, (size_t)len, fp) != len) {
        free(buf); fclose(fp); return NULL;
    }
    buf[len] = '\0';
    fclose(fp);
    return buf;
}

/* ── Bounding-box pass ────────────────────────────────────────────────────── */

/* Traverse a coordinate array at any nesting depth and accumulate
 * (xmin,ymin,xmax,ymax).  Stops recursing when it reaches a [lon,lat] leaf. */
static void gatherCoords(const cJSON *coords,
                          double *xmin, double *ymin,
                          double *xmax, double *ymax)
{
    if (!cJSON_IsArray(coords)) return;

    const cJSON *first = coords->child;
    if (!first) return;

    if (cJSON_IsNumber(first)) {
        /* Leaf: [lon, lat] (or [lon, lat, ele]) */
        double lon = first->valuedouble;
        const cJSON *lat_item = first->next;
        if (!lat_item || !cJSON_IsNumber(lat_item)) return;
        double lat = lat_item->valuedouble;
        if (lon < *xmin) *xmin = lon;
        if (lon > *xmax) *xmax = lon;
        if (lat < *ymin) *ymin = lat;
        if (lat > *ymax) *ymax = lat;
    } else {
        /* Nested arrays — recurse */
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, coords) {
            gatherCoords(item, xmin, ymin, xmax, ymax);
        }
    }
}

static void gatherGeomBounds(const cJSON *geom,
                              double *xmin, double *ymin,
                              double *xmax, double *ymax)
{
    if (!geom) return;
    const cJSON *typeItem = cJSON_GetObjectItemCaseSensitive(geom, "type");
    if (!typeItem || !cJSON_IsString(typeItem)) return;

    if (strcmp(typeItem->valuestring, "GeometryCollection") == 0) {
        const cJSON *geoms = cJSON_GetObjectItemCaseSensitive(geom, "geometries");
        const cJSON *g = NULL;
        cJSON_ArrayForEach(g, geoms) {
            gatherGeomBounds(g, xmin, ymin, xmax, ymax);
        }
    } else {
        const cJSON *coords = cJSON_GetObjectItemCaseSensitive(geom, "coordinates");
        if (coords) gatherCoords(coords, xmin, ymin, xmax, ymax);
    }
}

static void gatherNodeBounds(const cJSON *node,
                              double *xmin, double *ymin,
                              double *xmax, double *ymax)
{
    if (!node) return;
    const cJSON *typeItem = cJSON_GetObjectItemCaseSensitive(node, "type");
    if (!typeItem || !cJSON_IsString(typeItem)) return;
    const char *t = typeItem->valuestring;

    if (strcmp(t, "FeatureCollection") == 0) {
        const cJSON *features = cJSON_GetObjectItemCaseSensitive(node, "features");
        const cJSON *f = NULL;
        cJSON_ArrayForEach(f, features) {
            gatherNodeBounds(f, xmin, ymin, xmax, ymax);
        }
    } else if (strcmp(t, "Feature") == 0) {
        const cJSON *geom = cJSON_GetObjectItemCaseSensitive(node, "geometry");
        gatherGeomBounds(geom, xmin, ymin, xmax, ymax);
    } else {
        /* Bare geometry object */
        gatherGeomBounds(node, xmin, ymin, xmax, ymax);
    }
}

/* ── Drawing helpers ──────────────────────────────────────────────────────── */

/* Append one GeoJSON ring (array of [lon,lat] pairs) as a closed sub-path.
 * The caller owns the current CGPath and is responsible for drawing it. */
static void addRingToPath(const cJSON *ring, CGContextRef ctx)
{
    bool first = true;
    const cJSON *pt = NULL;
    cJSON_ArrayForEach(pt, ring) {
        const cJSON *xn = pt->child;
        if (!xn || !cJSON_IsNumber(xn)) { first = true; continue; }
        const cJSON *yn = xn->next;
        if (!yn || !cJSON_IsNumber(yn)) { first = true; continue; }
        double lon = xn->valuedouble, lat = yn->valuedouble;
        if (!isValidQuartzCoord(lon) || !isValidQuartzCoord(lat)) {
            first = true; continue;
        }
        if (first) {
            CGContextMoveToPoint(ctx, lon, lat);
            first = false;
        } else {
            CGContextAddLineToPoint(ctx, lon, lat);
        }
    }
    CGContextClosePath(ctx);
}

/* ── Per-geometry rendering ───────────────────────────────────────────────── */

static void drawGeometry(const cJSON *geom,
                          CGContextRef ctx,
                          double scale,
                          double circleR,
                          QLPreviewRequestRef preview,
                          QLThumbnailRequestRef thumbnail)
{
    if (!geom) return;
    const cJSON *typeItem = cJSON_GetObjectItemCaseSensitive(geom, "type");
    if (!typeItem || !cJSON_IsString(typeItem)) return;
    const char *t = typeItem->valuestring;
    const cJSON *coords = cJSON_GetObjectItemCaseSensitive(geom, "coordinates");

    if (strcmp(t, "Point") == 0) {
        if (!coords || cJSON_GetArraySize(coords) < 2) return;
        setColorForLines(ctx, scale);
        drawCircle(ctx,
                   cJSON_GetArrayItem(coords, 0)->valuedouble,
                   cJSON_GetArrayItem(coords, 1)->valuedouble,
                   circleR);

    } else if (strcmp(t, "MultiPoint") == 0) {
        setColorForLines(ctx, scale);
        const cJSON *pt = NULL;
        cJSON_ArrayForEach(pt, coords) {
            if (cJSON_GetArraySize(pt) < 2) continue;
            drawCircle(ctx,
                       cJSON_GetArrayItem(pt, 0)->valuedouble,
                       cJSON_GetArrayItem(pt, 1)->valuedouble,
                       circleR);
        }

    } else if (strcmp(t, "LineString") == 0) {
        setColorForLines(ctx, scale);
        CGContextBeginPath(ctx);
        bool first = true;
        const cJSON *pt = NULL;
        cJSON_ArrayForEach(pt, coords) {
            const cJSON *xn = pt->child;
            if (!xn || !cJSON_IsNumber(xn)) { first = true; continue; }
            const cJSON *yn = xn->next;
            if (!yn || !cJSON_IsNumber(yn)) { first = true; continue; }
            double lon = xn->valuedouble, lat = yn->valuedouble;
            if (!isValidQuartzCoord(lon) || !isValidQuartzCoord(lat)) {
                first = true; continue;
            }
            if (first) {
                CGContextMoveToPoint(ctx, lon, lat); first = false;
            } else {
                CGContextAddLineToPoint(ctx, lon, lat);
            }
        }
        CGContextStrokePath(ctx);

    } else if (strcmp(t, "MultiLineString") == 0) {
        setColorForLines(ctx, scale);
        const cJSON *line = NULL;
        cJSON_ArrayForEach(line, coords) {
            CGContextBeginPath(ctx);
            bool first = true;
            const cJSON *pt = NULL;
            cJSON_ArrayForEach(pt, line) {
                const cJSON *xn = pt->child;
                if (!xn || !cJSON_IsNumber(xn)) { first = true; continue; }
                const cJSON *yn = xn->next;
                if (!yn || !cJSON_IsNumber(yn)) { first = true; continue; }
                double lon = xn->valuedouble, lat = yn->valuedouble;
                if (!isValidQuartzCoord(lon) || !isValidQuartzCoord(lat)) {
                    first = true; continue;
                }
                if (first) {
                    CGContextMoveToPoint(ctx, lon, lat); first = false;
                } else {
                    CGContextAddLineToPoint(ctx, lon, lat);
                }
            }
            CGContextStrokePath(ctx);
        }

    } else if (strcmp(t, "Polygon") == 0) {
        /* coords = [ outerRing, holeRing, … ] */
        setColorForPolygons(ctx, scale);
        CGContextBeginPath(ctx);
        const cJSON *ring = NULL;
        cJSON_ArrayForEach(ring, coords) {
            addRingToPath(ring, ctx);
        }
        CGContextDrawPath(ctx, kCGPathEOFillStroke);

    } else if (strcmp(t, "MultiPolygon") == 0) {
        /* coords = [ polygon0, polygon1, … ] */
        setColorForPolygons(ctx, scale);
        const cJSON *poly = NULL;
        cJSON_ArrayForEach(poly, coords) {
            CGContextBeginPath(ctx);
            const cJSON *ring = NULL;
            cJSON_ArrayForEach(ring, poly) {
                addRingToPath(ring, ctx);
            }
            CGContextDrawPath(ctx, kCGPathEOFillStroke);
        }

    } else if (strcmp(t, "GeometryCollection") == 0) {
        const cJSON *geoms = cJSON_GetObjectItemCaseSensitive(geom, "geometries");
        const cJSON *g = NULL;
        cJSON_ArrayForEach(g, geoms) {
            drawGeometry(g, ctx, scale, circleR, preview, thumbnail);
        }
    }
}

/* Render all features in a node, counting shapes and checking cancellation. */
static bool renderNode(const cJSON *node,
                        CGContextRef ctx,
                        double scale,
                        double circleR,
                        QLPreviewRequestRef preview,
                        QLThumbnailRequestRef thumbnail,
                        int *count)
{
    if (!node) return true;
    const cJSON *typeItem = cJSON_GetObjectItemCaseSensitive(node, "type");
    if (!typeItem || !cJSON_IsString(typeItem)) return true;
    const char *t = typeItem->valuestring;

    if (strcmp(t, "FeatureCollection") == 0) {
        const cJSON *features = cJSON_GetObjectItemCaseSensitive(node, "features");
        const cJSON *f = NULL;
        cJSON_ArrayForEach(f, features) {
            if (!renderNode(f, ctx, scale, circleR, preview, thumbnail, count))
                return false;
        }
    } else if (strcmp(t, "Feature") == 0) {
        const cJSON *geom = cJSON_GetObjectItemCaseSensitive(node, "geometry");
        drawGeometry(geom, ctx, scale, circleR, preview, thumbnail);
        (*count)++;
        /* Cancellation check every 20 features (mirrors ESRIShape.c) */
        if (*count % 20 == 0) {
            if (preview   && QLPreviewRequestIsCancelled(preview))    return false;
            if (thumbnail && QLThumbnailRequestIsCancelled(thumbnail)) return false;
        }
    } else {
        /* Bare geometry at root */
        drawGeometry(node, ctx, scale, circleR, preview, thumbnail);
        (*count)++;
    }

    return true;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

bool readGeoJSONShapeSize(char *path,
                           double *x, double *y,
                           double *width, double *height,
                           QLPreviewRequestRef preview,
                           QLThumbnailRequestRef thumbnail)
{
    (void)preview; (void)thumbnail;

    char *buf = slurpFile(path);
    if (!buf) return false;

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return false;

    double xmin =  DBL_MAX, ymin =  DBL_MAX;
    double xmax = -DBL_MAX, ymax = -DBL_MAX;
    gatherNodeBounds(root, &xmin, &ymin, &xmax, &ymax);
    cJSON_Delete(root);

    if (xmin > xmax || ymin > ymax)             return false;
    if (!isfinite(xmin) || !isfinite(xmax) ||
        !isfinite(ymin) || !isfinite(ymax))     return false;

    *x      = xmin;
    *y      = ymin;
    *width  = xmax - xmin;
    *height = ymax - ymin;
    return true;
}

bool readGeoJSONShape(char *path,
                       double scale,
                       CGContextRef cgContext,
                       QLPreviewRequestRef preview,
                       QLThumbnailRequestRef thumbnail)
{
    char *buf = slurpFile(path);
    if (!buf) return false;

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return false;

    /* Use the feature-array length as the shape count for circleRadius(). */
    int nFeatures = 1;
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(root, "features");
    if (features) {
        nFeatures = cJSON_GetArraySize(features);
        if (nFeatures < 1) nFeatures = 1;
    }
    double r = circleRadius(nFeatures, scale);

    int count = 0;
    bool ok = renderNode(root, cgContext, scale, r, preview, thumbnail, &count);
    cJSON_Delete(root);

    return ok && count > 0;
}
