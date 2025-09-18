#if defined OS_QUARZDISPLAY

#include <CoreVideo/CoreVideo.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreGraphics/CoreGraphics.h>
#include <AVFoundation/AVFoundation.h>
#include <IOSurface/IOSurface.h>
#include <sys/sysctl.h>
#include "screencapturenativequartzdisplayobjc.h"


dispatch_queue_t captureDisplayStreamQueue = dispatch_queue_create("screenCaptureDisplayStreamQueue", DISPATCH_QUEUE_SERIAL);

bool bLoadOSVersion=false;
NSOperatingSystemVersion osVersion;
bool bCaptureDisplayStream=true;

@interface ScreenCaptureDisplayStreamDelegate : NSObject

@property (nonatomic, assign) bool bclose;
@property (nonatomic, assign) CGDisplayStreamRef captureSession;
@property (nonatomic, assign) unsigned char *lastFrameData;
@property (nonatomic, assign) size_t lastFrameDataSize;
@property (nonatomic, assign) size_t lastFrameWidth;
@property (nonatomic, assign) size_t lastFrameHeight;
@property (nonatomic, assign) size_t lastFrameBPR;
@property (nonatomic, assign) dispatch_semaphore_t semOutput;

@end

@implementation ScreenCaptureDisplayStreamDelegate

- (instancetype)init
{
    self = [super init];
    if (self) {
    	self.bclose = false;
    	self.semOutput = dispatch_semaphore_create(1);
    	self.lastFrameData=NULL;
    	self.lastFrameDataSize=0;
    	self.lastFrameWidth=0;
		self.lastFrameHeight=0;
		self.lastFrameBPR=0;
    }
    return self;
}

- (void)frameHandlerWithStatus:(CGDisplayStreamFrameStatus)status
                    displayTime:(uint64_t)displayTime
                    frameSurface:(IOSurfaceRef)frameSurface
                    updateRef:(CGDisplayStreamUpdateRef)updateRef {        
    
    if (status == kCGDisplayStreamFrameStatusFrameComplete && frameSurface != NULL) {    	
    	IOReturn lockResult = IOSurfaceLock(frameSurface, kIOSurfaceLockReadOnly, NULL);
		if (lockResult != kIOReturnSuccess) {
			return;
		}

		void *baseAddress = IOSurfaceGetBaseAddress(frameSurface);
		if (!baseAddress) {
			IOSurfaceUnlock(frameSurface, kIOSurfaceLockReadOnly, NULL);
			return;
		}
		dispatch_semaphore_wait(self.semOutput, DISPATCH_TIME_FOREVER);
		if (!self.bclose){
			self.lastFrameWidth = IOSurfaceGetWidth(frameSurface);
			self.lastFrameHeight = IOSurfaceGetHeight(frameSurface);
			self.lastFrameBPR = IOSurfaceGetBytesPerRow(frameSurface);	
			size_t sz = self.lastFrameHeight * self.lastFrameBPR;
			if (self.lastFrameData==NULL){
				self.lastFrameData=(unsigned char *)malloc(sz);
				self.lastFrameDataSize = sz;				
			}else if (self.lastFrameDataSize != sz){
				free(self.lastFrameData);
				self.lastFrameData=(unsigned char *)malloc(sz);
				self.lastFrameDataSize = sz;
			}			
			if (self.lastFrameData!=NULL){
				memcpy(self.lastFrameData, baseAddress, self.lastFrameDataSize);				
			}else{
				self.lastFrameDataSize=0;
				self.lastFrameWidth=0;
				self.lastFrameHeight=0;
				self.lastFrameBPR=0;
			}			
		}
		dispatch_semaphore_signal(self.semOutput);
		IOSurfaceUnlock(frameSurface, kIOSurfaceLockReadOnly, NULL);
    }       
    
}

- (int)startCapture:(ScreenCaptureInfo*)sci {
	__block int iret=0;
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= 120000)
	if (bCaptureDisplayStream){
		self.captureSession = CGDisplayStreamCreateWithDispatchQueue(sci->displayID,
									sci->w, sci->h,
									kCVPixelFormatType_32BGRA,
									NULL,
									captureDisplayStreamQueue,
									^(CGDisplayStreamFrameStatus status,
									  uint64_t displayTime,
									  IOSurfaceRef frameSurface,
									  CGDisplayStreamUpdateRef updateRef) {
					[self frameHandlerWithStatus:status
					  displayTime:displayTime
					  frameSurface:frameSurface
					  updateRef:updateRef];
				});
		if (self.captureSession!=NULL){
			CGDisplayStreamStart(self.captureSession);
		}else{
			iret=-18;
		}
	}
#endif
	return iret;
}

- (void)stopCapture {
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= 120000)
	if (bCaptureDisplayStream){
		dispatch_semaphore_wait(self.semOutput, DISPATCH_TIME_FOREVER);
		self.bclose=true;
		if (self.lastFrameData!=NULL){
			free(self.lastFrameData);
			self.lastFrameData=NULL;
			self.lastFrameDataSize=0;
			self.lastFrameWidth=0;
			self.lastFrameHeight=0;
			self.lastFrameBPR=0;
		}
		dispatch_semaphore_signal(self.semOutput);
		CGDisplayStreamStop(self.captureSession);
		CFRelease(self.captureSession);
		self.captureSession = NULL;
	}
#endif
}

- (int)getImage:(ScreenCaptureInfo*)sci
{	
	unsigned char* data = NULL;	
	size_t imgw = 0;
	size_t imgh = 0;
	size_t bpr = 0;
	size_t bpp=32;
	CGImageRef imageref = NULL;
	CFDataRef dataref = NULL;	
	if (bCaptureDisplayStream){
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= 120000)
		dispatch_semaphore_wait(self.semOutput, DISPATCH_TIME_FOREVER);
		if (!self.bclose){
			data=self.lastFrameData;		
			imgw=self.lastFrameWidth;
			imgh=self.lastFrameHeight;
			bpr=self.lastFrameBPR;
		}
		self.lastFrameData=NULL;
		self.lastFrameDataSize=0;
		self.lastFrameWidth=0;
		self.lastFrameHeight=0;
		self.lastFrameBPR=0;
		dispatch_semaphore_signal(self.semOutput);
#endif
	}else{
		imageref = CGDisplayCreateImage(sci->displayID);
		if (imageref==NULL){
			return -4; //Identifica CGDisplayCreateImage failed
		}
		CGDataProviderRef dataProvider = CGImageGetDataProvider(imageref);
		dataref = CGDataProviderCopyData(dataProvider);
		bpp = CGImageGetBitsPerPixel(imageref);
		bpr = CGImageGetBytesPerRow(imageref);
		imgw=CGImageGetWidth(imageref);
		imgh=CGImageGetHeight(imageref);
		data = (unsigned char*)CFDataGetBytePtr(dataref);
	}
	RGB_IMAGE* rgbimage=sci->rgbimage;
	rgbimage->sizechangearea=0;
	rgbimage->sizemovearea=0;
	if (data != NULL) {
		//CONVERT IN RGB
		int offsetSrc = 0;
		int offsetDst = 0;	
		int rowOffset = bpr % imgw;
		for (int row = 0; row < imgh; ++row){
			for (int col = 0; col < imgw; ++col){
				unsigned char r=0;
				unsigned char g=0;
				unsigned char b=0;
				if (bpp>24){
					r = data[offsetSrc+2];
					g = data[offsetSrc+1];
					b = data[offsetSrc];
					offsetSrc += 4;
				}else if (bpp>16){
					r = data[offsetSrc+2];
					g = data[offsetSrc+1];
					b = data[offsetSrc];
					offsetSrc += 3;
				//}else if (bpp>8){
				//	unsigned int pixel=(data[offsetSrc+1] << 8) | (data[offsetSrc]);
				//	r = (pixel & sci->image->red_mask) >> sci->redlshift;
				//	g = (pixel & sci->image->green_mask) >> sci->greenlshift;
				//	b = (pixel & sci->image->blue_mask) << sci->bluershift;
				//	offsetSrc += 2;
				//}else{
				//	unsigned int pixel=(data[offsetSrc]);
				//	r = (pixel & sci->image->red_mask) >> sci->redlshift;
				//	g = (pixel & sci->image->green_mask) >> sci->greenlshift;
				//	b = (pixel & sci->image->blue_mask) << sci->bluershift;
				//	offsetSrc += 1;
				}
				if ((row<sci->h) && (col<sci->w)){
					if ((rgbimage->sizechangearea==0) and ((sci->status==1) or ((rgbimage->data[offsetDst] != r) or (rgbimage->data[offsetDst+1] != g) or (rgbimage->data[offsetDst+2] != b)))){
						rgbimage->sizechangearea=1;
						rgbimage->changearea[0].x=0;
						rgbimage->changearea[0].y=0;
						rgbimage->changearea[0].width=sci->w;
						rgbimage->changearea[0].height=sci->h;
					}
					rgbimage->data[offsetDst] = r;
					rgbimage->data[offsetDst+1] = g;
					rgbimage->data[offsetDst+2] = b;
					offsetDst += 3;
				}
			}
			offsetSrc += rowOffset;
		}
		if (bCaptureDisplayStream){
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= 120000)
			free(data);
#endif
		}else{
			CFRelease(dataref);
			CGImageRelease(imageref);
		}
	}
	return 0;
}

@end

void loadMacOSVersion() {
	if (!bLoadOSVersion){
		size_t sz;
		sysctlbyname("kern.osrelease", NULL, &sz, NULL, 0);	
		char *appc = (char *)malloc(sz);
		sysctlbyname("kern.osrelease", appc, &sz, NULL, 0);	
		NSString *sver = [NSString stringWithCString:appc encoding:NSUTF8StringEncoding];
		free(appc);
		NSArray *ar = [sver componentsSeparatedByString:@"."];
		NSInteger majorVersion = [ar[0] integerValue];
		//NSInteger minorVersion = [ar[1] integerValue];
		//NSInteger patchVersion = (ar.count > 2) ? [ar[2] integerValue] : 0;
		if (majorVersion == 11) {
			osVersion = (NSOperatingSystemVersion){10, 7, 0};
		}else if (majorVersion == 12) {
			osVersion = (NSOperatingSystemVersion){10, 8, 0};
		}else if (majorVersion == 13) {
			osVersion = (NSOperatingSystemVersion){10, 9, 0};
		}else if (majorVersion == 14) {
			osVersion = (NSOperatingSystemVersion){10, 10, 0};
		}else if (majorVersion > 14) {
			osVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
		}else{
			osVersion = (NSOperatingSystemVersion){0, 0, 0};  // Version 0 for macOS < 10.10
		}
		bLoadOSVersion=true;
	}
}

bool isMacOSVersionAtLeast11() {
	loadMacOSVersion();
	return (osVersion.majorVersion >= 11);
}

bool isMacOSVersionAtLeast12() {
	loadMacOSVersion();
	return (osVersion.majorVersion >= 12);
}

void DWAScreenCaptureDisplayStreamLoad(){
	bCaptureDisplayStream=isMacOSVersionAtLeast12();	
}

void DWAScreenCaptureDisplayStreamUnload(){
}

int DWAScreenCaptureDisplayStreamStartCapture(ScreenCaptureInfo* sci){
	ScreenCaptureDisplayStreamDelegate* o=[[ScreenCaptureDisplayStreamDelegate alloc] init];
	sci->objcInfo=o;
	return [o startCapture:sci];	
}

void DWAScreenCaptureDisplayStreamStopCapture(ScreenCaptureInfo* sci){
	ScreenCaptureDisplayStreamDelegate* o = (ScreenCaptureDisplayStreamDelegate*)sci->objcInfo;
	[o stopCapture];
}

int DWAScreenCaptureDisplayStreamGetImage(ScreenCaptureInfo* sci){
	ScreenCaptureDisplayStreamDelegate* o = (ScreenCaptureDisplayStreamDelegate*)sci->objcInfo;
	return [o getImage:sci];	
}

#endif
