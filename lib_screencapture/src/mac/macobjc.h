#if defined OS_MAC

#ifndef MACOBJC_H_
#define MACOBJC_H_

/*struct MouseImageCapture{
	void* oref;
	CGImageRef image;
	int offx;
	int offy;
	int w;
	int h;
	int status; //0 NOT CHANGED; //1 CHANGED; //2 ERROR
};*/

//void macobjcInitApplication();
int macobjcGetClipboardText(wchar_t** wText);
void macobjcSetClipboardText(wchar_t* wText);
//MouseImageCapture macobjcCaptureMouseImage(void* prevoref);


#endif /* MACOBJC_H_ */
#endif
