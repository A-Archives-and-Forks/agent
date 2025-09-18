/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef EXTERN_V2_H_
#define EXTERN_V2_H_

#include "../common/util.h"

/*
DIFFERENCE FROM V1:

REMOVED:
int DWAScreenCaptureGetClipboardText(wchar_t** wText);
void DWAScreenCaptureSetClipboardText(wchar_t* wText);

ADDED:
void DWAScreenCaptureGetClipboardChanges(CLIPBOARD_DATA* clipboardData);
void DWAScreenCaptureSetClipboard(CLIPBOARD_DATA* clipboardData);

*/

extern "C" {
	int DWAScreenCaptureVersion(){
		return 2;
	}
	bool DWAScreenCaptureLoad();
	void DWAScreenCaptureFreeMemory(void* pnt);
	int DWAScreenCaptureIsChanged();
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

#endif /* EXTERN_V2_H_ */
