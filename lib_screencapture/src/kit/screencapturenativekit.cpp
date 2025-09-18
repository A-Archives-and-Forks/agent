/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#if defined OS_SCREENCAPTUREKIT

#include "screencapturenativekit.h"

int DWAScreenCaptureVersion(){
	return 1;
}

void DWAScreenCaptureFreeMemory(void* pnt){
	free(pnt);
}

int DWAScreenCaptureIsChanged(){
	if (!CGPreflightScreenCaptureAccess()) {
		if (!reqperm){
			reqperm=true;
			CGRequestScreenCaptureAccess();
		}
		return 2;
	}
	return 0;
}

int DWAScreenCaptureInitMonitor(MONITORS_INFO_ITEM* moninfoitem, RGB_IMAGE* capimage, void** capses){
	ScreenCaptureInfo* sci = new ScreenCaptureInfo();
	sci->monitor=moninfoitem->index;
	sci->x=moninfoitem->x;
	sci->y=moninfoitem->y;
	sci->w=moninfoitem->width;
	sci->h=moninfoitem->height;
	MonitorInternalInfo* mi = (MonitorInternalInfo*)moninfoitem->internal;
	sci->displayID=mi->displayID;
	int iret = DWAScreenCaptureKitStartCapture(sci);
	if (iret!=0){
		return iret;
	}
	capimage->width=moninfoitem->width;
	capimage->height=moninfoitem->height;
	capimage->sizedata=capimage->width*capimage->height*3;
	capimage->sizechangearea=0;
	capimage->sizemovearea=0;
	capimage->data=(unsigned char*)malloc(capimage->sizedata * sizeof(unsigned char));
	sci->rgbimage=capimage;
	sci->status=1;
	*capses=sci;
	return 0;
}

void DWAScreenCaptureTermMonitor(void* capses){
	ScreenCaptureInfo* sci = (ScreenCaptureInfo*)capses;
	if (sci->status==0){
		return;
	}
	RGB_IMAGE* rgbimage = sci->rgbimage;
	free(rgbimage->data);
	rgbimage->data=NULL;
	rgbimage->width=0;
	rgbimage->height=0;
	sci->status=0;
	DWAScreenCaptureKitStopCapture(sci);
	delete sci;
}

int DWAScreenCaptureGetImage(void* capses){

	ScreenCaptureInfo* sci = (ScreenCaptureInfo*)capses;
	if (sci->status==0){
		return -1; //NOT INIT
	}
	DWAScreenCaptureKitGetImage(sci);
	sci->status=2;
	return 0;
}

int DWAScreenCaptureCursor(CURSOR_IMAGE* curimage){
	curimage->changed=0;
	if (curmoninfo!=NULL){
		CGEventRef event = CGEventCreate(NULL);
		if (event!=NULL){
			if (curdef==false){
				setCursorImage(CURSOR_TYPE_ARROW_18_18,curimage);
				curdef=true;
				curimage->changed=1;
			}
			/*
			MouseImageCapture mouseic = macobjcCaptureMouseImage(curoref);
			if (mouseic.status==0){
				curdef=false;
			}else if ((mouseic.status==2) || (mouseic.image == NULL)){
				if (curdef==false){
					if (curimage->data!=NULL){
						free(curimage->data);
					}
					setCursorImage(CURSOR_TYPE_ARROW_18_18,curimage);
					curdef=true;
					curimage->changed=1;
				}
				curoref=NULL;
			}else if (mouseic.status==1){
				//printf("CUR INFO %d %d %d %d\n",mouseic.w,mouseic.h,mouseic.offx,mouseic.offy);
				int cw = CGImageGetWidth(mouseic.image);
				int ch = CGImageGetHeight(mouseic.image);
				int bpp = CGImageGetBitsPerPixel(mouseic.image);
				int bpr = CGImageGetBytesPerRow(mouseic.image);
				CGDataProviderRef prov = CGImageGetDataProvider(mouseic.image);
				CFDataRef dataref = CGDataProviderCopyData(prov);
				unsigned char* data = (unsigned char*)CFDataGetBytePtr(dataref);
				curimage->width=cw;
				curimage->height=ch;
				curimage->offx=mouseic.offx;
				curimage->offy=mouseic.offy;
				curimage->sizedata=curimage->width*curimage->height*4;
				int sz=(cw * ch);
				unsigned char* cursorData = (unsigned char*)malloc(sz * 4);
				int offsetSrc = 0;
				int rowOffsetSrc = bpr % cw;
				int offsetDst = 0;
				for (int row = 0; row < ch; ++row){
					for (int col = 0; col < cw; ++col){
						unsigned char a=0;
						unsigned char r=0;
						unsigned char g=0;
						unsigned char b=0;
						if (bpp>24){
							a = data[offsetSrc+3];
							r = data[offsetSrc+2];
							g = data[offsetSrc+1];
							b = data[offsetSrc];
							offsetSrc += 4;
						}else if (bpp>16){
							r = data[offsetSrc+2];
							g = data[offsetSrc+1];
							b = data[offsetSrc];
							offsetSrc += 3;
						}
						cursorData[offsetDst]=r;
						cursorData[offsetDst+1]=g;
						cursorData[offsetDst+2]=b;
						cursorData[offsetDst+3]=a;
						offsetDst += 4;
					}
					offsetSrc += rowOffsetSrc;
				}
				if (curimage->data!=NULL){
					free(curimage->data);
				}
				curimage->data = cursorData;
				CFRelease(data);
				CGImageRelease(mouseic.image);
				curoref=mouseic.oref;
				curdef=false;
				curimage->changed=1;
			}
			*/
			curimage->visible=1;
			CGPoint cursor = CGEventGetLocation(event);
			for (int i=0;i<=curmoninfo->count-1;i++){
				MonitorInternalInfo* appmi = (MonitorInternalInfo*)curmoninfo->monitor[i].internal;
				int appx = appmi->x;
				int appy = appmi->y;
				int appw = appmi->w;
				int apph = appmi->h;
				if ((cursor.x>=appx) && (cursor.y>=appy) && (cursor.x<appx+appw) && (cursor.y<appy+apph)){
					curimage->x=curmoninfo->monitor[i].x+(int)((cursor.x-appx)*appmi->factx);
					curimage->y=curmoninfo->monitor[i].y+(int)((cursor.y-appy)*appmi->facty);
					break;
				}
			}
			CFRelease(event);
			return 0;
		}
	}
	return -1;
}

void DWAScreenCaptureInputKeyboard(const char* type, const char* key, bool ctrl, bool alt, bool shift, bool command){
	macInputs->keyboard(type, key, ctrl, alt, shift, command);
}

void DWAScreenCaptureInputMouse(MONITORS_INFO_ITEM* moninfoitem, int x, int y, int button, int wheel, bool ctrl, bool alt, bool shift, bool command){
	if ((x!=-1) && (y!=-1)){
		bool bok=false;
		if (moninfoitem!=NULL){
			MonitorInternalInfo* appmi = (MonitorInternalInfo*)moninfoitem->internal;
			x=appmi->x+(int)(((float)x)/appmi->factx);
			y=appmi->y+(int)(((float)y)/appmi->facty);
			bok=true;
		}else if (curmoninfo!=NULL){
			for (int i=0;i<=curmoninfo->count-1;i++){
				MonitorInternalInfo* appmi = (MonitorInternalInfo*)curmoninfo->monitor[i].internal;
				int appx = curmoninfo->monitor[i].x;
				int appy = curmoninfo->monitor[i].y;
				int appw = curmoninfo->monitor[i].width;
				int apph = curmoninfo->monitor[i].height;
				if ((x>=appx) && (y>=appy) && (x<appx+appw) && (y<appy+apph)){
					x=appmi->x+(int)(((float)x-appx)/appmi->factx);
					y=appmi->y+(int)(((float)y-appy)/appmi->facty);
					bok=true;
					break;
				}
			}
		}
		if (!bok){
			return;
		}
	}
	macInputs->mouse(x, y, button, wheel, ctrl, alt, shift, command);
}

void DWAScreenCaptureCopy(){
	macInputs->copy();
}

void DWAScreenCapturePaste(){
	macInputs->paste();
}

int DWAScreenCaptureGetClipboardText(wchar_t** wText){
	usleep(200000);
	return macobjcGetClipboardText(wText);
	return 0;
}

void DWAScreenCaptureSetClipboardText(wchar_t* wText){
	macobjcSetClipboardText(wText);
	usleep(200000);
}

//// TO DO 30/09/22 REMOVE ClipboardText
void DWAScreenCaptureGetClipboardChanges(CLIPBOARD_DATA* clipboardData){

}

void DWAScreenCaptureSetClipboard(CLIPBOARD_DATA* clipboardData){

}
////////////////////////////////////////////////

int DWAScreenCaptureGetCpuUsage(){
	return (int)cpuUsage->getValue();
}

int clearMonitorsInfo(MONITORS_INFO* moninfo){
	moninfo->changed=0;
	for (int i=0;i<=MONITORS_INFO_ITEM_MAX-1;i++){
		moninfo->monitor[i].changed=-1;
	}
	for (int i=0;i<=moninfo->count-1;i++){
		moninfo->monitor[i].changed=0;
	}
	int oldmc=moninfo->count;
	moninfo->count=0;
	return oldmc;
}

void addMonitorsInfo(MONITORS_INFO* moninfo, int mw, int mh, CGDirectDisplayID did, int x, int y, int w, int h, float factx, float facty){
	int p=moninfo->count;
	moninfo->count+=1;
	MonitorInternalInfo* mi = NULL;
	if (moninfo->monitor[p].internal==NULL){
		mi = new MonitorInternalInfo();
		moninfo->monitor[p].internal=mi;
	}else{
		mi = (MonitorInternalInfo*)moninfo->monitor[p].internal;
	}
	if (moninfo->monitor[p].changed==-1){
		moninfo->monitor[p].index=p;
		moninfo->monitor[p].width=mw;
		moninfo->monitor[p].height=mh;
		mi->displayID=did;
		mi->x=x;
		mi->y=y;
		mi->w=w;
		mi->h=h;
		mi->factx=factx;
		mi->facty=facty;
		moninfo->monitor[p].changed=1;
		moninfo->changed=1;
	}else{
		if ((mi->displayID!=did) ||	(mi->x!=x) || (mi->y!=y) || (mi->w!=w) || (mi->h!=h) || (mi->factx!=factx) || (mi->facty!=facty)){
			moninfo->monitor[p].index=p;
			moninfo->monitor[p].width=mw;
			moninfo->monitor[p].height=mh;
			mi->displayID=did;
			mi->x=x;
			mi->y=y;
			mi->w=w;
			mi->h=h;
			mi->factx=factx;
			mi->facty=facty;
			moninfo->monitor[p].changed=1;
			moninfo->changed=1;
		}else{
			moninfo->monitor[p].changed=0;
		}
	}
}


int DWAScreenCaptureGetMonitorsInfo(MONITORS_INFO* moninfo){
	int iret=0;
	bool bwakeup=false;
	curmoninfo=moninfo;
	int oldmc=clearMonitorsInfo(moninfo);
	if (oldmc<0){
		return oldmc;
	}
	moninfo->changed=0;
	CGDisplayCount numDisplays=0;
	CGGetOnlineDisplayList(0, 0, &numDisplays);
	CGDirectDisplayID display[numDisplays];
	CGDisplayErr err;
	err = CGGetOnlineDisplayList(numDisplays, display, &numDisplays);
	if (err == CGDisplayNoErr){
		for (CGDisplayCount i = 0; i < numDisplays; ++i) {
			if(CGDisplayMirrorsDisplay(display[i]) == kCGNullDirectDisplay){
				CGDirectDisplayID dspy = display[i];

				//wakeup monitor
				if ((bwakeup==false) && (CGDisplayIsAsleep(dspy))){
					bwakeup=true;
#if (MAC_OS_X_VERSION_MAX_ALLOWED < 120000)
					#define kIOMainPortDefault kIOMasterPortDefault
#endif
					io_registry_entry_t reg = IORegistryEntryFromPath(kIOMainPortDefault, "IOService:/IOResources/IODisplayWrangler");
					if (reg){
						IORegistryEntrySetCFProperty(reg, CFSTR("IORequestIdle"), kCFBooleanFalse);
					}
					IOObjectRelease(reg);
				}

				CGDisplayModeRef dismode=CGDisplayCopyDisplayMode(dspy);
				int modw = CGDisplayModeGetPixelWidth(dismode);
				int modh = CGDisplayModeGetPixelHeight(dismode);

				float factx=1.0;
				float facty=1.0;
				CGRect r = CGDisplayBounds(dspy);
				int ox=int(r.origin.x);
				int oy=int(r.origin.y);
				int sw=int(r.size.width);
				int sh=int(r.size.height);
				int mw=sw;
				int mh=sh;
				if (modw>sw){
					factx=(float)modw/(float)sw;
					mw=modw;
				}
				if (modh>sh){
					facty=(float)modh/(float)sh;
					mh=modh;
				}
				//printf("CGDirectDisplayID %d %d %d %d %d %f %f\n",dspy,ox,oy,sw,sh,factx,facty);
				addMonitorsInfo(moninfo,mw,mh,dspy,ox,oy,sw,sh,factx,facty);
			}
		}
	}
	if (oldmc!=moninfo->count){
		moninfo->changed=1;
	}
	//FIX X Y SCALED
	if (moninfo->changed==1){
		bool bchanged=true;
		while (bchanged){
			bchanged=false;
			for (int i=0;i<=moninfo->count-1;i++){
				MonitorInternalInfo* cmi = (MonitorInternalInfo*)moninfo->monitor[i].internal;
				int cx = cmi->x;
				int cy = cmi->y;
				int sx = cx;
				int sy = cy;
				for (int j=0;j<=moninfo->count-1;j++){
					if (i!=j){
						MonitorInternalInfo* appmi = (MonitorInternalInfo*)moninfo->monitor[j].internal;
						int appx = appmi->x;
						int appy = appmi->y;
						int appw = appmi->w;
						int apph = appmi->h;
						if (cx==appx+appw){
							sx=(int)(((float)(appx+appw))*appmi->factx);
						}
						if (cy==appy+apph){
							sy=(int)(((float)(appy+apph))*appmi->facty);
						}
					}
				}
				if ((moninfo->monitor[i].x!=sx) || (moninfo->monitor[i].y!=sy)){
					moninfo->monitor[i].x=sx;
					moninfo->monitor[i].y=sy;
					bchanged=true;
				}
			}
		}
	}

	//RELOAD KEYMAP IF NEED
	macInputs->reloadKeyMap();

	return iret;
}

bool DWAScreenCaptureLoad(){
	cpuUsage=new MacCPUUsage();
	macInputs=new MacInputs();
	curdef=false;
	reqperm=false;
	curoref=NULL;
	curmoninfo=NULL;
	CFStringRef reasonForActivity=CFSTR("dwagent keep awake");
	successIOPM1 = kIOReturnError;
	successIOPM1 = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, reasonForActivity, &assertionIDIOPM1);
	successIOPM2 = kIOReturnError;
	successIOPM2 = IOPMAssertionCreateWithName(CFSTR("UserIsActive"), kIOPMAssertionLevelOn, reasonForActivity, &assertionIDIOPM2);
	DWAScreenCaptureKitLoad();
	return true;
}

void DWAScreenCaptureUnload(){
	DWAScreenCaptureKitUnload();
	delete cpuUsage;
	delete macInputs;
	if(successIOPM1 == kIOReturnSuccess) {
		IOPMAssertionRelease(assertionIDIOPM1);
		successIOPM1 = kIOReturnError;
	}
	if(successIOPM2 == kIOReturnSuccess) {
		IOPMAssertionRelease(assertionIDIOPM2);
		successIOPM2 = kIOReturnError;
	}
}


#endif
