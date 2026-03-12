/*
 *  ReadVector.c
 *  GISLook
 *
 *  Created by Bernhard Jenny on 28.05.08.
 *  Copyright 2008. All rights reserved.
 *
 */

#include "ReadVector.h"
#include "ESRIShape.h"
#include "E00.h"

#define MAX_CONTEXT_SIZE 1000
#define MIN_CONTEXT_SIZE 10
#define BORDER 7

#define CIRCLE_RAD_1 6.
#define CIRCLE_RAD_2 3.
#define CIRCLE_RAD_3 1.5
#define CIRCLE_RAD_4 1.0
#define CIRCLE_LIMIT_1 25
#define CIRCLE_LIMIT_2 200
#define CIRCLE_LIMIT_3 1000

// Returns an effective UTI for vector dispatch. When the system-assigned UTI
// is not a recognised GIS type (e.g. macOS assigned a dynamic UTI because the
// UTImportedTypeDeclarations in the plugin bundle aren't registered with Launch
// Services), fall back to the file extension so previews still work.
// The returned string is either contentTypeUTI or a CFSTR literal — neither
// needs to be released by the caller.
static CFStringRef vectorUTIForURL(CFStringRef contentTypeUTI, CFURLRef url) {
	CFStringRef ESRI_SHAPE_UTI = CFSTR("com.esri.shape");
	CFStringRef E00_UTI        = CFSTR("com.esri.e00");
	CFStringRef COVERAGE_UTI   = CFSTR("com.esri.coverage");

	if (UTTypeConformsTo(contentTypeUTI, ESRI_SHAPE_UTI) ||
	    UTTypeConformsTo(contentTypeUTI, E00_UTI) ||
	    UTTypeConformsTo(contentTypeUTI, COVERAGE_UTI))
		return contentTypeUTI;

	// UTI not recognised — map from file extension
	CFStringRef result = contentTypeUTI;
	CFStringRef ext = CFURLCopyPathExtension(url);
	if (ext) {
		if (CFStringCompare(ext, CFSTR("shp"), kCFCompareCaseInsensitive) == kCFCompareEqualTo ||
		    CFStringCompare(ext, CFSTR("shx"), kCFCompareCaseInsensitive) == kCFCompareEqualTo ||
		    CFStringCompare(ext, CFSTR("dbf"), kCFCompareCaseInsensitive) == kCFCompareEqualTo ||
		    CFStringCompare(ext, CFSTR("sbx"), kCFCompareCaseInsensitive) == kCFCompareEqualTo ||
		    CFStringCompare(ext, CFSTR("sbn"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			result = ESRI_SHAPE_UTI;
		else if (CFStringCompare(ext, CFSTR("e00"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			result = E00_UTI;
		else if (CFStringCompare(ext, CFSTR("adf"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			result = COVERAGE_UTI;
		CFRelease(ext);
	}
	return result;
}

bool isVector(CFStringRef contentTypeUTI, CFURLRef url) {
	CFStringRef uti = vectorUTIForURL(contentTypeUTI, url);
	return UTTypeConformsTo(uti, CFSTR("com.esri.shape"))
	    || UTTypeConformsTo(uti, CFSTR("com.esri.e00"))
	    || UTTypeConformsTo(uti, CFSTR("com.esri.coverage"));
}

void setColorForPolygons(CGContextRef cgContext, double scale) {
	CGContextSetFillColorWithColor(cgContext, CGColorGetConstantColor(kCGColorBlack));
	CGContextSetStrokeColorWithColor(cgContext, CGColorGetConstantColor(kCGColorWhite));
	CGContextSetLineWidth(cgContext, LINE_WIDTH / scale / 2);
}

void setColorForLines(CGContextRef cgContext, double scale) {
	CGColorRef strokeColor = CGColorCreateGenericRGB(0, 0, 0, 1);
	CGContextSetFillColorWithColor(cgContext, strokeColor);
	CGContextSetStrokeColorWithColor(cgContext, strokeColor);
	CFRelease(strokeColor);
	CGContextSetLineWidth(cgContext, LINE_WIDTH / scale);
}

bool readVector(QLPreviewRequestRef preview,
				QLThumbnailRequestRef thumbnail,
				CFURLRef url,
				CFStringRef contentTypeUTI) {

	// Resolve dynamic UTIs to known GIS types by file extension
	contentTypeUTI = vectorUTIForURL(contentTypeUTI, url);

	CFStringRef ESRI_SHAPE_UTI = CFSTR("com.esri.shape");
	CFStringRef E00_UTI = CFSTR("com.esri.e00");
	CFStringRef COVERAGE_UTI = CFSTR("com.esri.coverage");
	
	double x = 0, y = 0, width = 0, height = 0;
	unsigned char path[10240];
	if (!CFURLGetFileSystemRepresentation(url, true, path, 10240))
		return FALSE;
	
	if (UTTypeConformsTo (contentTypeUTI, ESRI_SHAPE_UTI)) {
		if (!readESRIShapeSize((char*)path, &x, &y, &width, &height, preview, thumbnail))
			return FALSE;
	} else if (UTTypeConformsTo (contentTypeUTI, E00_UTI)
			   || UTTypeConformsTo (contentTypeUTI, COVERAGE_UTI)) {
		if (!readE00Size((char*)path, &x, &y, &width, &height, preview, thumbnail))
			return FALSE;
	}
	
	if (width < 0 || height < 0)
		return FALSE;
	
	// height or width can be zero (e.g. a single point or a single vertical line) 
	if (width == 0) {
		width = MIN_CONTEXT_SIZE;
		x -= MIN_CONTEXT_SIZE / 2;
	}
	if (height == 0) {
		height = MIN_CONTEXT_SIZE;
		y -= MIN_CONTEXT_SIZE / 2;
	}
	
	double sh = MAX_CONTEXT_SIZE / width;
	double sv = MAX_CONTEXT_SIZE / height;
	double scale = sh < sv ? sh : sv;
	width *= scale;
	height *= scale;
	if (width < MIN_CONTEXT_SIZE || height < MIN_CONTEXT_SIZE)
		return FALSE;
	CGSize size = CGSizeMake(width + 2 * BORDER, height + 2 * BORDER);
	CGContextRef cgContext = NULL;
	if (preview != NULL)
		cgContext = QLPreviewRequestCreateContext(preview, size, false, NULL);
	else if (thumbnail != NULL)
		cgContext = QLThumbnailRequestCreateContext(thumbnail, size, false, NULL);
	if(!cgContext)
		return FALSE;
	
	// draw background
	CGContextSetFillColorWithColor(cgContext, CGColorGetConstantColor(kCGColorWhite)); 
	CGRect rect = CGRectMake(0, 0, width + 2 * BORDER, height + 2 * BORDER);
	CGContextFillRect (cgContext, rect);
	
	CGContextScaleCTM(cgContext, scale, scale); 
	CGContextTranslateCTM (cgContext, -x + BORDER / scale, -y + BORDER / scale);
	
	bool res = FALSE;
	if (UTTypeConformsTo (contentTypeUTI, ESRI_SHAPE_UTI)) {
		res = readESRIShape((char*)path, scale, cgContext, preview, thumbnail);
	} else if (UTTypeConformsTo (contentTypeUTI, E00_UTI)
			   || UTTypeConformsTo (contentTypeUTI, COVERAGE_UTI)){
		res = readE00((char*)path, scale, cgContext, preview, thumbnail);
	}
	
	if (preview)
		QLPreviewRequestFlushContext(preview, cgContext);
	else
		QLThumbnailRequestFlushContext(thumbnail, cgContext);
	CFRelease(cgContext);
	
	return res;
}

void drawCircle(CGContextRef cgContext, double x, double y, double r) {
	double d = 2. * r;
	x -= r;
	y -= r;
	if (isValidQuartzCoord(x) && isValidQuartzCoord(y))
		CGContextFillEllipseInRect (cgContext, CGRectMake(x, y, d, d));
}

double circleRadius(int nCircles, double scale) {
	double r = CIRCLE_RAD_1;
	if (nCircles > CIRCLE_LIMIT_1)
		r = CIRCLE_RAD_2;
	if (nCircles > CIRCLE_LIMIT_2)
		r = CIRCLE_RAD_3;
	if (nCircles > CIRCLE_LIMIT_3)
		r = CIRCLE_RAD_4;
	r /= scale;
	return r;
}
