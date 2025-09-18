#if defined OS_SCREENCAPTUREKIT

#include <CoreVideo/CoreVideo.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Foundation/Foundation.h>
#include <ScreenCaptureKit/ScreenCaptureKit.h>
#include "screencapturenativekitobjc.h"


//dispatch_queue_t videoConcurrentQueue = dispatch_queue_create("net.dwservice.dwagent.VideoConcurrent", DISPATCH_QUEUE_SERIAL);

@interface StreamCaptureDelegate: NSObject <SCStreamOutput>

@property (nonatomic, assign) bool bclose;
@property (nonatomic, assign) CMSampleBufferRef latestSampleBuffer;
//@property (nonatomic, assign) CVImageBufferRef lastFrame;
@property (nonatomic, assign) dispatch_semaphore_t semOutput;
@property (nonatomic, assign) SCDisplay* display;
@property (nonatomic, assign) SCContentFilter* filter;
@property (nonatomic, assign) SCStreamConfiguration* config;
@property (nonatomic, assign) SCStream* stream;

@end

@implementation StreamCaptureDelegate

- (instancetype)init
{
    self = [super init];
    if (self) {
    	_bclose = false;
    	_semOutput = dispatch_semaphore_create(1);
        _latestSampleBuffer = NULL; 
    	//_lastFrame = NULL;
    }
    return self;
}

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type
{	
	if (type==SCStreamOutputTypeScreen){
		
		/*
		dispatch_semaphore_wait(_semOutput, DISPATCH_TIME_FOREVER);
		if (_bclose){
			return;
		}
		if (_lastFrame) {
			CVPixelBufferRelease(_lastFrame);
			_lastFrame=NULL;
		}		
		if (CMSampleBufferIsValid(sampleBuffer)) {
			CVImageBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
			if (pixelBuffer != NULL) {
				CVPixelBufferRetain(pixelBuffer);		
				_lastFrame = pixelBuffer;
			}					
		}
		dispatch_semaphore_signal(_semOutput);
		*/
		
		/*dispatch_async(videoConcurrentQueue, ^{
			if (_lastFrame != NULL) {
				CVPixelBufferRelease(_lastFrame);
			}
			_lastFrame = pixelBuffer;			
		});*/
		
		CMSampleBufferRef newSampleBuffer = NULL;
		OSStatus status = CMSampleBufferCreateCopy(kCFAllocatorDefault, sampleBuffer, &newSampleBuffer);
		if (status == noErr && newSampleBuffer) {
			dispatch_semaphore_wait(_semOutput, DISPATCH_TIME_FOREVER);
			if (_latestSampleBuffer) {
				CFRelease(_latestSampleBuffer);
			}
			if (!_bclose){
				_latestSampleBuffer = newSampleBuffer;
			}else{
				CFRelease(newSampleBuffer);
			}
			dispatch_semaphore_signal(_semOutput);
		}		
		
	}
}

- (int)startCapture:(ScreenCaptureInfo*)sci {
	__block int iret=0;
	_display = NULL;	
	dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
	[SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent * _Nullable content, NSError * _Nullable error) {
		if (!error) {
			for (int i=0;i<=content.displays.count;i++){
				if (content.displays[i].displayID==sci->displayID){
					_display = content.displays[i];
					break;
				}
			}
			if (_display!=NULL){
				_filter = [[SCContentFilter alloc] initWithDisplay:_display excludingWindows:@[]];
				_config = [[SCStreamConfiguration alloc] init];
				_config.width = sci->w;
				_config.height = sci->h;
				//_config.minimumFrameInterval = CMTimeMake(1, 30);
				_config.showsCursor = false;
				_config.pixelFormat = kCVPixelFormatType_32BGRA;
				_config.colorSpaceName = kCGColorSpaceSRGB;				
				_stream = [[SCStream alloc] initWithFilter:_filter configuration:_config delegate:nil];
				NSError* error = nil;
				BOOL did_add_output = [_stream addStreamOutput:self type:SCStreamOutputTypeScreen sampleHandlerQueue:nil error:&error];
				if (!did_add_output) {
					iret=-12;
				}else{
					[_stream addStreamOutput:self type:SCStreamOutputTypeAudio sampleHandlerQueue:nil error:&error];
					[_stream startCaptureWithCompletionHandler:^(NSError * _Nullable error) {
						if (error) {
							iret=-13;				
						}
						dispatch_semaphore_signal(semaphore);			
					}];
				}
			}else{
				iret=-11;
				dispatch_semaphore_signal(semaphore);
			}
		}else{
			dispatch_semaphore_signal(semaphore);
		}
	}];	
	dispatch_time_t timeoutTime = dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC);
	long result = dispatch_semaphore_wait(semaphore, timeoutTime);
	if (result != 0) {
		iret=-19;
	}	
	return iret;	
}

- (void)stopCapture {
	
	/*
	dispatch_semaphore_wait(_semOutput, DISPATCH_TIME_FOREVER);
	_bclose=true;
	if (_lastFrame) {
		CVPixelBufferRelease(_lastFrame);
		_lastFrame=NULL;
	}
	dispatch_semaphore_signal(_semOutput);
	*/
	
	dispatch_semaphore_wait(_semOutput, DISPATCH_TIME_FOREVER);
	_bclose=true;
	if (_latestSampleBuffer) {		
		CFRelease(_latestSampleBuffer);
		_latestSampleBuffer = NULL;
	}
	dispatch_semaphore_signal(_semOutput);
	
	
	dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
	NSError* error = nil;
	[_stream removeStreamOutput:self type:SCStreamOutputTypeScreen error:&error];
	[_stream removeStreamOutput:self type:SCStreamOutputTypeAudio error:&error];
	[_stream stopCaptureWithCompletionHandler:^(NSError * _Nullable error) {
		dispatch_semaphore_signal(semaphore);
	}];
	dispatch_time_t timeoutTime = dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC);
	dispatch_semaphore_wait(semaphore, timeoutTime);
}


- (void)getImage:(ScreenCaptureInfo*)sci
{
	/*
	CVImageBufferRef imageBuffer = NULL;
	dispatch_semaphore_wait(_semOutput, DISPATCH_TIME_FOREVER);
	imageBuffer=_lastFrame;
	_lastFrame=NULL;
	dispatch_semaphore_signal(_semOutput);		
	if (imageBuffer != NULL) {
		CVPixelBufferLockBaseAddress(imageBuffer, 0);
		size_t width = CVPixelBufferGetWidth(imageBuffer);
		size_t height = CVPixelBufferGetHeight(imageBuffer);
		uint8_t *baseAddress = (uint8_t *)CVPixelBufferGetBaseAddress(imageBuffer);
		size_t bytesPerRow = CVPixelBufferGetBytesPerRow(imageBuffer);		
		RGB_IMAGE* rgbimage=sci->rgbimage;
		rgbimage->sizechangearea=0;
		rgbimage->sizemovearea=0;		
		int offsetDst = 0;
		for (int row = 0; row < height; ++row){
			for (int col = 0; col < width; ++col){
				uint8_t *pixel = baseAddress + (row * bytesPerRow) + (col * 4); 
				unsigned char r=pixel[2];
				unsigned char g=pixel[1];
				unsigned char b=pixel[0];				
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
		}
		CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
		CVPixelBufferRelease(imageBuffer);
		imageBuffer=NULL;
	}
	*/
	
	
	CMSampleBufferRef tmpSampleBuffer = NULL;
	dispatch_semaphore_wait(_semOutput, DISPATCH_TIME_FOREVER);
	tmpSampleBuffer=_latestSampleBuffer;
	_latestSampleBuffer=NULL;
	dispatch_semaphore_signal(_semOutput);
	if (tmpSampleBuffer){
		CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(tmpSampleBuffer);
		CVPixelBufferLockBaseAddress(imageBuffer, 0);
		size_t width = CVPixelBufferGetWidth(imageBuffer);
		size_t height = CVPixelBufferGetHeight(imageBuffer);
		uint8_t *baseAddress = (uint8_t *)CVPixelBufferGetBaseAddress(imageBuffer);
		size_t bytesPerRow = CVPixelBufferGetBytesPerRow(imageBuffer);		
		RGB_IMAGE* rgbimage=sci->rgbimage;
		rgbimage->sizechangearea=0;
		rgbimage->sizemovearea=0;		
		int offsetDst = 0;
		for (int row = 0; row < height; ++row){
			for (int col = 0; col < width; ++col){
				uint8_t *pixel = baseAddress + (row * bytesPerRow) + (col * 4); 
				unsigned char r=pixel[2];
				unsigned char g=pixel[1];
				unsigned char b=pixel[0];				
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
		}
		CVPixelBufferUnlockBaseAddress(imageBuffer, 0);	
		CFRelease(tmpSampleBuffer);
	}
	
}

@end

void DWAScreenCaptureKitLoad(){
}

void DWAScreenCaptureKitUnload(){
}

int DWAScreenCaptureKitStartCapture(ScreenCaptureInfo* sci){
	StreamCaptureDelegate* ki=[[StreamCaptureDelegate alloc] init];
	sci->kitInfo=ki;
	return [ki startCapture:sci];
}

void DWAScreenCaptureKitStopCapture(ScreenCaptureInfo* sci){
	StreamCaptureDelegate* ki = (StreamCaptureDelegate*)sci->kitInfo;
	[ki stopCapture];
}


int DWAScreenCaptureKitGetImage(ScreenCaptureInfo* sci){
	StreamCaptureDelegate* ki = (StreamCaptureDelegate*)sci->kitInfo;
	[ki getImage:sci];
	return 0;
}

#endif
