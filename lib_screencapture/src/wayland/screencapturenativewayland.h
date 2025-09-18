/* 
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#if defined OS_WAYLAND

#ifndef SCREENCAPTURENATIVE_H_
#define SCREENCAPTURENATIVE_H_

#define STATUS_NONE 0
#define STATUS_PERMISSION 1
#define STATUS_CAPTURE 2
#define STATUS_ERROR 3
#define STATUS_EXIT 4

using namespace std;

#include <string.h>
#include <stdlib.h>
#include <dbus/dbus.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <atomic>
#include <random>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <sys/mman.h>
#include <sys/time.h>
#include <X11/keysym.h>
#include "../linux/linuxcpuusage.h"
#include "../common/logger.h"
#include "../common/extern_v3.h"

struct DBusMonitorInfo {
    const char* id;
    int x, y;
    int width, height;
    pw_stream *pwstream;
    spa_hook streamlistener;
    unsigned char *lastFrameData;
    int lastFrameSize;
    int lastFrameBPR;
    uint32_t nodeid;
};
vector<DBusMonitorInfo*> dbusmonitors;

struct DBusRequest{
	bool complete;
	string name;
	void* argument;
	dbus_uint32_t rserial;
	string strerror;
	string progress;
	string response;
	string session;
	unsigned long cnttoken;
	int versionRemoteDesktop;
	int versionScreenCast;
	int availableCursorModesScreenCast;
	bool clipboard;
	string restoreToken;
	bool restoreTokenChanged;
};

struct DBusClipboard{
	bool busy;
	string state;
	string progress;
	dbus_uint32_t rserial;
	dbus_uint32_t userial;
	long sizedata;
	char* data;
	bool changed;
};

struct DBusInput{
	string name;
	dbus_uint32_t rserial;
	bool complete;
	uint32_t nodeid;
	double x;
	double y;
	int32_t button;
	uint32_t state;
	uint32_t axis;
	int32_t step;
	int32_t ikey;
	useconds_t vsleep;
	struct timeval ssleep;
};

queue<DBusInput*> dbusinputs;

struct ScreenCaptureInfo{
	int status; //0:NOT INIT; 1:INIT; 2:READY
	DBusMonitorInfo* dbusmonitor;
	int x;
	int y;
	int w;
	int h;
	RGB_IMAGE* rgbimage;
};

pthread_t tdcapture;
pthread_t tddbus;
int tdstatus;
bool monchanged;
string tderror;
mutex tdlock;
condition_variable tdcondvar;

DBusConnection* dbusconn;
DBusRequest dbusrequest;
DBusClipboard dbusclipboard;
atomic<bool> dbusclose(false);

pw_main_loop* pwloop = nullptr;
LinuxCPUUsage* cpuUsage;

bool ctrlDown;
bool altDown;
bool shiftDown;
bool commandDown;
int32_t mousebtn1Code;
int32_t mousebtn2Code;
int32_t mousebtn3Code;
uint32_t mousebtn1Down;
uint32_t mousebtn2Down;
uint32_t mousebtn3Down;

int cursorX;
int cursorY;
bool cursorChanged;
int cursorHotspotX;
int cursorHotspotY;
int cursorWidth;
int cursorHeight;
unsigned char * cursorData;
int cursorDataSize;



#endif /* SCREENCAPTURENATIVE_H_ */

#endif
