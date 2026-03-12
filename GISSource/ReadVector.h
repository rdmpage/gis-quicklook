/*
 *  ReadVector.h
 *  GISLook
 *
 *  Created by Bernhard Jenny on 28.05.08.
 *  Copyright 2008. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include <float.h>

#ifndef __READVECTOR__
#define __READVECTOR__

#define LINE_WIDTH 1.

bool isVector(CFStringRef contentTypeUTI, CFURLRef url);

void setColorForPolygons(CGContextRef cgContext, double scale);
void setColorForLines(CGContextRef cgContext, double scale);

bool readVector(QLPreviewRequestRef preview,
				QLThumbnailRequestRef thumbnail,
				CFURLRef url, 
				CFStringRef contentTypeUTI);

void drawCircle(CGContextRef cgContext, double x, double y, double r);

double circleRadius(int nCircles, double scale);

static inline bool isValidQuartzCoord(double c) {
	return c < FLT_MAX && c > -FLT_MAX;
}

#endif