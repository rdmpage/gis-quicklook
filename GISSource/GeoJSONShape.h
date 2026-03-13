/*
 *  GeoJSONShape.h
 *  GISLook
 *
 *  Renders previews and thumbnails for GeoJSON (.geojson) files as map images,
 *  using the same visual style as the ESRI shapefile renderer.
 */

#ifndef __GEOJSONSHAPE__
#define __GEOJSONSHAPE__

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include <stdbool.h>

/* Compute the bounding box of all geometries in the file.
 * Returns false if the file cannot be read, parsed, or contains no coordinates. */
bool readGeoJSONShapeSize(char *path,
                           double *x, double *y,
                           double *width, double *height,
                           QLPreviewRequestRef preview,
                           QLThumbnailRequestRef thumbnail);

/* Render all geometries into cgContext, which has already been scaled and
 * translated by readVector() so that data coordinates map directly to canvas
 * coordinates.  Returns false if nothing was drawn. */
bool readGeoJSONShape(char *path,
                       double scale,
                       CGContextRef cgContext,
                       QLPreviewRequestRef preview,
                       QLThumbnailRequestRef thumbnail);

#endif /* __GEOJSONSHAPE__ */
