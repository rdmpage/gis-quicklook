#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include <os/log.h>
#include "ReadRaster.h"
#include "ReadVector.h"
#include "ReadDBF.h"

/* -----------------------------------------------------------------------------
 Generate a thumbnail for file

 This function's job is to create thumbnail for designated file as fast as possible
 ----------------------------------------------------------------------------- */

OSStatus GenerateThumbnailForURL(void *thisInterface,
								 QLThumbnailRequestRef thumbnail,
								 CFURLRef url,
								 CFStringRef contentTypeUTI,
								 CFDictionaryRef options,
								 CGSize maxSize)
{
	os_log(OS_LOG_DEFAULT, "GISLook thumbnail: UTI=%{public}@", contentTypeUTI);

	if (isDBF(contentTypeUTI, url)) {
		bool readOk = readDBF(NULL, thumbnail, url, contentTypeUTI);
		os_log(OS_LOG_DEFAULT, "GISLook thumbnail: readDBF=%d", readOk);
		if (readOk) return noErr;
	}

	bool vec = isVector(contentTypeUTI, url);
	os_log(OS_LOG_DEFAULT, "GISLook thumbnail: isVector=%d", vec);

	if (vec) {
		bool readOk = readVector(NULL, thumbnail, url, contentTypeUTI);
		os_log(OS_LOG_DEFAULT, "GISLook thumbnail: readVector=%d", readOk);
		if (readOk) return noErr;
	}

	CGImageRef image = readRaster(NULL, thumbnail, url, contentTypeUTI);
	os_log(OS_LOG_DEFAULT, "GISLook thumbnail: readRaster=%p", image);
	if (image != NULL)
		QLThumbnailRequestSetImage(thumbnail, image, NULL);

	return noErr;

}

void CancelThumbnailGeneration(void* thisInterface, QLThumbnailRequestRef thumbnail)
{
    // implement only if supported
}
