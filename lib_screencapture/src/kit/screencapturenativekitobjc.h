/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#if defined OS_SCREENCAPTUREKIT

#ifndef SCREENCAPTUREKIT_H_
#define SCREENCAPTUREKIT_H_

#include "../common/util.h"

struct ScreenCaptureInfo{
	int status; //0:NOT INIT; 1:INIT; 2:READY
	int monitor;
	int x;
	int y;
	int w;
	int h;
	RGB_IMAGE* rgbimage;
	CGDirectDisplayID displayID;
	void* kitInfo;
};

void DWAScreenCaptureKitLoad();
void DWAScreenCaptureKitUnload();
int DWAScreenCaptureKitStartCapture(ScreenCaptureInfo* sci);
void DWAScreenCaptureKitStopCapture(ScreenCaptureInfo* sci);
int DWAScreenCaptureKitGetImage(ScreenCaptureInfo* sci);


#endif /* SCREENCAPTUREKIT_H_ */
#endif
