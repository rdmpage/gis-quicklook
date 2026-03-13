/*
 *  ReadDBF.h
 *  GISLook
 *
 *  Renders previews and thumbnails for:
 *    .dbf  — dBASE IV attribute table (shown as a formatted column grid)
 *    .shx  — Shapefile spatial index  (shown as a brief info panel)
 */

#ifndef __READDBF__
#define __READDBF__

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include <stdbool.h>

/* Returns true if the URL refers to a .dbf or .shx file.
 * These are intercepted before the generic vector path so they get
 * purpose-built renderers instead of failing through SHPOpen. */
bool isDBF(CFStringRef contentTypeUTI, CFURLRef url);

/* Generates a preview or thumbnail for a .dbf or .shx file.
 * Exactly one of preview / thumbnail must be non-NULL.
 * Returns true on success. */
bool readDBF(QLPreviewRequestRef      preview,
             QLThumbnailRequestRef    thumbnail,
             CFURLRef                 url,
             CFStringRef              contentTypeUTI);

#endif /* __READDBF__ */
