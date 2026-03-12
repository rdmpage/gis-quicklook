#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include <os/log.h>
#include "ReadRaster.h"
#include "ReadVector.h"

/* -----------------------------------------------------------------------------
 Generate a preview for file
 
 This function's job is to create preview for designated file
 ----------------------------------------------------------------------------- */

OSStatus GeneratePreviewForURL(void *thisInterface,
							   QLPreviewRequestRef preview,
							   CFURLRef url,
							   CFStringRef contentTypeUTI,
							   CFDictionaryRef options)
{
	os_log(OS_LOG_DEFAULT, "GISLook preview: UTI=%{public}@", contentTypeUTI);

	bool vec = isVector(contentTypeUTI, url);
	os_log(OS_LOG_DEFAULT, "GISLook preview: isVector=%d", vec);

	if (vec) {
		bool readOk = readVector(preview, NULL, url, contentTypeUTI);
		os_log(OS_LOG_DEFAULT, "GISLook preview: readVector=%d", readOk);
		if (readOk) return noErr;
	}
	
	CGImageRef image = readRaster(preview, NULL, url, contentTypeUTI);
	if (image == NULL)
		return noErr;
	CGSize size = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
	// specify size in points (pass false for isBitmap). Finder will display 
	// previews at a larger size.
	CGContextRef cgContext = QLPreviewRequestCreateContext(preview, size, false, NULL);
	if(cgContext) {
		// draw the image
		CGRect crt = CGRectMake (0.f, 0.f, size.width, size.height);
		CGContextDrawImage (cgContext, crt, image);
		QLPreviewRequestFlushContext(preview, cgContext);
		CGImageRelease (image);
		CGContextRelease(cgContext);
	}
	
	return noErr;
	
}

void CancelPreviewGeneration(void* thisInterface, QLPreviewRequestRef preview)
{
    // implement only if supported
}
