/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#if defined OS_LINUX

#include "main.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <wchar.h>
#include <locale.h>
#include <sys/timeb.h>
#include <vector>
#include <map>

#define SYSTEM_TRAY_REQUEST_DOCK   0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

CallbackEventMessage g_callEventMessage;
JSONWriter jonextevent;
bool exitloop=false;
Display* display;
Visual* visual;
int screenid;
Screen* screen;
Window root;
XIM im;
XIMStyle best_style;
Atom wm_protocols;
Atom wm_delete_window;
int x11_fd;
fd_set in_fds;
struct timeval tv;
Colormap colormap;
std::map<FcChar32, FcChar32> charToFontHash;
std::map<FcChar32, XftFont*> hashToFont;

struct DWAWindow {
	int id;
	Window win;
	XIC ic;
	GC gc;
	unsigned long curcol;
	Pixmap dblbuffer;
	XftDraw* xtfdraw;
	XftColor* xtfcolor;
	int x;
	int y;
	int penWidth;
};
std::vector<DWAWindow*> windowList;

struct DWANotifyIcon {
	int id;
	wstring iconPath;
	Window win;
	GC gc;
	int w;
	int h;
};
std::vector<DWANotifyIcon*> notifyIconList;

struct DWAFont {
	int id;
	int type; //0=fontset; 1=xft
	XFontSet fontset;
	int fontascent;
	int fontheight;
};
std::vector<DWAFont*> fontList;

struct DWAImage {
	int id;
	ImageReader imageReader;
};
std::vector<DWAImage*> imageList;

DWAWindow* addWindow(int id, Window win,GC gc,XIC ic){
	DWAWindow* ww = new DWAWindow();
	ww->id=id;
	ww->ic=ic;
	ww->win=win;
	ww->gc=gc;
	ww->x=0;
	ww->y=0;
	ww->penWidth=1;
	ww->xtfdraw=NULL;
	ww->xtfcolor=NULL;
	windowList.push_back(ww);
	return ww;
}

void removeWindowByHandle(Window win){
	for (unsigned int i=0;i<windowList.size();i++){
		DWAWindow* app = windowList.at(i);
		if (app->win==win){
			if (app->xtfdraw!=NULL){
				XftDrawDestroy(app->xtfdraw);
				app->xtfdraw=NULL;
			}
			if (app->xtfcolor!=NULL){
				XftColorFree(display, visual, colormap, app->xtfcolor);
				delete app->xtfcolor;
				app->xtfcolor=NULL;
			}
			windowList.erase(windowList.begin()+i);
			delete app;
			break;
		}
	}
}

DWAWindow* getWindowByHandle(Window win){
	if (windowList.size()==0){
		return NULL;
	}
	for (unsigned int i=0;i<windowList.size();i++){
		DWAWindow* app = windowList.at(i);
		if (app->win==win){
			return app;
		}
	}
	return NULL;
}


DWAWindow* getWindowByID(int id){
	if (windowList.size()==0){
		return NULL;
	}
	for (unsigned int i=0;i<windowList.size();i++){
		DWAWindow* app = windowList.at(i);
		if (app->id==id){
			return app;
		}
	}
	return NULL;
}

DWANotifyIcon* addNotifyIcon(int id){
	DWANotifyIcon* ww = new DWANotifyIcon();
	ww->id=id;
	ww->w=0;
	ww->h=0;
	notifyIconList.push_back(ww);
	return ww;
}

DWANotifyIcon* getNotifyIconByID(int id){
	if (notifyIconList.size()==0){
		return NULL;
	}
	for (unsigned int i=0;i<=notifyIconList.size()-1;i++){
		if (notifyIconList.at(i)->id==id){
			return notifyIconList.at(i);
		}
	}
	return NULL;
}

DWANotifyIcon* getNotifyIconByHandle(Window win){
	if (notifyIconList.size()==0){
		return NULL;
	}
	for (unsigned int i=0;i<=notifyIconList.size()-1;i++){
		if (notifyIconList.at(i)->win==win){
			return notifyIconList.at(i);
		}
	}
	return NULL;
}

DWAFont* addFont(int id){
	DWAFont* ft = new DWAFont();
	ft->id=id;
	fontList.push_back(ft);
	return ft;
}

DWAFont* getFontByID(int id){
	if (fontList.size()==0){
		return NULL;
	}
	for (unsigned int i=0;i<fontList.size();i++){
		if (fontList.at(i)->id==id){
			return fontList.at(i);
		}
	}
	return NULL;
}

DWAImage* addImage(int id){
	DWAImage* im = new DWAImage();
	im->id=id;
	imageList.push_back(im);
	return im;
}

DWAImage* getImageByID(int id){
	if (imageList.size()==0){
		return NULL;
	}
	for (unsigned int i=0;i<imageList.size();i++){
		if (imageList.at(i)->id==id){
			return imageList.at(i);
		}
	}
	return NULL;
}

XIMStyle ChooseBetterStyle(XIMStyle style1,XIMStyle style2){
    XIMStyle s,t;
    XIMStyle preedit = XIMPreeditArea | XIMPreeditCallbacks |
        XIMPreeditPosition | XIMPreeditNothing | XIMPreeditNone;
    XIMStyle status = XIMStatusArea | XIMStatusCallbacks |
        XIMStatusNothing | XIMStatusNone;
    if (style1 == 0) return style2;
    if (style2 == 0) return style1;
    if ((style1 & (preedit | status)) == (style2 & (preedit | status)))
        return style1;
    s = style1 & preedit;
    t = style2 & preedit;
    if (s != t) {
        if (s | t | XIMPreeditCallbacks)
            return (s == XIMPreeditCallbacks)?style1:style2;
        else if (s | t | XIMPreeditPosition)
            return (s == XIMPreeditPosition)?style1:style2;
        else if (s | t | XIMPreeditArea)
            return (s == XIMPreeditArea)?style1:style2;
        else if (s | t | XIMPreeditNothing)
            return (s == XIMPreeditNothing)?style1:style2;
    }
    else { /* if preedit flags are the same, compare status flags */
        s = style1 & status;
        t = style2 & status;
        if (s | t | XIMStatusCallbacks)
            return (s == XIMStatusCallbacks)?style1:style2;
        else if (s | t | XIMStatusArea)
            return (s == XIMStatusArea)?style1:style2;
        else if (s | t | XIMStatusNothing)
            return (s == XIMStatusNothing)?style1:style2;
    }
}

int addImageToBuffer(wchar_t* file,unsigned long* &bf){
	int iret=0;
	ImageReader imgr;
	imgr.load(file);
	if (imgr.isLoaded()){
		iret=(2+(imgr.getWidth()*imgr.getHeight()));
		bf = (unsigned long*)malloc(iret*sizeof(unsigned long));
		int i=0;
		bf[i]=imgr.getWidth();
		i++;
		bf[i]=imgr.getHeight();
		i++;
		for (unsigned int x=0;x<=(unsigned int)imgr.getWidth()-1;x++){
			for (unsigned int y=0;y<=(unsigned int)imgr.getHeight()-1;y++){
				unsigned char r;
				unsigned char g;
				unsigned char b;
				unsigned char a;
				imgr.getPixel(x, y, &r, &g, &b, &a);
				bf[i] = a << 24 | r << 16 | g << 8 | b << 0;
				i++;
			}
		}
		imgr.destroy();
	}
	return iret;
}

void DWAGDIUnloadFont(int id){
	for (unsigned int i=0;i<fontList.size();i++){
		DWAFont* dwf = fontList.at(i);
		if (dwf->id==id){
			if (dwf->type==0){
				XFreeFontSet(display, dwf->fontset);
			}
			fontList.erase(fontList.begin()+i);
			delete dwf;
			break;
		}
	}

}

std::vector<FcChar32> wcharToUcs4(const wchar_t* wstr) {
   std::vector<FcChar32> result;
   if (!wstr) return result;
   size_t len = wcslen(wstr);
   for (size_t i = 0; i < len; i++) {
	   wchar_t wch = wstr[i];
	   //UTF-16
	   if (sizeof(wchar_t) == 2) {
		   if (wch >= 0xD800 && wch <= 0xDBFF && i + 1 < len) {
			   wchar_t low = wstr[i + 1];
			   if (low >= 0xDC00 && low <= 0xDFFF) {
				   FcChar32 codepoint = 0x10000 + ((wch & 0x3FF) << 10) + (low & 0x3FF);
				   result.push_back(codepoint);
				   i++;
				   continue;
			   }
		   }
		   else if (wch >= 0xDC00 && wch <= 0xDFFF) {
			   continue;
		   }
	   }
	   result.push_back(static_cast<FcChar32>(wch));
   }
   return result;
}

XftFont* findXtfFontbyChar(FcChar32 ucs4chars, int size=11) {

	std::map<FcChar32, FcChar32>::iterator charIt = charToFontHash.find(ucs4chars);
	if (charIt != charToFontHash.end()) {
	    FcChar32 hash = charIt->second;
	    return hashToFont[hash];
	}

	FcPattern* pattern = FcPatternCreate();
	FcPattern* matched;
	FcResult result;
	FcPatternAddString(pattern, FC_FAMILY, (const FcChar8*)"sans-serif");
	FcPatternAddInteger(pattern, FC_SIZE, size);
	FcPatternAddDouble(pattern, FC_DPI, 96.0);

	FcCharSet* charset = FcCharSetCreate();
	FcCharSetAddChar(charset, ucs4chars);
	FcPatternAddCharSet(pattern, FC_CHARSET, charset);

	FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
	FcPatternAddBool(pattern, FC_ANTIALIAS, FcTrue);
	FcPatternAddInteger(pattern, FC_HINT_STYLE, FC_HINT_FULL);

	FcConfigSubstitute(NULL, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);
	matched = FcFontMatch(NULL, pattern, &result);
	XftFont* font = NULL;
	if (matched) {
		FcChar32 hash = FcPatternHash(matched);
		std::map<FcChar32, XftFont*>::iterator fontIt = hashToFont.find(hash);
		if (fontIt != hashToFont.end()) {
			charToFontHash[ucs4chars] = hash;
			FcPatternDestroy(matched);
			font = fontIt->second;
		} else {
			font = XftFontOpenPattern(display, matched);
			if (font) {
				hashToFont[hash] = font;
				charToFontHash[ucs4chars] = hash;
			}else{
				FcPatternDestroy(matched);
			}
		}
	}
	FcCharSetDestroy(charset);
	FcPatternDestroy(pattern);
	return font;
}

bool loadFontType1(DWAFont* dwf, wchar_t* name){
	dwf->type=1;
	if (!XftInit(NULL)) {
		return false;
	}
	std::vector<FcChar32> ucs4chars = wcharToUcs4(name); //Can I use any text
	XftFont* font = findXtfFontbyChar(ucs4chars[0]);
	dwf->fontascent=font->ascent;
	dwf->fontheight=font->height;
	return true;
}


bool loadFontType0(DWAFont* dwf,wchar_t* name){
	dwf->type=0;
	int nmissing;
	char **missing;
	char *def_string;
	//fontset = XCreateFontSet(display, "-*-*-*-r-normal--14-*-*-*-P-*-*-*", &missing, &nmissing, &def_string);
	//fontset = XCreateFontSet(display, "-*-*-*-r-normal--*-120-100-100-*-*", &missing, &nmissing, &def_string);
	//fontset = XCreateFontSet(display, "fixed", &missing, &nmissing, &def_string);
	//fontset = XCreateFontSet(display, "-*-*-medium-r-normal--13-*-*-*-p-*-*-*", &missing, &nmissing, &def_string);
	//fontset = XCreateFontSet(display, "-*-*-medium-r-*--14-*-*-*-m-*-*-*", &missing, &nmissing, &def_string);
	dwf->fontset = XCreateFontSet(display, "-*-*-medium-*-*--13-*-*-*-*-*-*-*", &missing, &nmissing, &def_string);
	if (!dwf->fontset){
		dwf->fontset = XCreateFontSet(display, "-*-*-medium-*-*--12-*-*-*-*-*-*-*", &missing, &nmissing, &def_string);
	}
	if (!dwf->fontset){
		dwf->fontset = XCreateFontSet(display, "-*-*-*-*-*--13-*-*-*-*-*-*-*", &missing, &nmissing, &def_string);
	}
	if (!dwf->fontset){
		dwf->fontset = XCreateFontSet(display, "-*-*-*-*-*--12-*-*-*-*-*-*-*", &missing, &nmissing, &def_string);
	}
	if (!dwf->fontset){
		dwf->fontset = XCreateFontSet(display, "-*-*-medium-*-*--14-*-*-*-*-*-*-*", &missing, &nmissing, &def_string);
	}
	if (!dwf->fontset){
		dwf->fontset = XCreateFontSet(display, "-*-*-*-*-*--14-*-*-*-*-*-*-*", &missing, &nmissing, &def_string);
	}
	if (!dwf->fontset){
		dwf->fontset = XCreateFontSet(display, "-*-*-*-*-*--*-*-*-*-*-*-*-*", &missing, &nmissing, &def_string);
	}
	if (nmissing > 0) {
		/*for(int i=0; i < nmissing; i++){
			fprintf(stderr, "%s: %s\n", "program_name", missing[i]);
			fprintf(stderr, "%s: The string %s will be used in place\n","program_name", def_string);
		}*/
		XFreeStringList(missing);
	}
	//CALCOLA DIMENSIONI FONT
	XFontStruct **fonts;
	char **font_names;
	int nfonts;
	int j;
	dwf->fontascent = 0;
	dwf->fontheight = 0;
	nfonts = XFontsOfFontSet(dwf->fontset, &fonts, &font_names);
	for(j = 0; j < nfonts; j += 1){
		//fprintf(stderr, "%s: %s\n", "font name", font_names[j]);
		if (dwf->fontascent < fonts[j]->ascent) dwf->fontascent = fonts[j]->ascent;
		if (dwf->fontheight < fonts[j]->ascent+fonts[j]->descent) dwf->fontheight = fonts[j]->ascent+fonts[j]->descent;
	}
	return true;
}

void DWAGDILoadFont(int id, wchar_t* name){
	DWAFont* dwf = addFont(id);
	if (!loadFontType1(dwf,name)){
		loadFontType0(dwf,name);
	}
}

void DWAGDINewWindow(int id,int tp, int x, int y, int w, int h, wchar_t* iconPath){
	Window appwin;
	GC appgc;
	XIC appic;

	XSetWindowAttributes attributes;
	attributes.background_pixel = XWhitePixel(display,screenid);
	visual = DefaultVisual(display,screenid);
	int depth  = DefaultDepth(display,screenid);
	appwin = XCreateWindow(display,root,x, y, w, h, 0,depth,InputOutput,visual,CWBackPixel,&attributes);


	//PREVENT CLOSE BY X BUTTON
    XSetWMProtocols(display, appwin, &wm_delete_window, 1);

	appgc = XCreateGC(display, appwin, 0, 0);

	if ((tp==WINDOW_TYPE_NORMAL_NOT_RESIZABLE) || (tp==WINDOW_TYPE_DIALOG) || (tp==WINDOW_TYPE_TOOL)){
		XSizeHints *sh = XAllocSizeHints();
		sh->flags = PMinSize | PMaxSize;
		sh->min_width = sh->max_width = w;
		sh->min_height = sh->max_height = h;
		XSetWMNormalHints(display, appwin, sh);
		XFree(sh);
	}

	bool bhidetaskbar=false;
	if (tp==WINDOW_TYPE_TOOL){
		Atom key = XInternAtom(display, "_NET_WM_WINDOW_TYPE", True);
		Atom val= XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", True);
		XChangeProperty(display, appwin, key, XA_ATOM, 32, PropModeReplace, (unsigned char*)&val,  1);
		bhidetaskbar=true;
	}else if (tp==WINDOW_TYPE_DIALOG){
		Atom key = XInternAtom(display, "_NET_WM_WINDOW_TYPE", True);
		Atom val= XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", True);
		XChangeProperty(display, appwin, key, XA_ATOM, 32, PropModeReplace, (unsigned char*)&val,  1);
		bhidetaskbar=true;
	}else if (tp==WINDOW_TYPE_POPUP){
		Atom key = XInternAtom(display, "_NET_WM_WINDOW_TYPE", True);
		Atom val = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", True);
		XChangeProperty(display, appwin, key, XA_ATOM, 32, PropModeReplace, (unsigned char*)&val,  1);
		bhidetaskbar=true;
   	}

	if (bhidetaskbar){
		Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
		Atom states[4];
		states[0] = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
		states[1] = XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", False);
		states[2] = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
		states[3] = XInternAtom(display, "_NET_WM_STATE_STICKY", False);
		XChangeProperty(display, appwin, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char*)&states, 4);
	}

	//LOAD ICON
	if (iconPath != NULL){
		unsigned long *buffer = NULL;
		int length = 0;
		wchar_t wcs[wcslen(iconPath)];
		wcpcpy(wcs,iconPath);
		wchar_t *state;
		wchar_t *token = wcstok(wcs, L"\n", &state);
		while (token != NULL){
			unsigned long* appbf = NULL;
			int appln = addImageToBuffer(token,appbf);
			if (appln>0){
				if (length==0){
					buffer = (unsigned long*)malloc(appln*sizeof(unsigned long));
				}else{
					buffer = (unsigned long*)realloc(buffer,(length+appln)*sizeof(unsigned long));
				}
				memcpy(buffer+length, appbf, appln*sizeof(unsigned long));
				free(appbf);
				length+=appln;
			}
			token = wcstok(NULL, L"\n", &state);
		}
		if (length>0){
			Atom net_wm_icon = XInternAtom(display, "_NET_WM_ICON", False);
			Atom cardinal = XInternAtom(display, "CARDINAL", False);
			XChangeProperty(display, appwin, net_wm_icon, cardinal, 32, PropModeReplace, (const unsigned char*)buffer, length);
			free(buffer);
		}
	}

	/*
	DWAFont* dwf = fontList.at(0);
	XVaNestedList list = XVaCreateNestedList(0,XNFontSet,dwf->fontset,NULL);
	appic = XCreateIC(im,
				   XNInputStyle, best_style,
				   XNClientWindow, appwin,
				   XNPreeditAttributes, list,
				   XNStatusAttributes, list,
				   NULL);
	XFree(list);
	*/

	appic = XCreateIC(im,
				   XNInputStyle,
				   XIMPreeditNothing | XIMStatusNothing,
				   XNClientWindow, appwin,
				   XNFocusWindow, appwin,
				   NULL);


	long im_event_mask=0;
	if (appic != NULL) {
		XGetICValues(appic, XNFilterEvents, &im_event_mask, NULL);
		XSetICFocus(appic);
	}
	XSelectInput (display, appwin, ExposureMask | PointerMotionMask
				| ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask
				| FocusChangeMask | VisibilityChangeMask| im_event_mask);

	DWAWindow* dwa = addWindow(id, appwin,appgc, appic);
	XWindowAttributes wa;
	XGetWindowAttributes(display, appwin, &wa);
	dwa->dblbuffer = XCreatePixmap(display, appwin, wa.width, wa.height, wa.depth);
	dwa->x=x;
	dwa->y=y;
}

void DWAGDIPosSizeWindow(int id,int x, int y, int w, int h){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XFreePixmap(display, dwa->dblbuffer);
		if (dwa->xtfdraw!=NULL){
			XftDrawDestroy(dwa->xtfdraw);
			dwa->xtfdraw=NULL;
		}
		XWindowAttributes wa;
		XGetWindowAttributes(display, dwa->win, &wa);
		dwa->dblbuffer = XCreatePixmap(display, dwa->win, w, h, wa.depth);
		dwa->x=x;
		dwa->y=y;
		XMoveResizeWindow(display, dwa->win, x, y, w, h);
	}
}

void DWAGDIDestroyWindow(int id){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XIC xic=dwa->ic;
		Window ww=dwa->win;
		removeWindowByHandle(dwa->win);
		XDestroyIC(xic);
		XDestroyWindow(display,ww);
		XFreePixmap(display, dwa->dblbuffer);
		if (dwa->xtfdraw!=NULL){
			XftDrawDestroy(dwa->xtfdraw);
			dwa->xtfdraw=NULL;
		}
	}
}

void DWAGDISetTitle(int id, wchar_t* title){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XTextProperty prop;
		XwcTextListToTextProperty(display, &title, 1, XUTF8StringStyle, &prop);
		XSetWMName(display, dwa->win, &prop);
	}
}

void DWAGDIGetScreenSize(int* size){
	size[0]=screen->width;
	size[1]=screen->height;
}

void DWAGDIGetImageSize(wchar_t* fname, int* size){
	ImageReader imageReader;
	imageReader.load(fname);
	size[0]=imageReader.getWidth();
	size[1]=imageReader.getHeight();
}

void DWAGDIShow(int id,int mode){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XMapWindow(display, dwa->win);
		XMoveWindow(display,dwa->win,dwa->x, dwa->y);
		//XSync(display, false);
		//XSetInputFocus(display, dwa->win, RevertToParent, CurrentTime);
	}
}

void DWAGDIHide(int id){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XUnmapWindow(display, dwa->win);
	}
}

void DWAGDIToFront(int id){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XRaiseWindow(display, dwa->win);
	}
}

void DWAGDIPenColor(int id, int r, int g, int b){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		dwa->curcol = r << 16 | g << 8 | b << 0;
		if (dwa->xtfcolor!=NULL){
			XftColorFree(display, visual, colormap, dwa->xtfcolor);
			delete dwa->xtfcolor;
			dwa->xtfcolor=NULL;
		}
		XRenderColor render_color;
		render_color.red = (r << 8) | (unsigned char)r;
		render_color.green = (g << 8) | (unsigned char)g;
		render_color.blue = (b << 8) | (unsigned char)b;
		render_color.alpha = 0xffff;
		dwa->xtfcolor=new XftColor;
		if (!XftColorAllocValue(display, visual, colormap, &render_color, dwa->xtfcolor)) {
			delete dwa->xtfcolor;
			dwa->xtfcolor=NULL;
		}

	}
}

void DWAGDIPenWidth(int id, int w){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		dwa->penWidth=w;
	}
}

void DWAGDIDrawLine(int id, int x1,int y1,int x2,int y2){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XSetLineAttributes(display, dwa->gc, dwa->penWidth, LineSolid, CapButt, JoinMiter);
		XSetForeground(display,  dwa->gc, dwa->curcol);
		XDrawLine(display, dwa->dblbuffer, dwa->gc, x1, y1, x2, y2);
	}
}

void DWAGDIDrawEllipse(int id, int x, int y, int w,int h){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XSetLineAttributes(display, dwa->gc, dwa->penWidth, LineSolid, CapButt, JoinMiter);
		XSetForeground(display,  dwa->gc, dwa->curcol);
		XDrawArc(display, dwa->dblbuffer, dwa->gc, x, y, w, h, 0, 360*64);
	}
}

void DWAGDIFillEllipse(int id, int x, int y, int w,int h){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XSetLineAttributes(display, dwa->gc, dwa->penWidth, LineSolid, CapButt, JoinMiter);
		XSetForeground(display,  dwa->gc, dwa->curcol);
		XFillArc(display, dwa->dblbuffer, dwa->gc, x, y, w, h, 0, 360*64);
	}
}

void DWAGDIFillRectangle(int id, int x, int y, int w,int h){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XSetLineAttributes(display, dwa->gc, dwa->penWidth, LineSolid, CapButt, JoinMiter);
		XSetForeground(display,  dwa->gc, dwa->curcol);
		XFillRectangle(display, dwa->dblbuffer, dwa->gc, x, y, w, h);
	}
}

void DWAGDILoadImage(int id, wchar_t* fname, int* size){
	DWAImage* dwaim = addImage(id);
	dwaim->imageReader.load(fname);
	size[0]=dwaim->imageReader.getWidth();
	size[1]=dwaim->imageReader.getHeight();
}

void DWAGDIUnloadImage(int id){
	for (unsigned int i=0;i<imageList.size();i++){
		DWAImage* dwaim = imageList.at(i);
		if (dwaim->id==id){
			dwaim->imageReader.destroy();
			imageList.erase(imageList.begin()+i);
			delete dwaim;
			break;
		}
	}
}

void DWAGDIDrawImage(int id, int imgid, int x, int y){
	DWAWindow* dwa = getWindowByID(id);
	DWAImage* dwaim = getImageByID(imgid);
	if ((dwa!=NULL) && (dwaim!=NULL)){
		for (unsigned int cx=0;cx<=(unsigned int)dwaim->imageReader.getWidth()-1;cx++){
			for (unsigned int cy=0;cy<=(unsigned int)dwaim->imageReader.getHeight()-1;cy++){
				unsigned char r;
				unsigned char g;
				unsigned char b;
				unsigned char a;
				dwaim->imageReader.getPixel(cx, cy, &r, &g, &b, &a);
				if (a==255){
					unsigned long c = r << 16 | g << 8 | b << 0;
					XSetForeground(display, dwa->gc, c);
					XDrawPoint(display, dwa->dblbuffer, dwa->gc, x+cx, y+cy);
				}
			}
		}
	}
}

int getFontAscent(int id, int fntid){
	DWAFont* dwf = getFontByID(fntid);
	if (dwf!=NULL){
		return dwf->fontascent;
	}
	return 0;
}

int DWAGDIGetTextHeight(int id, int fntid){
	DWAFont* dwf = getFontByID(fntid);
	if (dwf!=NULL){
		return dwf->fontheight;
	}
	return 0;
}

int DWAGDIGetTextWidth(int id, int fntid, wchar_t* str){
	DWAFont* dwf = getFontByID(fntid);
	if (dwf!=NULL){
		if (dwf->type==0){
			return XwcTextEscapement(dwf->fontset,str,wcslen(str));
		}else{
			std::vector<FcChar32> ucs4chars = wcharToUcs4(str);
			if (ucs4chars.empty()) return 0;
			int totalWidth = 0;
			for (std::vector<FcChar32>::iterator it = ucs4chars.begin(); it != ucs4chars.end(); ++it) {
			    FcChar32 ch = *it;
				XftFont* font = findXtfFontbyChar(ch);
				if (font) {
					XGlyphInfo extents;
					XftTextExtents32(display, font, &ch, 1, &extents);
					totalWidth += extents.xOff;
				}
			}
			return totalWidth;
		}

	}else{
		return 0;
	}
}

void DWAGDIDrawText(int id, int fntid, wchar_t* str, int x, int y){
	DWAWindow* dwa = getWindowByID(id);
	DWAFont* dwf = getFontByID(fntid);
	if ((dwa!=NULL) && (dwf!=NULL)){
		if (dwf->type==0){
			XSetForeground(display,  dwa->gc, dwa->curcol);
			XwcDrawString(display,dwa->dblbuffer,dwf->fontset,dwa->gc,x,y+getFontAscent(id, fntid),str,wcslen(str));
		}else{
			if (dwa->xtfdraw==NULL){
				dwa->xtfdraw = XftDrawCreate(display, dwa->dblbuffer, visual, colormap);
			}
			if ((dwa->xtfdraw!=NULL) && (dwa->xtfcolor!=NULL)){
				std::vector<FcChar32> ucs4chars = wcharToUcs4(str);
				if (ucs4chars.empty()) return;
				int currentX = x;
				for (std::vector<FcChar32>::iterator it = ucs4chars.begin(); it != ucs4chars.end(); ++it) {
				    FcChar32 ch = *it;
					XftFont* font = findXtfFontbyChar(ch);
					if (font) {
						XftDrawString32(dwa->xtfdraw,dwa->xtfcolor,font,currentX,y+getFontAscent(id, fntid),&ch,1);
						XGlyphInfo extents;
						XftTextExtents32(display, font, &ch, 1, &extents);
						currentX += extents.xOff;
					} else {
						FcChar32 replacement = 0xFFFD;
						XftFont* fallbackFont = findXtfFontbyChar(replacement);
						if (fallbackFont) {
							XftDrawString32(dwa->xtfdraw,dwa->xtfcolor,font,currentX,y+getFontAscent(id, fntid),&ch,1);
							XGlyphInfo extents;
							XftTextExtents32(display, font, &replacement, 1, &extents);
							currentX += extents.xOff;
						}
					}
				}
			}
		}

	}
}

void DWAGDIGetMousePosition(int* pos){
	unsigned int mask_return;
	Window winr;
	int winx;
	int winy;
	int rootx;
	int rooty;
	if (XQueryPointer(display,root,&winr,&winr,&rootx,&rooty,&winx,&winy,&mask_return)==True){
		pos[0]=rootx;
		pos[1]=rooty;
	}
}

void DWAGDIRepaint(int id, int x, int y, int w, int h){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XEvent exppp;
		memset(&exppp, 0, sizeof(exppp));
		exppp.type = Expose;
		exppp.xexpose.window = dwa->win;
		exppp.xexpose.x=x;
		exppp.xexpose.y=y;
		exppp.xexpose.width=w;
		exppp.xexpose.height=h;
		XSendEvent(display,dwa->win,False,ExposureMask,&exppp);
	}
}

void DWAGDIClipRectangle(int id, int x, int y, int w, int h){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XRectangle curcliprect;
		curcliprect.x=x;
		curcliprect.y=y;
		curcliprect.width=w;
		curcliprect.height=h;
		XSetClipRectangles(display, dwa->gc, 0, 0, &curcliprect, 1, Unsorted);
	}
}

void DWAGDIClearClipRectangle(int id){
	DWAWindow* dwa = getWindowByID(id);
	if (dwa!=NULL){
		XSetClipMask(display, dwa->gc, None);
	}
}

void DWAGDISetClipboardText(wchar_t* str){

}

wchar_t* DWAGDIGetClipboardText(){
	return NULL;
}


void DWAGDICreateNotifyIcon(int id, wchar_t* iconPath, wchar_t* toolTip){
	DWANotifyIcon* dwanfi=addNotifyIcon(id);

	XVisualInfo vinfo;
	if (XMatchVisualInfo(display, screenid, 32, TrueColor, &vinfo)){
		XSetWindowAttributes attributes;
		attributes.colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
		attributes.border_pixel = 0;
		attributes.background_pixel = 0;
		dwanfi->win = XCreateWindow(display,root, -24, -24, 24, 24, 0,
								vinfo.depth,
								InputOutput,
								vinfo.visual,
								CWColormap | CWBorderPixel | CWBackPixel,
								&attributes);
	}else{
		XSetWindowAttributes attrs;
		attrs.background_pixel = 0x000000;
		attrs.event_mask = ButtonPressMask | ExposureMask;
		attrs.override_redirect = True;
		dwanfi->win = XCreateWindow(display,root,-24, -24, 24, 24, 0,
								DefaultDepth(display, screenid),
								InputOutput,
								DefaultVisual(display, screenid),
								CWBackPixel | CWEventMask | CWOverrideRedirect ,
								&attrs);
	}


	dwanfi->gc = XCreateGC(display, dwanfi->win, 0, 0);
	XSizeHints *sh = XAllocSizeHints();
	sh->flags = PPosition | PSize | PMinSize;
	sh->min_width = 24;
	sh->min_height = 24;
	XSetWMNormalHints(display, dwanfi->win, sh);
	XFree(sh);

	Atom atomInfo = XInternAtom(display, "_XEMBED_INFO", False);
	unsigned long xembedInfo[2];
	xembedInfo[0] = 0;
	xembedInfo[1] = 1;
	XChangeProperty(display, dwanfi->win, atomInfo, atomInfo, 32, PropModeReplace, (unsigned char*)xembedInfo, 2);
	XSelectInput (display, dwanfi->win, ExposureMask | PointerMotionMask | StructureNotifyMask
					| ButtonPressMask | ButtonReleaseMask | FocusChangeMask);

	char atomtn[128];
	sprintf(atomtn, "_NET_SYSTEM_TRAY_S%i", screenid);
	Atom satom = XInternAtom(display, atomtn, False);
	Window wtray = XGetSelectionOwner(display, satom);
	if (wtray != None){
		XSelectInput(display, wtray ,StructureNotifyMask);
	}
	XTextProperty prop;
	XwcTextListToTextProperty(display, &toolTip, 1, XUTF8StringStyle, &prop);
	XSetWMName(display, dwanfi->win, &prop);
	dwanfi->iconPath=wstring(iconPath);
	XEvent ev;
	memset(&ev, 0, sizeof(ev));
	ev.xclient.type = ClientMessage;
	ev.xclient.window = wtray;
	ev.xclient.message_type = XInternAtom(display, "_NET_SYSTEM_TRAY_OPCODE", False);
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = CurrentTime;
	ev.xclient.data.l[1] = SYSTEM_TRAY_REQUEST_DOCK;
	ev.xclient.data.l[2] = dwanfi->win;
	ev.xclient.data.l[3] = 0;
	ev.xclient.data.l[4] = 0;
	XSendEvent(display, wtray, False, NoEventMask, &ev);
}

void DWAGDIUpdateNotifyIcon(int id,wchar_t* iconPath,wchar_t* toolTip){
	DWANotifyIcon* dwanfi = getNotifyIconByID(id);
	if (dwanfi!=NULL){
		XTextProperty prop;
		XwcTextListToTextProperty(display, &toolTip, 1, XUTF8StringStyle, &prop);
		XSetWMName(display, dwanfi->win, &prop);
		dwanfi->iconPath=wstring(iconPath);
		if ((dwanfi->w>0) and (dwanfi->h>0)){
			XEvent exppp;
			memset(&exppp, 0, sizeof(exppp));
			exppp.type = Expose;
			exppp.xexpose.window = dwanfi->win;
			exppp.xexpose.x=0;
			exppp.xexpose.y=0;
			exppp.xexpose.width=dwanfi->w;
			exppp.xexpose.height=dwanfi->h;
			XSendEvent(display,dwanfi->win,False,ExposureMask,&exppp);
		}
	}
}

void DWAGDIDestroyNotifyIcon(int id){
	DWANotifyIcon* dwanfi = getNotifyIconByID(id);
	if (dwanfi!=NULL){
		XDestroyWindow(display,dwanfi->win);
		dwanfi->win=0;
		dwanfi->gc=NULL;
		dwanfi->w=0;
		dwanfi->h=0;
		dwanfi->iconPath=wstring();
	}
}

void drawNotify(DWANotifyIcon* dwanfi, int w, int h){
	XSetForeground(display, dwanfi->gc, 0x00000000);
	XFillRectangle(display, dwanfi->win, dwanfi->gc, 0, 0, w, h);
	ImageReader imgr;
	imgr.load(dwanfi->iconPath.c_str());
	if (imgr.isLoaded()){
		dwanfi->w=w;
		dwanfi->h=h;
		int offx=0;
		int offy=0;
		if (w>imgr.getWidth()){
			offx=(int)((w/2)-(imgr.getWidth()/2));
		}
		if (h>imgr.getHeight()){
			offy=(int)((h/2)-(imgr.getHeight()/2));
		}
		for (unsigned int x=0;x<=(unsigned int)imgr.getWidth()-1;x++){
			for (unsigned int y=0;y<=(unsigned int)imgr.getHeight()-1;y++){
				int dx=x+offx;
				int dy=y+offy;
				if ((dx<w) && (dy<h)){
					unsigned char r;
					unsigned char g;
					unsigned char b;
					unsigned char a;
					imgr.getPixel(x, y, &r, &g, &b, &a);
					unsigned long c = a << 24 | r << 16 | g << 8 | b << 0;
					if (a==255){
						XSetForeground(display, dwanfi->gc, c);
						XDrawPoint(display, dwanfi->win, dwanfi->gc, dx, dy);
					}
				}
			}
		}
		imgr.destroy();
	}
}


bool detectKeyType(DWAWindow* dwa,XEvent* e){
	bool bret=false;
	int len;
	int buf_len = 10;
	wchar_t *buffer = (wchar_t *)malloc(buf_len * sizeof(wchar_t));
	KeySym ks;
	Status status;
	len = XwcLookupString(dwa->ic, &e->xkey, buffer, buf_len,
									  &ks, &status);
	/*
	 * Workaround:  the Xsi implementation of XwcLookupString
	 * returns a length that is 4 times too big.  If this bug
	 * does not exist in your version of Xlib, remove the
	 * following line, and the similar line below.
	 */
	if (len>=4){
		len = len / 4;
	}
	if (status == XBufferOverflow) {
		buf_len = len;
		buffer = (wchar_t *)realloc((char *)buffer,
									buf_len * sizeof(wchar_t));
		len = XwcLookupString(dwa->ic, &e->xkey, buffer, buf_len,
							  &ks, &status);
		/* Workaround */
		if (len>=4){
			len = len / 4;
		}
	}
	wstring skey;
	switch (status) {
	case XLookupNone:
		break;
	case XLookupKeySym:
	case XLookupBoth:
		/* Handle backspacing, and <Return> to exit */
		if (ks==XK_Escape){
			skey.append(L"ESCAPE");
		}else if ((ks==XK_F1)|| (ks==XK_KP_F1)){
			skey.append(L"F1");
		}else if ((ks==XK_F2)|| (ks==XK_KP_F2)){
			skey.append(L"F2");
		}else if ((ks==XK_F3)|| (ks==XK_KP_F3)){
			skey.append(L"F3");
		}else if ((ks==XK_F4)|| (ks==XK_KP_F4)){
			skey.append(L"F4");
		}else if ((ks==XK_F5)){
			skey.append(L"F5");
		}else if ((ks==XK_F6)){
			skey.append(L"F6");
		}else if ((ks==XK_F7)){
			skey.append(L"F7");
		}else if ((ks==XK_F8)){
			skey.append(L"F8");
		}else if ((ks==XK_F9)){
			skey.append(L"F9");
		}else if ((ks==XK_F10)){
			skey.append(L"F10");
		}else if ((ks==XK_F11)){
			skey.append(L"F11");
		}else if ((ks==XK_F12)){
			skey.append(L"F12");
		}else if (ks==XK_Print){
			skey.append(L"PRINT");
		}else if (ks==XK_Scroll_Lock){
			skey.append(L"SCROLLOCK");
		}else if (ks==XK_Pause){
			skey.append(L"PAUSE");
		}else if (ks==XK_Break){
			skey.append(L"BREAK");
		}else if (ks==XK_BackSpace){
			skey.append(L"BACKSPACE");
		}else if ((ks==XK_Tab) || (ks==XK_KP_Tab) || ks==XK_ISO_Left_Tab){
			skey.append(L"TAB");
		}else if ((ks==XK_Return) || (ks==XK_KP_Enter)){
			skey.append(L"RETURN");
		}else if (ks==XK_Caps_Lock){
			skey.append(L"CAPSLOCK");
		}else if (ks==XK_Shift_Lock){
			skey.append(L"SHIFTLOCK");
		}else if ((ks==XK_Delete)|| (ks==XK_KP_Delete)){
			skey.append(L"DELETE");
		}else if ((ks==XK_Left)|| (ks==XK_KP_Left)){
			skey.append(L"LEFT");
		}else if ((ks==XK_Right)|| (ks==XK_KP_Right)){
			skey.append(L"RIGHT");
		}else if ((ks==XK_Up)|| (ks==XK_KP_Up)){
			skey.append(L"UP");
		}else if ((ks==XK_Down)|| (ks==XK_KP_Down)){
			skey.append(L"DOWN");
		}else if ((ks==XK_Home) || (ks==XK_KP_Home)){
			skey.append(L"HOME");
		}else if ((ks==XK_End) || (ks==XK_KP_End)){
			skey.append(L"END");
		}
		if (skey.length()>0){
			jonextevent.addString(L"type", L"KEY");
			jonextevent.addString(L"value", skey);
			bret=true;
			break;
		}
		if (status == XLookupKeySym) break;
	case XLookupChars:
		if (len>0){
			wchar_t appstr[len+1];
			int applen = 0;
			for(int i=0; i < len; i++){
				appstr[applen++] = buffer[i];
			}
			appstr[applen++]=L'\0';
			jonextevent.addString(L"type", L"CHAR");
			jonextevent.addString(L"value", appstr);
			bret=true;
		}
		break;
	}
	free(buffer);
	return bret;
}

int detectMouseButton(XEvent* e){
	int btn=0;
	if (e->xbutton.button == Button1) {
		btn=1;
	}else if (e->xbutton.button == Button3) {
		btn=2;
	}
	return btn;
}


void DWAGDILoop(CallbackEventMessage callback){
	g_callEventMessage=callback;
	setlocale(LC_ALL, getenv("LANG"));
	display = XOpenDisplay(NULL);
	if (! display) {
		fprintf(stderr, "Could not open display.\n");
	}

	wm_protocols = XInternAtom(display, "WM_PROTOCOLS", False);
	wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);

	XSetLocaleModifiers("");
	screenid = DefaultScreen(display);
	root = RootWindow(display, screenid);
	screen = XScreenOfDisplay(display, screenid);

	if ((im = XOpenIM(display, NULL, NULL, NULL)) == NULL) {
		fprintf(stderr, "Couldn't open input method");
	}
	XIMStyles *im_supported_styles;
	XIMStyle app_supported_styles;
	app_supported_styles = XIMPreeditNone | XIMPreeditNothing | XIMPreeditArea;
	app_supported_styles |= XIMStatusNone | XIMStatusNothing | XIMStatusArea;
	XGetIMValues(im, XNQueryInputStyle, &im_supported_styles, NULL);
	XIMStyle style;
	best_style = 0;
	for(int i=0; i < im_supported_styles->count_styles; i++) {
		style = im_supported_styles->supported_styles[i];
		if ((style & app_supported_styles) == style) /* if we can handle it */
			best_style = ChooseBetterStyle(style, best_style);
	}
	XFree(im_supported_styles);

	//CARICA COLOR MAP
	colormap = DefaultColormap(display, screenid);
	x11_fd = ConnectionNumber(display);

	g_callEventMessage(NULL);//INIT
	while (!exitloop){
		FD_ZERO(&in_fds);
		FD_SET(x11_fd, &in_fds);
		tv.tv_usec = 10*1000;
		tv.tv_sec = 0;
		select(x11_fd+1, &in_fds, 0, 0, &tv);
		if (XPending(display)){
			XEvent e;
			XNextEvent (display, &e);
			if (e.type == Expose){
				DWAWindow* dwa = getWindowByHandle(e.xany.window);
				if (dwa!=NULL){
					jonextevent.clear();
					jonextevent.beginObject();
					jonextevent.addString(L"name", L"REPAINT");
					jonextevent.addNumber(L"id", dwa->id);
					jonextevent.addNumber(L"x", e.xexpose.x);
					jonextevent.addNumber(L"y", e.xexpose.y);
					jonextevent.addNumber(L"width", e.xexpose.width);
					jonextevent.addNumber(L"height", e.xexpose.height);
					jonextevent.endObject();
					g_callEventMessage(jonextevent.getString().c_str());
					XCopyArea(display, dwa->dblbuffer, dwa->win, dwa->gc, e.xexpose.x, e.xexpose.y, e.xexpose.width, e.xexpose.height, e.xexpose.x, e.xexpose.y);
				}else{
					DWANotifyIcon* dwanfi = getNotifyIconByHandle(e.xany.window);
					if (dwanfi!=NULL){
						XWindowAttributes wa;
						XGetWindowAttributes(display, dwanfi->win, &wa);
						drawNotify(dwanfi,wa.width,wa.height);
					}
				}
			}else if (e.type == KeyPress){
				DWAWindow* dwa = getWindowByHandle(e.xany.window);
				if (dwa!=NULL){
					jonextevent.clear();
					jonextevent.beginObject();
					jonextevent.addString(L"name", L"KEYBOARD");
					jonextevent.addNumber(L"id", dwa->id);
					if (detectKeyType(dwa,&e)){
						jonextevent.addBoolean(L"shift", e.xkey.state & ShiftMask ? true : false);
						jonextevent.addBoolean(L"ctrl", e.xkey.state & ControlMask ? true : false);
						jonextevent.addBoolean(L"alt", false);
						jonextevent.addBoolean(L"command", false);
						jonextevent.endObject();
						g_callEventMessage(jonextevent.getString().c_str());
					}else{
						jonextevent.clear();
					}
				}
			}else if (e.type == MappingNotify) {
			   XRefreshKeyboardMapping(&e.xmapping);
			}else if (e.type == MotionNotify){
				DWAWindow* dwa = getWindowByHandle(e.xany.window);
				if (dwa!=NULL){
					jonextevent.clear();
					jonextevent.beginObject();
					jonextevent.addString(L"name", L"MOUSE");
					jonextevent.addString(L"action", L"MOVE");
					jonextevent.addNumber(L"id", dwa->id);
					jonextevent.addNumber(L"x", e.xmotion.x);
					jonextevent.addNumber(L"y", e.xmotion.y);
					jonextevent.addNumber(L"button", detectMouseButton(&e));
					jonextevent.endObject();
					g_callEventMessage(jonextevent.getString().c_str());
				}
			}else if (e.type == ButtonPress) {
				DWAWindow* dwa = getWindowByHandle(e.xany.window);
				if (dwa!=NULL){
					jonextevent.clear();
					jonextevent.beginObject();
					jonextevent.addString(L"name", L"MOUSE");
					jonextevent.addString(L"action", L"BUTTON_DOWN");
					jonextevent.addNumber(L"id", dwa->id);
					jonextevent.addNumber(L"x", e.xmotion.x);
					jonextevent.addNumber(L"y", e.xmotion.y);
					jonextevent.addNumber(L"button", detectMouseButton(&e));
					jonextevent.endObject();
					g_callEventMessage(jonextevent.getString().c_str());
				}
			}else if (e.type == ButtonRelease) {
				DWAWindow* dwa = getWindowByHandle(e.xany.window);
				if (dwa!=NULL){
					jonextevent.clear();
					jonextevent.beginObject();
					jonextevent.addString(L"name", L"MOUSE");
					jonextevent.addString(L"action", L"BUTTON_UP");
					jonextevent.addNumber(L"id", dwa->id);
					jonextevent.addNumber(L"x", e.xmotion.x);
					jonextevent.addNumber(L"y", e.xmotion.y);
					jonextevent.addNumber(L"button", detectMouseButton(&e));
					jonextevent.endObject();
					g_callEventMessage(jonextevent.getString().c_str());
				}else{
					DWANotifyIcon* dwanfi = getNotifyIconByHandle(e.xany.window);
					if (dwanfi!=NULL){
						if (e.xbutton.button == Button1) {
							jonextevent.clear();
							jonextevent.beginObject();
							jonextevent.addString(L"name", L"NOTIFY");
							jonextevent.addString(L"action", L"ACTIVATE");
							jonextevent.addNumber(L"id", dwanfi->id);
							jonextevent.endObject();
							g_callEventMessage(jonextevent.getString().c_str());
						}else if (e.xbutton.button == Button3) {
							jonextevent.clear();
							jonextevent.beginObject();
							jonextevent.addString(L"name", L"NOTIFY");
							jonextevent.addString(L"action", L"CONTEXTMENU");
							jonextevent.addNumber(L"id", dwanfi->id);
							jonextevent.endObject();
							g_callEventMessage(jonextevent.getString().c_str());
						}
					}
				}
			}else if (e.type == VisibilityNotify) {
				XSetInputFocus(display, e.xany.window, RevertToParent, CurrentTime);
			}else if (e.type == FocusIn) {
				DWAWindow* dwa = getWindowByHandle(e.xany.window);
				if (dwa!=NULL){
					jonextevent.clear();
					jonextevent.beginObject();
					jonextevent.addString(L"name", L"WINDOW");
					jonextevent.addString(L"action", L"ACTIVE");
					jonextevent.addNumber(L"id", dwa->id);
					jonextevent.endObject();
					g_callEventMessage(jonextevent.getString().c_str());
				}
			}else if (e.type == FocusOut) {
				DWAWindow* dwa = getWindowByHandle(e.xany.window);
				if (dwa!=NULL){
					jonextevent.clear();
					jonextevent.beginObject();
					jonextevent.addString(L"name", L"WINDOW");
					jonextevent.addString(L"action", L"INACTIVE");
					jonextevent.addNumber(L"id", dwa->id);
					jonextevent.endObject();
					g_callEventMessage(jonextevent.getString().c_str());
				}
			}else if (e.type == ClientMessage) {
				DWAWindow* dwa = getWindowByHandle(e.xany.window);
				if (dwa!=NULL){
					if (e.xclient.message_type == wm_protocols &&
							static_cast<Atom>(e.xclient.data.l[0]) == wm_delete_window)  {
						jonextevent.clear();
						jonextevent.beginObject();
						jonextevent.addString(L"name", L"WINDOW");
						jonextevent.addString(L"action", L"ONCLOSE");
						jonextevent.addNumber(L"id", dwa->id);
						jonextevent.endObject();
						g_callEventMessage(jonextevent.getString().c_str());
					}
				}
			}else if (e.type == ConfigureNotify) {
				DWANotifyIcon* dwanfi = getNotifyIconByHandle(e.xany.window);
				if (dwanfi!=NULL){
					drawNotify(dwanfi,e.xconfigure.width,e.xconfigure.height);
				}
			}
		}else{
			g_callEventMessage(NULL);
		}

	}
	//Destroy Fonts here because XFreeFontSet need of display
	unsigned int cnt=fontList.size();
	for (unsigned int i=0;i<cnt;i++){
		DWAFont* dwf = fontList.at(i);
		if (dwf->type==0){
			XFreeFontSet(display, dwf->fontset);
		}
		delete dwf;
	}
	for (unsigned int i=0;i<cnt;i++){
		fontList.erase(fontList.begin());
	}
	for (std::map<FcChar32, XftFont*>::iterator it = hashToFont.begin();
	     it != hashToFont.end(); ++it) {
	    if (it->second) {
	        XftFontClose(display, it->second);
	    }
	}
	charToFontHash.clear();
	hashToFont.clear();
	XFreeColormap(display, colormap);
	XCloseDisplay(display);
}

void DWAGDIEndLoop(){
	exitloop=true;
}

#endif
