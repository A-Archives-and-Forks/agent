#if defined OS_MAC

#include <AppKit/AppKit.h>
#include <wchar.h>
#include "macobjc.h"

//__strong NSImage* macobjcCaptureMouseImagePrev = NULL;

int macobjcGetClipboardText(wchar_t** wText){
	NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
	NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
	int reqsize = [string lengthOfBytesUsingEncoding:NSUTF8StringEncoding]+1;
	if (reqsize>1){
		char* buf = (char*)malloc(reqsize);
		memcpy(buf, [string UTF8String], reqsize);
		*wText = (wchar_t*)malloc((reqsize+1) * sizeof(wchar_t));
		mbstowcs(*wText, (char*)buf, reqsize+1);
		free(buf);
		return [string length];
	}else{
		return 0;
	}
}

void macobjcSetClipboardText(wchar_t* wText){
	size_t len = wcstombs(NULL, wText, 0);
	char* buf = (char*)malloc(((len)+1) * sizeof(char));
	wcstombs(buf, wText, len+1);
	NSString* string = [[NSString alloc] initWithBytes:(void*)buf
	                                     length:len
	                                     encoding:NSUTF8StringEncoding];
	free(buf);
	NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
	[pasteboard clearContents];
	[pasteboard setString:string forType:NSPasteboardTypeString];
}

/*void macobjcInitApplication(){
	[NSApplication sharedApplication];
}*/

/*
MouseImageCapture macobjcCaptureMouseImage(void* prevoref){
	MouseImageCapture ret = {};	
    //@autoreleasepool {
    	ret.status=0;
        NSCursor *cur = [NSCursor currentSystemCursor];
        if(cur==nil){
        	ret.status=2;
        	return ret;
        }
        
        NSImage* nsimage = [cur image];
        if (nsimage == nil || !nsimage.isValid) {
        	ret.status=2;
			return ret;
        }
        if (prevoref!=NULL){
        	NSImage *nsimageprev = (NSImage*)prevoref;
        	if ([[nsimage TIFFRepresentation] isEqual:[nsimageprev TIFFRepresentation]]){
				return ret;
			}
        }
        ret.oref=nsimage;
        
        
        NSSize nssize = [nsimage size];
                
        //[nsimage TIFFRepresentation];
        CGImageSourceRef source = CGImageSourceCreateWithData((CFDataRef)[nsimage TIFFRepresentation], NULL);
        //ret.image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
        
        CGImageRef cgimage = CGImageSourceCreateImageAtIndex(source, 0, NULL);
        //CGImageRef cgimage = [nsimage CGImageForProposedRect:NULL context:nil hints:nil];
        if (cgimage!=nil){
        	
			if ((CGImageGetWidth(cgimage) != nssize.width) || (CGImageGetHeight(cgimage) != nssize.height)) {
				CGImageRef scaledcgimage = nil;
				CGColorSpaceRef colorspace = CGImageGetColorSpace(cgimage);
				CGContextRef context = CGBitmapContextCreate(NULL,
															   nssize.width,
															   nssize.height,
															   CGImageGetBitsPerComponent(cgimage),
															   nssize.width * 4,
															   colorspace,
															   CGImageGetBitmapInfo(cgimage));
				if (context!=nil){
					CGContextDrawImage(context, CGRectMake(0, 0, nssize.width, nssize.height), cgimage);
					scaledcgimage = CGBitmapContextCreateImage(context);
					CGContextRelease(context);
				}
				CGImageRelease(cgimage);
				cgimage = scaledcgimage;
			}
			//if (CGImageGetBitsPerPixel(cgimage) != DesktopFrame::kBytesPerPixel * 8 ||
			//	  CGImageGetWidth(cgimage) != static_cast<size_t>(size.width()) ||
			//	  CGImageGetBitsPerComponent(cgimage) != 8) {
			//	if (scaledcgimage != nil) CGImageRelease(scaledcgimage);
			//		return;
			//}        
			ret.image=cgimage;
			
        }
        if (ret.image!=nil){
			NSPoint p = [cur hotSpot];				
			ret.offx = p.x;
			ret.offy = p.y;
			ret.w=nssize.width;
			ret.h=nssize.height;
			ret.status=1;
        }else{
        	ret.status=2;
        }
        CFRelease(source);
    //} 
    return ret;
}
*/

#endif
