/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#if defined OS_QUARZDISPLAY

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
	void* objcInfo;
};

void DWAScreenCaptureDisplayStreamLoad();
void DWAScreenCaptureDisplayStreamUnload();
int DWAScreenCaptureDisplayStreamStartCapture(ScreenCaptureInfo* sci);
void DWAScreenCaptureDisplayStreamStopCapture(ScreenCaptureInfo* sci);
int DWAScreenCaptureDisplayStreamGetImage(ScreenCaptureInfo* sci);
bool isMacOSVersionAtLeast11();
bool isMacOSVersionAtLeast12();

#endif /* SCREENCAPTUREKIT_H_ */
#endif
