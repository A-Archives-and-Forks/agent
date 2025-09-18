/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef EXTERN_V3_H_
#define EXTERN_V3_H_

#include "../common/util.h"

/*
DIFFERENCE FROM V2:

CHANGED
DWAScreenCaptureIsChanged() can return 3 = PERMISSION TOKEN; 4 = ERROR

ADDED
DWAScreenCaptureGetPermissionToken() message for DWAScreenCaptureIsChanged=3
DWAScreenCaptureSetPermissionToken()
DWAScreenCaptureErrorMessage() message for DWAScreenCaptureIsChanged=4
*/

/*
typedef void (*DWAScreenCaptureOnMonitorsCallback)(MONITORS_INFO* moninfo);
DWAScreenCaptureOnMonitorsCallback dwaScreenCaptureOnMonitorsCallback=NULL;

typedef void (*DWAScreenCaptureOnRGBFrameCallback)(int idxmon, RGB_IMAGE* capimage);
DWAScreenCaptureOnRGBFrameCallback dwaScreenCaptureOnRGBFrameCallback=NULL;
*/

extern "C" {
	int DWAScreenCaptureVersion(){
		return 3;
	}

	/*
	void DWAScreenCaptureSetOnMonitorsCallback(DWAScreenCaptureOnMonitorsCallback callback){
		dwaScreenCaptureOnMonitorsCallback=callback;
	}

	void DWAScreenCaptureSetOnRGBFrameCallback(DWAScreenCaptureOnRGBFrameCallback callback){
		dwaScreenCaptureOnRGBFrameCallback=callback;
	}
	*/

	bool DWAScreenCaptureLoad();
	void DWAScreenCaptureFreeMemory(void* pnt);
	int DWAScreenCaptureIsChanged();
	int DWAScreenCaptureErrorMessage(char* bf, int sz);
	int DWAScreenCaptureGetPermissionToken(char* bf, int sz);
	void DWAScreenCaptureSetPermissionToken(char* bf, int sz);
	int DWAScreenCaptureGetMonitorsInfo(MONITORS_INFO* moninfo);
	int DWAScreenCaptureInitMonitor(MONITORS_INFO_ITEM* moninfoitem, RGB_IMAGE* capimage, void** capses);
	int DWAScreenCaptureGetImage(void* capses);
	void DWAScreenCaptureTermMonitor(void* capses);
	void DWAScreenCaptureUnload();
	void DWAScreenCaptureInputKeyboard(const char* type, const char* key, bool ctrl, bool alt, bool shift, bool command);
	void DWAScreenCaptureInputMouse(MONITORS_INFO_ITEM* moninfoitem, int x, int y, int button, int wheel, bool ctrl, bool alt, bool shift, bool command);
	int DWAScreenCaptureCursor(CURSOR_IMAGE* curimage);
	void DWAScreenCaptureGetClipboardChanges(CLIPBOARD_DATA* clipboardData);
	void DWAScreenCaptureSetClipboard(CLIPBOARD_DATA* clipboardData);
	void DWAScreenCaptureCopy();
	void DWAScreenCapturePaste();
	int DWAScreenCaptureGetCpuUsage();
}

#endif /* EXTERN_V3_H_ */
