/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef EXTERN_V4_H_
#define EXTERN_V4_H_

#include "../common/util.h"

/*
DIFFERENCE FROM V3:

CHANGED
//TODO Callbacks implementation

*/


typedef void (*DWAScreenCaptureOnMonitorsCallback)(MONITORS_INFO* moninfo);
DWAScreenCaptureOnMonitorsCallback dwaScreenCaptureOnMonitorsCallback=NULL;

typedef void (*DWAScreenCaptureOnRGBFrameCallback)(int idxmon, RGB_IMAGE* capimage);
DWAScreenCaptureOnRGBFrameCallback dwaScreenCaptureOnRGBFrameCallback=NULL;


extern "C" {
	int DWAScreenCaptureVersion(){
		return 4;
	}

	void DWAScreenCaptureSetOnMonitorsCallback(DWAScreenCaptureOnMonitorsCallback callback){
		dwaScreenCaptureOnMonitorsCallback=callback;
	}

	void DWAScreenCaptureSetOnRGBFrameCallback(DWAScreenCaptureOnRGBFrameCallback callback){
		dwaScreenCaptureOnRGBFrameCallback=callback;
	}


}

#endif /* EXTERN_V4_H_ */
