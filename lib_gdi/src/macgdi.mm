/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#if defined OS_MAC

#include <wchar.h>
#include "jsonwriter.h"
#include "imagereader.h"

#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>



NSAutoreleasePool *pool=NULL;
NSApplication *app=NULL;
NSTimer *timer=NULL;
NSMutableArray *windowList=NULL;
NSMutableArray *notifyIconList=NULL;
NSMutableArray *imageList=NULL;
JSONWriter jonextevent;
NSInteger appActPolicy=NSApplicationActivationPolicyRegular;

//const int WINDOW_TYPE_NORMAL=0;
//const int WINDOW_TYPE_NORMAL_NOT_RESIZABLE=1;
const int WINDOW_TYPE_DIALOG=100;
const int WINDOW_TYPE_POPUP=200;
const int WINDOW_TYPE_TOOL=300;

typedef void (*CallbackEventMessage)(const wchar_t* msg);

CallbackEventMessage g_callEventMessage=NULL;

void callEventMessage(wstring msg){
	if (![NSThread isMainThread]){
		dispatch_async(dispatch_get_main_queue(), ^{		
			g_callEventMessage(msg.c_str());
		});
	}else{
		g_callEventMessage(msg.c_str());
	}
}

void loopEventMessage() {
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		if(g_callEventMessage!=NULL){
			g_callEventMessage(NULL);
			loopEventMessage();
		}
	});
}


NSString* wchartToNString(wchar_t* inStr){
	if (NSHostByteOrder() == NS_LittleEndian){
		return [[NSString alloc] initWithBytes:inStr length:wcslen(inStr)*sizeof(*inStr) encoding:NSUTF32LittleEndianStringEncoding];
	}else{
		return [[NSString alloc] initWithBytes:inStr length:wcslen(inStr)*sizeof(*inStr) encoding:NSUTF32BigEndianStringEncoding];
	}
}

wchar_t* nStringTowchart(NSString* inStr){
	if (NSHostByteOrder() == NS_LittleEndian){
	    return (wchar_t *)[inStr cStringUsingEncoding:NSUTF32LittleEndianStringEncoding];
	}else{
		return (wchar_t *)[inStr cStringUsingEncoding:NSUTF32BigEndianStringEncoding];
	}
}


void fireCallBackRepaint(int id, int x,int y,int w, int h){
	jonextevent.clear();
	jonextevent.beginObject();
	jonextevent.addString(L"name", L"REPAINT");
	jonextevent.addNumber(L"id", id);
	jonextevent.addNumber(L"x", x);
	jonextevent.addNumber(L"y", y);
	jonextevent.addNumber(L"width", w);
	jonextevent.addNumber(L"height", h);
	jonextevent.endObject();
	callEventMessage(jonextevent.getString());	
}

void fireCallBackKeyboard(int id,const wchar_t* type,const wchar_t* val,bool bshift){
	jonextevent.clear();
	jonextevent.beginObject();
	jonextevent.addString(L"name", L"KEYBOARD");
	jonextevent.addNumber(L"id", id);
	jonextevent.addString(L"type", type);
	jonextevent.addString(L"value", val);
	jonextevent.addBoolean(L"shift", bshift);
	jonextevent.addBoolean(L"ctrl", false);
	jonextevent.addBoolean(L"alt", false);
	jonextevent.addBoolean(L"command", false);
	jonextevent.endObject();
	callEventMessage(jonextevent.getString());
}

void fireCallBackMouse(int id,const wchar_t* action, int x, int y, int button){
	jonextevent.clear();
	jonextevent.beginObject();
	jonextevent.addString(L"name", L"MOUSE");
	jonextevent.addString(L"action", action);
	jonextevent.addNumber(L"id", id);
	jonextevent.addNumber(L"x", x);
	jonextevent.addNumber(L"y", y);
	jonextevent.addNumber(L"button", button);
	jonextevent.endObject();
	callEventMessage(jonextevent.getString());
}

void fireCallBackWindow(int id, const wchar_t* action){
	jonextevent.clear();
	jonextevent.beginObject();
	jonextevent.addString(L"name", L"WINDOW");
	jonextevent.addString(L"action", action);
	jonextevent.addNumber(L"id", id);
	jonextevent.endObject();
	callEventMessage(jonextevent.getString());
}


@interface DWADelegate : NSObject <NSApplicationDelegate>{
}
@end

@implementation DWADelegate


- (void) applicationDidFinishLaunching:(NSNotification *)aNotification{
	//FIND OTHER SOLUTION
	/*dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		NSImage *imgDock = [[NSImage alloc] initWithContentsOfFile:@"images/logo.icns"];
		if (imgDock != nil){
			if ([imgDock isValid]) {
				[NSApp setApplicationIconImage: imgDock];
			}
			//[imgDock release];
		}
	});*/
	

	
	//FIND OTHER SOLUTION
	NSImage *imgDock = [[NSImage alloc] initWithContentsOfFile:@"images/logo.icns"];
	if ((imgDock != nil) && ([imgDock isValid])){
		[[NSApplication sharedApplication] setApplicationIconImage:imgDock];
	}	
		
}

@end

@interface NSEXWindow:NSWindow
{

}
@end

@implementation NSEXWindow
-(id)init
{
   self = [super init];
   return self;
}

- (BOOL) canBecomeKeyWindow { return true; }
- (BOOL) canBecomeMainWindow { return true; }
@end

@interface DWAWindow:NSObject
{
	NSWindow *win;
	NSColor *penColor;
	int wid;
	int x;
	int y;
	int h;
	int w;
	bool boolClipRect;
}
@end

@implementation DWAWindow
-(id)init
{
   self = [super init];
   penColor=[NSColor blackColor];
   //boolInit=false;
   boolClipRect=false;
   return self;
}

- (void) setWin : (NSWindow*) p {  win = p; }
- (NSWindow*) win { return win; }

- (void) setPenColor : (NSColor*) p {  penColor = p; }
- (NSColor*) penColor { return penColor; }

- (void) setWid : (int) p {  wid = p; }
- (int) wid { return wid; }

- (void) setX : (int) p {  x = p; }
- (int) x { return x; }

- (void) setY : (int) p {  y = p; }
- (int) y { return y; }

- (void) setW : (int) p {  w = p; }
- (int) w { return w; }

- (void) setH : (int) p {  h = p; }
- (int) h { return h; }

- (void) setBoolClipRect : (bool) p {  boolClipRect = p; }
- (bool) boolClipRect { return boolClipRect; }

@end

DWAWindow* addWindow(NSWindow* win, int wid){
	DWAWindow *ww = [[DWAWindow alloc]init];
	[ww setWin:win];
	[ww setWid:wid];
	[windowList addObject:ww];
	return ww;
}

DWAWindow* getWindowByID(int wid){
	unsigned int i=0;
	for (i=0;i<[windowList count];i++){
		DWAWindow *ww = [windowList objectAtIndex:i];
		if ([ww wid]==wid){
			return ww;
		}
	}
	return NULL;
}

void removeWindowByID(int wid){
	unsigned int i=0;
	for (i=0;i<[windowList count];i++){
		DWAWindow *ww = [windowList objectAtIndex:i];
		if ([ww wid]==wid){
			[windowList removeObject:ww];
			return;
		}
	}

}

@interface NotifyIconView : NSControl {
    NSImage *image;
    int wid;
}
@end

@implementation NotifyIconView

- (void) setImage : (NSImage*) p {  image = p; }
- (NSImage*) image { return image; }

- (void) setWid : (int) p {  wid = p; }
- (int) wid { return wid; }

- (void)mouseDown:(NSEvent *)theEvent{
	jonextevent.clear();
	jonextevent.beginObject();
	jonextevent.addString(L"name", L"NOTIFY");
	jonextevent.addString(L"action", L"ACTIVATE");
	jonextevent.addNumber(L"id", wid);
	jonextevent.endObject();
	callEventMessage(jonextevent.getString());
}
- (void)rightMouseDown:(NSEvent *)theEvent{
	jonextevent.clear();
	jonextevent.beginObject();
	jonextevent.addString(L"name", L"NOTIFY");
	jonextevent.addString(L"action", L"CONTEXTMENU");
	jonextevent.addNumber(L"id", wid);
	jonextevent.endObject();
	callEventMessage(jonextevent.getString());
}
- (void)dealloc {
    self.image = nil;
    [super dealloc];
}
- (void)drawRect:(NSRect)rect {
	NSRect imageRect = NSMakeRect((CGFloat)round(rect.size.width*0.5f-16*0.5f),
								  (CGFloat)round(rect.size.height*0.5f-16*0.5f),
								  16,16);
	[self.image drawInRect:imageRect fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0f];
}
@end

@interface DWANotifyIcon:NSObject
{
	NSStatusItem *statusItem;
	int wid;
}
@end

@implementation DWANotifyIcon
-(id)init
{
   self = [super init];
   return self;
}

- (void) setStatusItem : (NSStatusItem*) p {  statusItem = p; }
- (NSStatusItem*) statusItem { return statusItem; }

- (void) setWid : (int) p {  wid = p; }
- (int) wid { return wid; }

@end

DWANotifyIcon* addNotifyIcon(int wid){
	DWANotifyIcon *ww = [[DWANotifyIcon alloc]init];
	[ww setWid:wid];
	[notifyIconList addObject:ww];
	return ww;
}

DWANotifyIcon* getNotifyIconByID(int wid){
	unsigned int i=0;
	for (i=0;i<[notifyIconList count];i++){
		DWANotifyIcon *ww = [notifyIconList objectAtIndex:i];
		if ([ww wid]==wid){
			return ww;
		}
	}
	return NULL;
}

@interface DWAImage:NSObject
{
	NSImage *image;
	int wid;
}
@end

@implementation DWAImage
-(id)init
{
   self = [super init];
   return self;
}

- (void) setImage : (NSImage*) p {  image = p; }
- (NSImage*) image { return image; }

- (void) setWid : (int) p {  wid = p; }
- (int) wid { return wid; }
@end

DWAImage* addImage(int wid){
	DWAImage *ww = [[DWAImage alloc]init];
	[ww setWid:wid];
	[imageList addObject:ww];
	return ww;
}

DWAImage* getImageByID(int wid){
	unsigned int i=0;
	for (i=0;i<[imageList count];i++){
		DWAImage *ww = [imageList objectAtIndex:i];
		if ([ww wid]==wid){
			return ww;
		}
	}
	return NULL;
}

@interface GView : NSView <NSWindowDelegate> {
	int wid;
	int h;
	int w;
	//bool bfocus;
	bool bshow;
	bool bdestroy;
}
-(void)drawRect:(NSRect)rect;
@end

@implementation GView
-(id)init
{
   self = [super init];   
   //bfocus=false;
   self->bshow=false;
   self->bdestroy=false;
   return self;
}

- (void) setWid : (int) p {  wid = p; }
- (int) wid { return wid; }

- (void) setW : (int) p {  w = p; }
- (int) w { return w; }

- (void) setH : (int) p {  h = p; }
- (int) h { return h; }

-(void)drawRect:(NSRect)rect {
	[[NSColor whiteColor] set];
	NSRect irect = NSIntersectionRect([self bounds], rect);
	NSRectFill(irect);
	fireCallBackRepaint(wid,irect.origin.x,[self bounds].size.height-irect.origin.y-irect.size.height,irect.size.width,irect.size.height);
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent {
	return YES;
}

- (void)keyDown:(NSEvent *)theEvent {
	//NSLog(@"keyDown");
	NSUInteger keyCode = theEvent.keyCode;
	NSUInteger modifierFlags = theEvent.modifierFlags;
	bool bshift=false;
	if (modifierFlags & NSEventModifierFlagShift) {
		bshift=true;
	}
	if (keyCode == 126) {
		fireCallBackKeyboard(wid,L"KEY",L"UP",bshift);
	} else if (keyCode == 125) {
		fireCallBackKeyboard(wid,L"KEY",L"DOWN",bshift);
	} else if (keyCode == 123) {
		fireCallBackKeyboard(wid,L"KEY",L"LEFT",bshift);
	} else if (keyCode == 124) {
		fireCallBackKeyboard(wid,L"KEY",L"RIGHT",bshift);
	} else if (keyCode == 51) {
		fireCallBackKeyboard(wid,L"KEY",L"BACKSPACE",bshift);
	} else if (keyCode == 117) {
		fireCallBackKeyboard(wid,L"KEY",L"DELETE",bshift);
	} else if (keyCode == 36) {
		fireCallBackKeyboard(wid,L"KEY",L"RETURN",bshift);
	} else if (keyCode == 48) {
		fireCallBackKeyboard(wid,L"KEY",L"TAB",bshift);
	} else if (keyCode == 122) {
		fireCallBackKeyboard(wid,L"KEY",L"F1",bshift);
	} else if (keyCode == 120) {
		fireCallBackKeyboard(wid,L"KEY",L"F2",bshift);
	} else if (keyCode == 99) {
		fireCallBackKeyboard(wid,L"KEY",L"F3",bshift);
	} else if (keyCode == 118) {
		fireCallBackKeyboard(wid,L"KEY",L"F4",bshift);
	} else if (keyCode == 96) {
		fireCallBackKeyboard(wid,L"KEY",L"F5",bshift);
	} else if (keyCode == 97) {
		fireCallBackKeyboard(wid,L"KEY",L"F6",bshift);
	} else if (keyCode == 98) {
		fireCallBackKeyboard(wid,L"KEY",L"F7",bshift);
	} else if (keyCode == 100) {
		fireCallBackKeyboard(wid,L"KEY",L"F8",bshift);
	} else if (keyCode == 101) {
		fireCallBackKeyboard(wid,L"KEY",L"F9",bshift);
	} else if (keyCode == 109) {
		fireCallBackKeyboard(wid,L"KEY",L"F10",bshift);
	} else if (keyCode == 103) {
		fireCallBackKeyboard(wid,L"KEY",L"F11",bshift);
	} else if (keyCode == 111) {
		fireCallBackKeyboard(wid,L"KEY",L"F12",bshift);
	} else if (keyCode == 116) {
		//Page Up key pressed
	} else if (keyCode == 121) {
		//Page Down key pressed
	} else if (keyCode == 115) {
		//Home key pressed
	} else if (keyCode == 119) {
		//End key pressed
	} else if (keyCode == 118) {
		//??
	} else if (keyCode == 114) {
		//Insert key pressed
	} else if (keyCode == 105) {
		//Stamp key pressed
	}else{	
		NSString *characters = theEvent.characters;
		unichar firstChar = [characters characterAtIndex:0];	
		if (isprint(firstChar)){			
			//NSLog(@"KeyCode %d",keyCode);			
			wchar_t *wideChar = (wchar_t *)malloc(2 * sizeof(wchar_t));
			wideChar[0] = (wchar_t)firstChar;
			wideChar[1] = L'\0';
			fireCallBackKeyboard(wid,L"CHAR",wideChar,false);
			free(wideChar);
		}
	}
}


/*

- (void)keyDown:(NSEvent *)theEvent {
	[self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
}
 
- (void)insertText:(id)string{
    [super insertText:string];
    int n = [string length];
    int i=0;
    for (i=0;i<n;i++){
    	NSString* c = [string substringWithRange:NSMakeRange(i, 1)];
    	//NSLog(c);
    	wchar_t* buffer=nStringTowchart(c);
    	fireCallBackKeyboard(wid,L"CHAR",buffer,false);
    }
}


-(IBAction)insertTab:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"TAB",false);
}

-(IBAction)insertBacktab:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"TAB",true);
}

-(IBAction)deleteBackward:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"BACKSPACE",false);
}

-(IBAction)deleteForward:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"DELETE",false);
}

-(IBAction)moveLeft:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"LEFT",false);
}

-(IBAction)moveLeftAndModifySelection:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"LEFT",true);
}

-(IBAction)moveRight:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"RIGHT",false);
}

-(IBAction)moveRightAndModifySelection:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"RIGHT",true);
}

-(IBAction)moveToBeginningOfLine:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"HOME",false);
}

-(IBAction)moveToBeginningOfLineAndModifySelection:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"HOME",true);
}

-(IBAction)moveToEndOfLine:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"END",false);
}

-(IBAction)moveToEndOfLineAndModifySelection:(id)sender{
	fireCallBackKeyboard(wid,L"KEY",L"END",true);
}
*/

- (void)mouseMoved:(NSEvent *)theEvent {
	NSPoint p = [theEvent locationInWindow];
	float x=(int)p.x;
	float y=(int)(h-p.y);
	if ((x>0) && (y>0) && (x<=w) && (y<=h)) {
		fireCallBackMouse(wid,L"MOVE",x,y,0);
		//NSLog(@"mouseMove x: %f y: %f",x,y);
	}
}

- (void)mouseDown:(NSEvent *)theEvent {
	NSPoint p = [theEvent locationInWindow];
	float x=(int)p.x;
	float y=(int)(h-p.y);
	if ((x>0) && (y>0) && (x<=w) && (y<=h)) {
		fireCallBackMouse(wid,L"BUTTON_DOWN",x,y,1);
		//NSLog(@"mouseDown x: %f y: %f",x,y);
	}
}

- (void)mouseUp:(NSEvent *)theEvent {
	NSPoint p = [theEvent locationInWindow];
	float x=(int)p.x;
	float y=(int)(h-p.y);
	if ((x>0) && (y>0) && (x<=w) && (y<=h)) {
		fireCallBackMouse(wid,L"BUTTON_UP",x,y,1);
		//NSLog(@"mouseUp x: %f y: %f",x,y);
	}
}

- (BOOL)windowShouldClose:(NSNotification *)note {
	fireCallBackWindow(wid,L"ONCLOSE");
	return false;
}

- (void)windowWillClose:(NSNotification *)note {
	removeWindowByID(wid);
	if ([windowList count]==0){
		[app stop:self];
		[app terminate: self];
	}
}


- (BOOL)canBecomeKeyWindow {
    return YES;
}

- (BOOL)canBecomeMainWindow {
    return YES;
}


-(void)windowDidBecomeMain:(NSNotification *)note {
	if (!self->bdestroy){
		dispatch_async(dispatch_get_main_queue(), ^{
			if (self.window.isVisible && self->bshow==false){
				[self.window orderOut:nil];
			}
		});
	}
}

-(void)windowDidResignMain:(NSNotification *)note {

}

-(void)windowDidBecomeKey:(NSNotification *)note {
	[self gotFocus];
}

-(void)windowDidResignKey:(NSNotification *)note {
	[self lostFocus];
}

-(void)gotFocus{
	if (!self->bdestroy){
		//NSLog(@"GOT FOCUS %d",wid);
		fireCallBackWindow(wid,L"ACTIVE");
	}
}

-(void)lostFocus{
	if (!self->bdestroy){
		//NSLog(@"LOST FOCUS %d",wid);
		fireCallBackWindow(wid,L"INACTIVE");
	}
}

-(void)showWindow{
	self->bshow=true;
	dispatch_async(dispatch_get_main_queue(), ^{
		[self.window makeKeyAndOrderFront:nil];
	});
}

-(void)toFrontWindow{	
	self->bshow=true;
	dispatch_async(dispatch_get_main_queue(), ^{
		[self.window makeKeyAndOrderFront:nil];		
		[[NSRunningApplication currentApplication] activateWithOptions:(NSApplicationActivateAllWindows | NSApplicationActivateIgnoringOtherApps)];
	});
}

-(void)hideWindow{
	self->bshow=false;
	[self.window orderOut:nil];
}
		
-(void)destroyWindow{
	self->bdestroy=true;
	[self.window close];
}

@end

/*
@interface DWATimer:NSObject{

}
@end

@implementation DWATimer
-(id)init
{
   return self;
}

-(void)onTick:(NSTimer*)timer{
	callEventMessage(NULL);
}
@end
*/

extern "C" void DWAGDINSAppSetActivationPolicy(int v){
	if (v==1){
		appActPolicy=NSApplicationActivationPolicyAccessory;
	}else if (v==2){
		appActPolicy=NSApplicationActivationPolicyProhibited;
	}else{
		appActPolicy=NSApplicationActivationPolicyRegular;
	}
}


extern "C" void DWAGDILoop(CallbackEventMessage callback){
	windowList = [[NSMutableArray alloc] init];
	notifyIconList = [[NSMutableArray alloc] init];
	imageList = [[NSMutableArray alloc] init];

	if (app==NULL){
		pool = [NSAutoreleasePool new];

		//[NSApp setPoolProtected:NO];
		//[NSApp setDelegate: [DWADelegate new]];
		app = [NSApplication sharedApplication];
		[NSApp setDelegate:[[DWADelegate alloc] init]];

		/*timer = [NSTimer scheduledTimerWithTimeInterval: 0.1
				 target: [DWATimer new]
				 selector: @selector(onTick:)
				 userInfo: nil
				 repeats: YES];*/
		

		//[NSApp setActivationPolicy:appActPolicy];		
		NSApplicationActivationPolicy policy = (NSApplicationActivationPolicy)appActPolicy;
		[NSApp setActivationPolicy:policy];

		
		[NSApp activateIgnoringOtherApps:YES];

		id menubar = [[NSMenu new] autorelease];
		id appMenuItem = [[NSMenuItem new] autorelease];
		[menubar addItem:appMenuItem];
		[NSApp setMainMenu:menubar];
		id appMenu = [[NSMenu new] autorelease];
		//id appName = [[NSProcessInfo processInfo] processName];
		//id quitTitle = [@"Quit " stringByAppendingString:appName];
		id quitTitle = [@"Quit " stringByAppendingString:@""];
		id quitMenuItem = [[[NSMenuItem alloc] initWithTitle:quitTitle
		action:@selector(terminate:) keyEquivalent:@"q"] autorelease];
		[appMenu addItem:quitMenuItem];
		[appMenuItem setSubmenu:appMenu];
	}

	g_callEventMessage=callback;
	g_callEventMessage(NULL);
	loopEventMessage();
	
	[app run];
	
	if (![NSThread isMainThread]){
		dispatch_sync(dispatch_get_main_queue(), ^{		
			g_callEventMessage=NULL;
		});
	}else{
		g_callEventMessage=NULL;
	}
	[pool release];
}

extern "C" void DWAGDIEndLoop(){
	
}

extern "C" void DWAGDICreateNotifyIcon(int wid,wchar_t* iconPath,wchar_t* toolTip){
	DWANotifyIcon* dwa = addNotifyIcon(wid);
	NSStatusBar *statusBar = [NSStatusBar systemStatusBar];
	NSStatusItem *statusItem = [statusBar statusItemWithLength:NSSquareStatusItemLength];
	NSImage *image = [[NSImage alloc] initWithContentsOfFile:wchartToNString(iconPath)];
	NotifyIconView *view = [NotifyIconView new];
	[view setWid: wid];
	[view setImage: image];
	[statusItem setView:view];
	[statusItem setToolTip:wchartToNString(toolTip)];
	[view release];
	[dwa setStatusItem:statusItem];
	[statusItem retain];
}

extern "C" void DWAGDIUpdateNotifyIcon(int wid,wchar_t* iconPath,wchar_t* toolTip){
	DWANotifyIcon* dwa = getNotifyIconByID(wid);
	if (dwa!=NULL){
		NSStatusItem* statusItem = [dwa statusItem];
		NSImage *iconold =[statusItem image];
		[iconold release];
		NSImage *image = [[NSImage alloc] initWithContentsOfFile:wchartToNString(iconPath)];
		NotifyIconView *view = [NotifyIconView new];
		[view setWid: wid];
		[view setImage: image];
		[statusItem setView:view];
		[statusItem setToolTip:wchartToNString(toolTip)];
		[view release];
	}
}

extern "C" void DWAGDIDestroyNotifyIcon(int wid){
	unsigned int i=0;
	for (i=0;i<[notifyIconList count];i++){
		DWANotifyIcon *dwa = [notifyIconList objectAtIndex:i];
		if ([dwa wid]==wid){
			NSStatusItem* statusItem = [dwa statusItem];
			NSImage *imageold =[statusItem image];
			[imageold release];
			[statusItem release];
			[notifyIconList removeObject:dwa];
			return;
		}
	}
}

extern "C" void DWAGDILoadFont(int wid,wchar_t* name){

}

extern "C" void DWAGDIUnloadFont(int wid){

}

extern "C" void DWAGDILoadImage(int wid, wchar_t* fname, int* size){
	DWAImage* dwa = addImage(wid);
	NSImage *image = [[NSImage alloc] initWithContentsOfFile:wchartToNString(fname)];
	[dwa setWid:wid];
	[dwa setImage:image];
	NSImageRep* bitmapImageRep = [[image representations] objectAtIndex:0];
	size[0] = bitmapImageRep.pixelsWide;
	size[1] = bitmapImageRep.pixelsHigh;
}

extern "C" void DWAGDIUnloadImage(int wid){
	unsigned int i=0;
	for (i=0;i<[imageList count];i++){
		DWAImage *dwa = [imageList objectAtIndex:i];
		if ([dwa wid]==wid){
			NSImage *imageold =[dwa image];
			[imageold release];
			[imageList removeObject:dwa];
			return;
		}
	}
}

extern "C" void DWAGDIDrawImage(int wid, int imgid, int x, int y){
	DWAWindow* dwa = getWindowByID(wid);
	DWAImage* dwaimg = getImageByID(imgid);
	if ((dwa!=NULL) && (dwaimg!=NULL)){
		NSImage *image =[dwaimg image];
		NSImageRep* bitmapImageRep = [[image representations] objectAtIndex:0];
		int w=bitmapImageRep.pixelsWide;
		int h=bitmapImageRep.pixelsHigh;
		NSRect imageRect = NSMakeRect(x,dwa.h-h-y,w,h);
		[image drawInRect:imageRect fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0f];
	}
}

extern "C" void DWAGDINewWindow(int wid, int tp,int x, int y, int w, int h, wchar_t* iconPath){
	NSRect sr = [[NSScreen mainScreen] frame];
	NSRect svr = [[NSScreen mainScreen] visibleFrame];
	NSRect frame = NSMakeRect(x, svr.size.height-y+(sr.size.height-svr.size.height)-y, w, h);

	int sm=NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask;
	if (tp==WINDOW_TYPE_TOOL){
		sm=NSTitledWindowMask;
	}else if (tp==WINDOW_TYPE_DIALOG){
		sm=NSTitledWindowMask;
	}else if (tp==WINDOW_TYPE_POPUP){
		sm=NSWindowStyleMaskBorderless;
	}

	NSEXWindow *window = [[NSEXWindow alloc]
		initWithContentRect:frame
				  styleMask:sm
					backing:NSBackingStoreBuffered
					  defer:false];

	if (tp==WINDOW_TYPE_POPUP){
		[window setLevel: NSPopUpMenuWindowLevel];
	}
	
	
	DWAWindow* dwa=addWindow(window, wid);
	[dwa setX:x];
	[dwa setY:y];
	[dwa setW:w];
	[dwa setH:h];
	
	
	//dwa.boolInit=true;
	GView *view = [[[GView alloc] initWithFrame:frame] autorelease];
	[view setWid:wid];
	[view setW:[dwa w]];
	[view setH:[dwa h]];	
	[window setContentView:view];
	[window setDelegate:view];
	[window setAcceptsMouseMovedEvents:YES];
	[window contentView]; //FORCE LOADING
	[window orderOut:nil];	
	[window setOrderedIndex:0];
}

extern "C" void DWAGDIPosSizeWindow(int wid,int x, int y, int w, int h){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		NSWindow *window = [dwa win];
		[dwa setX:x];
		[dwa setY:y];
		[dwa setW:w];
		[dwa setH:h];
		//if (dwa.boolInit){
		GView *view = [window contentView];
		[view setW:w];
		[view setH:h];
		//}
		NSRect sr = [[NSScreen mainScreen] frame];
		NSRect svr = [[NSScreen mainScreen] visibleFrame];
		NSPoint pos;
		pos.x = x;
		pos.y = svr.size.height-h+(sr.size.height-svr.size.height)-y;
		[window setFrameOrigin:pos];
		NSSize sz;
		sz.width=w;
		sz.height=h;
		[window setContentSize:sz];
	}
}


extern "C" void DWAGDIDestroyWindow(int wid){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		NSWindow *window = [dwa win];
		GView *view = [window contentView];
		[view destroyWindow];
		//[window close];
	}
}

extern "C" void DWAGDISetTitle(int wid, wchar_t* title){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		NSWindow *window = [dwa win];
		NSString* nsTitle = wchartToNString(title);
		[window setTitle:nsTitle];
	}
}

extern "C" void DWAGDIShow(int wid,int mode){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		NSWindow *window = [dwa win];
		GView *view = [window contentView];
		[view showWindow];
	}
}

extern "C" void DWAGDIHide(int wid){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		NSWindow *window = [dwa win];
		GView *view = [window contentView];
		[view hideWindow];
	}
}

extern "C" void DWAGDIToFront(int wid){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		NSWindow *window = [dwa win];
		GView *view = [window contentView];
		[view toFrontWindow];
	}
}

extern "C" void DWAGDIPenColor(int wid, int r, int g, int b){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		[dwa setPenColor:[NSColor colorWithCalibratedRed:(r/255.0f) green:(g/255.0f) blue:(b/255.0f) alpha:1.0]];
	}
}

extern "C" void DWAGDIPenWidth(int wid, int w){

}

extern "C" void DWAGDIDrawLine(int wid, int x1,int y1,int x2,int y2){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		[[NSGraphicsContext currentContext] setShouldAntialias: NO];
		//NSWindow *window = [dwa win];
		[[dwa penColor] set];
		NSPoint p1 = NSMakePoint(x1+0.5,dwa.h-y1-0.5);
		NSPoint p2 = NSMakePoint(x2+0.5,dwa.h-y2-0.5);
		NSBezierPath* bp = [NSBezierPath bezierPath];
		[bp setLineWidth:1];
		[bp moveToPoint:p1];
		[bp lineToPoint:p2];
		[bp stroke];
	}
}

extern "C" void DWAGDIDrawEllipse(int wid, int x, int y, int w,int h){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		[[NSGraphicsContext currentContext] setShouldAntialias: YES];
		[[dwa penColor] setStroke];
		NSRect rect = NSMakeRect(x+0.5, dwa.h-h-y-0.5, w, h);
		NSBezierPath* circlePath = [NSBezierPath bezierPath];
		[circlePath appendBezierPathWithOvalInRect: rect];
		[circlePath stroke];
	}
}

extern "C" void DWAGDIFillEllipse(int wid, int x, int y, int w,int h){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		[[NSGraphicsContext currentContext] setShouldAntialias: YES];
		[[dwa penColor] setStroke];
		[[dwa penColor] setFill];
		NSRect rect = NSMakeRect(x+0.5, dwa.h-h-y-0.5, w, h);
		NSBezierPath* circlePath = [NSBezierPath bezierPath];
		[circlePath appendBezierPathWithOvalInRect: rect];
		[circlePath stroke];
		[circlePath fill];
	}
}

extern "C" int DWAGDIGetTextHeight(int wid, int fntid){
	NSFont* fnt = [NSFont menuBarFontOfSize:0];
	return fnt.xHeight+8;
}

extern "C" int DWAGDIGetTextWidth(int wid, int fntid, wchar_t* str){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		[[NSGraphicsContext currentContext] setShouldAntialias: YES];
		NSFont* fnt = [NSFont menuBarFontOfSize:0];
		NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:fnt, NSFontAttributeName,[dwa penColor], NSForegroundColorAttributeName, nil];
		NSString* nsStr = wchartToNString(str);
		NSAttributedString * currentText=[[NSAttributedString alloc] initWithString:nsStr attributes: attributes];
		NSSize attrSize = [currentText size];
		return attrSize.width;
	}
	return 0;
}

extern "C" void DWAGDIDrawText(int wid, int fntid, wchar_t* str, int x, int y){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		[[NSGraphicsContext currentContext] setShouldAntialias: YES];
		//NSWindow *window = [dwa win];
		NSFont* fnt = [NSFont menuBarFontOfSize:0];
		//NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:fnt, NSFontAttributeName,[dwa penColor], NSForegroundColorAttributeName, nil];
		NSDictionary *attributes = @{
				    NSFontAttributeName: fnt,
				    NSForegroundColorAttributeName: [dwa penColor],
				    NSWritingDirectionAttributeName: @[@(NSWritingDirectionLeftToRight | NSWritingDirectionOverride)]
				};
		NSString* nsStr = wchartToNString(str);
		NSAttributedString * currentText=[[NSAttributedString alloc] initWithString:nsStr attributes: attributes];
		[currentText drawAtPoint:NSMakePoint(x, dwa.h-(y+fnt.ascender+2))];
	}
}

extern "C" void DWAGDIFillRectangle(int wid, int x, int y, int w,int h){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		//NSWindow *window = [dwa win];
		[[NSGraphicsContext currentContext] setShouldAntialias: NO];
		[[dwa penColor] set];
		NSRect rec = NSMakeRect(x, dwa.h-h-y, w, h);
		NSRectFill(rec);
	}
}

extern "C" void DWAGDIRepaint(int wid, int x, int y, int w, int h){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		NSWindow *window = [dwa win];
		NSRect rec = NSMakeRect(x, dwa.h-h-y, w, h);
		[[window contentView] setNeedsDisplayInRect:rec];
		//[[window contentView] setNeedsDisplay:YES];
	}
}

extern "C" void DWAGDIClipRectangle(int wid, int x, int y, int w, int h){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		if ([dwa boolClipRect]==true){
			[[NSGraphicsContext currentContext] restoreGraphicsState];
			[dwa setBoolClipRect:false];
		}
		[[NSGraphicsContext currentContext] saveGraphicsState];
		NSRectClip(NSMakeRect(x, dwa.h-h-y, w, h));
		[dwa setBoolClipRect:true];
	}
}

extern "C" void DWAGDIClearClipRectangle(int wid){
	DWAWindow* dwa = getWindowByID(wid);
	if (dwa!=NULL){
		if ([dwa boolClipRect]==true){
			[[NSGraphicsContext currentContext] restoreGraphicsState];
			[dwa setBoolClipRect:false];
		}
	}
}

extern "C" void DWAGDIGetScreenSize(int* size){
	NSRect e = [[NSScreen mainScreen] frame];
	size[0]=e.size.width;
	size[1]=e.size.height;
}

extern "C" void DWAGDIGetImageSize(wchar_t* fname, int* size){
	NSImage *image = [[NSImage alloc] initWithContentsOfFile:wchartToNString(fname)];
	NSImageRep* bitmapImageRep = [[image representations] objectAtIndex:0];
	size[0] = bitmapImageRep.pixelsWide;
	size[1] = bitmapImageRep.pixelsHigh;
	[image release];
}

extern "C" void DWAGDIGetMousePosition(int* pos){
	CGEventRef ourEvent = CGEventCreate(NULL);
	CGPoint point = CGEventGetLocation(ourEvent);
	CFRelease(ourEvent);
	pos[0]=point.x;
	pos[1]=point.y;
}


extern "C" void DWAGDISetClipboardText(wchar_t* str){

}

extern "C" wchar_t* DWAGDIGetClipboardText(){
	return NULL;
}

#endif
