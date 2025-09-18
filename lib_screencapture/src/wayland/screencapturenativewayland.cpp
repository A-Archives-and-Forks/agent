/* 
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#if defined OS_WAYLAND

#include "screencapturenativewayland.h"


int tdstatusGet(){
	int st=0;
	tdlock.lock();
	st=tdstatus;
	tdlock.unlock();
	return st;
}

int tdstatusSet(int st){
	int stret=0;
	tdlock.lock();
	stret=tdstatus;
	tdstatus=st;
	tdlock.unlock();
	return stret;
}

void tdjoinwait(pthread_t td,int tm){
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += tm;
	int result = pthread_timedjoin_np(td, NULL, &timeout);
	if (result == ETIMEDOUT) {
		pthread_cancel(td);
	}
}

std::string dbusGenerateRequestToken() {
	dbusrequest.cnttoken++;
    return "dwa_req_token_" + std::to_string(dbusrequest.cnttoken);
}

std::string dbusGenerateSessionToken() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 9);
    std::string token = "dwagent";
    for (int i = 0; i < 20; ++i) {
        token += std::to_string(distrib(gen));
    }
    return token;
}

void dbusAddDictEntry(DBusMessageIter* outer_array, const char* key, int type, void* value) {

    const char* type_sig = NULL;
    if (type == DBUS_TYPE_UINT32) type_sig = DBUS_TYPE_UINT32_AS_STRING;
    else if (type == DBUS_TYPE_BOOLEAN) type_sig = DBUS_TYPE_BOOLEAN_AS_STRING;
    else if (type == DBUS_TYPE_STRING) type_sig = DBUS_TYPE_STRING_AS_STRING;

    DBusMessageIter dict_entry, variant;
    dbus_message_iter_open_container(outer_array, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&dict_entry, DBUS_TYPE_VARIANT, type_sig, &variant);
    dbus_message_iter_append_basic(&variant, type, &value);
    dbus_message_iter_close_container(&dict_entry, &variant);
    dbus_message_iter_close_container(outer_array, &dict_entry);

}

void dbusCompleteRequest(const char* errmsg){
	if (dbusrequest.complete==false){
		dbusrequest.progress="";
		dbusrequest.response="";
		if (errmsg!=NULL){
			dbusrequest.strerror=errmsg;
		}else{
			dbusrequest.strerror="";
		}
		dbusrequest.complete=true;
		tdcondvar.notify_all();
	}
}

void dbusClearRequest(){
	dbusrequest.complete=false;
	dbusrequest.name="";
	dbusrequest.argument=NULL;
	dbusrequest.progress="";
	dbusrequest.response="";
	dbusrequest.strerror="";
	dbusrequest.rserial=0;
}

void dbusClipboardReady(){
	dbusclipboard.busy=false;
	dbusclipboard.state="";
	dbusclipboard.progress="";
	dbusclipboard.userial=0;
}

void dbusPermissionVersionRemoteDesktop(){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.DBus.Properties",
		"Get"
	);
	DBusMessageIter iter;
	dbus_message_iter_init_append(msg, &iter);
	const char* iname = "org.freedesktop.portal.RemoteDesktop";
	const char* pname = "version";
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iname);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &pname);

	dbusrequest.progress="GetVersionRemoteDesktop";
	dbus_connection_send(dbusconn,msg,&dbusrequest.rserial);
	dbus_message_unref(msg);
}

void dbusPermissionVersionScreenCast(){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.DBus.Properties",
		"Get"
	);
	DBusMessageIter iter;
	dbus_message_iter_init_append(msg, &iter);
	const char* iname = "org.freedesktop.portal.ScreenCast";
	const char* pname = "version";
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iname);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &pname);

	dbusrequest.progress="GetVersionScreenCast";
	dbus_connection_send(dbusconn,msg,&dbusrequest.rserial);
	dbus_message_unref(msg);
}

void dbusPermissionAvailableCursorModesScreenCast(){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.DBus.Properties",
		"Get"
	);
	DBusMessageIter iter;
	dbus_message_iter_init_append(msg, &iter);
	const char* iname = "org.freedesktop.portal.ScreenCast";
	const char* pname = "AvailableCursorModes";
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iname);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &pname);

	dbusrequest.progress="GetAvailableCursorModesScreenCast";
	dbus_connection_send(dbusconn,msg,&dbusrequest.rserial);
	dbus_message_unref(msg);
}

void dbusPermissionCreateSession(){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.RemoteDesktop",
		"CreateSession"
	);
	DBusMessageIter iter, outer_array;
	dbus_message_iter_init_append(msg, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &outer_array);
	dbusAddDictEntry(&outer_array, "handle_token", DBUS_TYPE_STRING, (void*)dbusGenerateRequestToken().c_str());
	dbusAddDictEntry(&outer_array, "session_handle_token", DBUS_TYPE_STRING, (void*)dbusGenerateSessionToken().c_str());
	dbus_message_iter_close_container(&iter, &outer_array);
	dbusrequest.progress="CreateSession";
	dbus_connection_send(dbusconn,msg,&dbusrequest.rserial);
	dbus_message_unref(msg);
}

void dbusPermissionSelectDevices(){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.RemoteDesktop",
		"SelectDevices"
	);
	DBusMessageIter iter1, outer1_array;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
	dbus_message_iter_open_container(&iter1, DBUS_TYPE_ARRAY, "{sv}", &outer1_array);
	dbusAddDictEntry(&outer1_array, "handle_token", DBUS_TYPE_STRING, (void*)dbusGenerateRequestToken().c_str());
	dbusAddDictEntry(&outer1_array, "types", DBUS_TYPE_UINT32, (void*)7);

	if (dbusrequest.versionRemoteDesktop>=2){
		dbusAddDictEntry(&outer1_array, "persist_mode", DBUS_TYPE_UINT32, (void*)2);
		if (dbusrequest.restoreToken!=""){
			dbusAddDictEntry(&outer1_array, "restore_token", DBUS_TYPE_STRING, (void*)dbusrequest.restoreToken.c_str());
		}
	}

	dbus_message_iter_close_container(&iter1, &outer1_array);
	dbusrequest.progress="SelectDevices";
	dbus_connection_send(dbusconn,msg,&dbusrequest.rserial);
	dbus_message_unref(msg);
}

void dbusPermissionSelectSources(){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.ScreenCast",
		"SelectSources"
	);
	DBusMessageIter iter1, outer1_array;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
	dbus_message_iter_open_container(&iter1, DBUS_TYPE_ARRAY, "{sv}", &outer1_array);
	dbusAddDictEntry(&outer1_array, "handle_token", DBUS_TYPE_STRING, (void*)dbusGenerateRequestToken().c_str());
	dbusAddDictEntry(&outer1_array, "types", DBUS_TYPE_UINT32, (void*)1);
	dbusAddDictEntry(&outer1_array, "multiple", DBUS_TYPE_BOOLEAN, (void*)TRUE);

	if (dbusrequest.versionScreenCast>=2){
		if (dbusrequest.availableCursorModesScreenCast & 4) {
			dbusAddDictEntry(&outer1_array, "cursor_mode", DBUS_TYPE_UINT32, (void*)4); //METADATA
		} else if (dbusrequest.availableCursorModesScreenCast & 2) {
			dbusAddDictEntry(&outer1_array, "cursor_mode", DBUS_TYPE_UINT32, (void*)2); //VISIBLE
		}
	}

	dbus_message_iter_close_container(&iter1, &outer1_array);
	dbusrequest.progress="SelectSources";
	dbus_connection_send(dbusconn,msg,&dbusrequest.rserial);
	dbus_message_unref(msg);
}

void dbusPermissionClipboard(){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.Clipboard",
		"RequestClipboard"
	);
	DBusMessageIter iter1, outer1_array;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str4 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str4);
	dbus_message_iter_open_container(&iter1, DBUS_TYPE_ARRAY, "{sv}", &outer1_array);
	dbusAddDictEntry(&outer1_array, "handle_token", DBUS_TYPE_STRING, (void*)dbusGenerateRequestToken().c_str());
	dbus_message_iter_close_container(&iter1, &outer1_array);
	dbus_connection_send(dbusconn,msg,NULL);
	dbus_message_unref(msg);
}

void dbusClipboardSelectionRead(){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.Clipboard",
		"SelectionRead"
	);
	DBusMessageIter iter1;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
	const char* mime_type = "text/plain;charset=utf-8";
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_STRING, &mime_type);
	dbusrequest.progress="ClipboardSelectionRead";
	dbus_connection_send(dbusconn,msg,&dbusrequest.rserial);
	dbus_message_unref(msg);
}

void dbusClipboardSelectionReadReply(DBusMessage* reply,CLIPBOARD_DATA* clipboardData){
	DBusMessageIter iter;
	dbus_message_iter_init(reply, &iter);
	if (DBUS_TYPE_UNIX_FD == dbus_message_iter_get_arg_type(&iter)) {
		int fd;
		dbus_message_iter_get_basic(&iter, &fd);
		bool bok=false;
		char *buffer = NULL;
		size_t chunksize=1024;
		size_t totalsize=0;
		size_t pos=0;
		ssize_t bytesread=0;
		while(true){
			if (pos>=totalsize){
				totalsize=pos+chunksize;
				buffer = (char *)realloc(buffer, totalsize);
				if (!buffer) {
					bok=false;
					break;
				}
			}
			bytesread = read(fd, buffer+pos, totalsize-pos);
			if (bytesread > 0) {
				bok=true;
				pos+=bytesread;
			} else {
				break;
			}
		}
		if (bok){
			//std::cout << "buffer: " << buffer << std::endl;
			clipboardData->type=1; //TEXT
			clipboardData->sizedata=0;
			buffer = (char *)realloc(buffer, pos+1);
			if (buffer) {
				buffer[pos] = '\0';
				size_t wchars_needed = mbstowcs(NULL, buffer, pos);
				if (wchars_needed == (size_t)-1) {
					std::cout << "ERROR: Conversion failed: " << std::endl;
				}else{
					clipboardData->data=(unsigned char*)malloc((wchars_needed) * sizeof(wchar_t));
					mbstowcs((wchar_t*)clipboardData->data, buffer, wchars_needed);
					clipboardData->sizedata=wchars_needed*sizeof(wchar_t);
				}
			}
			free(buffer);
		}
		close(fd);
	}
}

void dbusClipboardSelectionSet(){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.Clipboard",
		"SetSelection"
	);
	DBusMessageIter iter1, outer1_array;
    dbus_message_iter_init_append(msg, &iter1);
    const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
    dbus_message_iter_open_container(&iter1, DBUS_TYPE_ARRAY, "{sv}", &outer1_array);
    const char* mime_types_key = "mime_types";
    const char* mime_types[] = { "text/plain;charset=utf-8" };
	int mime_type_count = sizeof(mime_types) / sizeof(mime_types[0]);
    DBusMessageIter dict_entry, variant, string_array;
    dbus_message_iter_open_container(&outer1_array, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
	dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &mime_types_key);
	dbus_message_iter_open_container(&dict_entry, DBUS_TYPE_VARIANT, "as", &variant);
	dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "s", &string_array);
	for (int i = 0; i < mime_type_count; i++) {
		dbus_message_iter_append_basic(&string_array, DBUS_TYPE_STRING, &mime_types[i]);
	}
	dbus_message_iter_close_container(&variant, &string_array);
	dbus_message_iter_close_container(&dict_entry, &variant);
	dbus_message_iter_close_container(&outer1_array, &dict_entry);
    dbus_message_iter_close_container(&iter1, &outer1_array);

    dbusrequest.progress="ClipboardSelectionSet";
    dbus_connection_send(dbusconn,msg,&dbusrequest.rserial);
	dbus_message_unref(msg);
}

void dbusClipboardSelectionSetReply(DBusMessage* reply,CLIPBOARD_DATA* clipboardData){
	if (dbusclipboard.sizedata>0){
		free(dbusclipboard.data);
		dbusclipboard.data=NULL;
		dbusclipboard.sizedata=0;
	}
	size_t m = wcstombs(NULL, (wchar_t*)clipboardData->data, 0);
	if (m == (size_t)-1) {
		return;
	}
	dbusclipboard.data=(char*)malloc(m);
	dbusclipboard.sizedata=m;
	size_t result = wcstombs(dbusclipboard.data, (wchar_t*)clipboardData->data, m);
	if (result == (size_t)-1) {
		free(dbusclipboard.data);
		dbusclipboard.data=NULL;
		dbusclipboard.sizedata=0;
	}
}

void dbusClipboardSelectionWrite(uint32_t userial){
	tdlock.lock();
	dbusclipboard.busy=true;
	dbusclipboard.state="set";
	dbusclipboard.userial=userial;

	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.Clipboard",
		"SelectionWrite"
	);
	DBusMessageIter iter1;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_UINT32, &dbusclipboard.userial);
	dbusclipboard.progress="ClipboardSelectionWrite";
	dbus_connection_send(dbusconn,msg,&dbusclipboard.rserial);
	dbus_message_unref(msg);
	tdlock.unlock();
}

void dbusClipboardSelectionWriteDone(dbus_bool_t success){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.Clipboard",
		"SelectionWriteDone"
	);
	DBusMessageIter iter1;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_UINT32, &dbusclipboard.userial);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_BOOLEAN, &success);
	dbusclipboard.progress="ClipboardSelectionWriteDone";
	dbus_connection_send(dbusconn,msg,&dbusclipboard.rserial);
	dbus_message_unref(msg);
}

void dbusSetClipboard(DBusMessage* reply){
	dbus_bool_t success=FALSE;
	DBusMessageIter iter;
	dbus_message_iter_init(reply, &iter);
	if (DBUS_TYPE_UNIX_FD == dbus_message_iter_get_arg_type(&iter)) {
		int fd;
		dbus_message_iter_get_basic(&iter, &fd);
		if (dbusclipboard.sizedata>0){
			write(fd, dbusclipboard.data, dbusclipboard.sizedata);
			success=TRUE;
		}
		close(fd);
	}
	dbusClipboardSelectionWriteDone(success);
}

void dbusPermissionStart(){
	DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.RemoteDesktop",
		"Start"
    );
    DBusMessageIter iter1, outer1_array;
    dbus_message_iter_init_append(msg, &iter1);
    const char* dbushandleses_str2 = dbusrequest.session.c_str();
    dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str2);
    const char* empty_str2 = "";
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_STRING, &empty_str2);
	dbus_message_iter_open_container(&iter1, DBUS_TYPE_ARRAY, "{sv}", &outer1_array);
	dbusAddDictEntry(&outer1_array, "handle_token", DBUS_TYPE_STRING, (void*)dbusGenerateRequestToken().c_str());
    dbus_message_iter_close_container(&iter1, &outer1_array);
    dbusrequest.progress="Start";
    dbus_connection_send(dbusconn,msg,&dbusrequest.rserial);
    dbus_message_unref(msg);
}

void dbusNotifyPointerMotionAbsolute(DBusInput* i){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.RemoteDesktop",
		"NotifyPointerMotionAbsolute"
	);
	DBusMessageIter iter1, outer1_array;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
	dbus_message_iter_open_container(&iter1, DBUS_TYPE_ARRAY, "{sv}", &outer1_array);
	dbus_message_iter_close_container(&iter1, &outer1_array);

	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_UINT32, &i->nodeid);
	double dx=static_cast<double>(i->x);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_DOUBLE, &dx);
	double dy=static_cast<double>(i->y);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_DOUBLE, &dy);
	dbus_connection_send(dbusconn,msg,&i->rserial);
	dbus_message_unref(msg);
}

void dbusNotifyPointerButton(DBusInput* i){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.RemoteDesktop",
		"NotifyPointerButton"
	);
	DBusMessageIter iter1, outer1_array;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
	dbus_message_iter_open_container(&iter1, DBUS_TYPE_ARRAY, "{sv}", &outer1_array);
	dbus_message_iter_close_container(&iter1, &outer1_array);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_INT32, &i->button);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_UINT32, &i->state);
	dbus_connection_send(dbusconn,msg,&i->rserial);
	dbus_message_unref(msg);
}

void dbusNotifyPointerAxisDiscrete(DBusInput* i){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.RemoteDesktop",
		"NotifyPointerAxisDiscrete"
	);
	DBusMessageIter iter1, outer1_array;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
	dbus_message_iter_open_container(&iter1, DBUS_TYPE_ARRAY, "{sv}", &outer1_array);
	dbus_message_iter_close_container(&iter1, &outer1_array);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_UINT32, &i->axis);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_INT32, &i->step);
	dbus_connection_send(dbusconn,msg,&i->rserial);
	dbus_message_unref(msg);
}

void dbusNotifyKeyboardKeysym(DBusInput* i){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.RemoteDesktop",
		"NotifyKeyboardKeysym"
	);
	DBusMessageIter iter1, outer1_array;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
	dbus_message_iter_open_container(&iter1, DBUS_TYPE_ARRAY, "{sv}", &outer1_array);
	dbus_message_iter_close_container(&iter1, &outer1_array);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_INT32, &i->ikey);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_UINT32, &i->state);
	dbus_connection_send(dbusconn,msg,&i->rserial);
	dbus_message_unref(msg);
}

void dbusNotifyKeyboardKeycode(DBusInput* i){
	DBusMessage* msg = dbus_message_new_method_call(
		"org.freedesktop.portal.Desktop",
		"/org/freedesktop/portal/desktop",
		"org.freedesktop.portal.RemoteDesktop",
		"NotifyKeyboardKeycode"
	);
	DBusMessageIter iter1, outer1_array;
	dbus_message_iter_init_append(msg, &iter1);
	const char* dbushandleses_str1 = dbusrequest.session.c_str();
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_OBJECT_PATH, &dbushandleses_str1);
	dbus_message_iter_open_container(&iter1, DBUS_TYPE_ARRAY, "{sv}", &outer1_array);
	dbus_message_iter_close_container(&iter1, &outer1_array);
	dbus_message_iter_append_basic(&iter1, DBUS_TYPE_INT32, &i->ikey);
		dbus_message_iter_append_basic(&iter1, DBUS_TYPE_UINT32, &i->state);
		dbus_connection_send(dbusconn,msg,&i->rserial);
	dbus_message_unref(msg);
}

void dbusHandleResponse(DBusConnection* connection, DBusMessage* msg) {
	string sresponse="";
	tdlock.lock();
	if ((dbusrequest.name=="permission") && (dbusrequest.response!="")){
		sresponse=dbusrequest.response;
	}
	tdlock.unlock();

	DBusError error;
	dbus_error_init(&error);
	uint32_t response_code;
	if (!dbus_message_get_args(msg, &error,
		DBUS_TYPE_UINT32, &response_code,
		DBUS_TYPE_INVALID)) {
		if (!sresponse.empty()){
			string errMsg="Response " + sresponse + ": " + error.message;
			tdlock.lock();
			dbusCompleteRequest(errMsg.c_str());
			tdlock.unlock();
		}
		dbus_error_free(&error);
		return;
	}
	if (!sresponse.empty() && sresponse == "CreateSession") {
		DBusMessageIter main_iter, array_iter;
		dbus_message_iter_init(msg, &main_iter);
		dbus_message_iter_next(&main_iter);
		dbus_message_iter_recurse(&main_iter, &array_iter);
		while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID) {
			DBusMessageIter dict_entry_iter, variant_iter;
			const char *key = NULL, *value = NULL;
			dbus_message_iter_recurse(&array_iter, &dict_entry_iter);
			dbus_message_iter_get_basic(&dict_entry_iter, &key);
			dbus_message_iter_next(&dict_entry_iter);
			dbus_message_iter_recurse(&dict_entry_iter, &variant_iter);
			dbus_message_iter_get_basic(&variant_iter, &value);
			if (strcmp(key, "session_handle") == 0) {
				tdlock.lock();
				dbusrequest.session=value;
				dbusrequest.progress="";
				dbusrequest.response="";
				dbusPermissionSelectDevices();
				tdlock.unlock();
				break;
			}
			dbus_message_iter_next(&array_iter);
		}
	}else if (!sresponse.empty() && sresponse == "SelectDevices") {
		tdlock.lock();
		dbusPermissionSelectSources();
		tdlock.unlock();
	}else if (!sresponse.empty() && sresponse == "SelectSources") {
		tdlock.lock();
		dbusrequest.clipboard=false;
		dbusPermissionClipboard();
		dbusPermissionStart();
		tdlock.unlock();
	}else if (!sresponse.empty() && sresponse == "Start") {
		DBusMessageIter main_iter, array_iter;
		dbus_message_iter_init(msg, &main_iter);
		dbus_message_iter_next(&main_iter);
		dbus_message_iter_recurse(&main_iter, &array_iter);
		bool bstartok=false;
		const char *restoreToken=NULL;
		while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID) {
			DBusMessageIter entry_iter;
			dbus_message_iter_recurse(&array_iter, &entry_iter);
			const char* key;
			dbus_message_iter_get_basic(&entry_iter, &key);
			dbus_message_iter_next(&entry_iter);
			//std::cerr << "dbusHandleResponse Start key: " << key << std::endl;
			if (strcmp(key, "streams") == 0) {
				tdlock.lock();

				DBusMessageIter variant_iter;
				dbus_message_iter_recurse(&entry_iter, &variant_iter);

				DBusMessageIter streams_iter;
				dbus_message_iter_recurse(&variant_iter, &streams_iter);

				while (dbus_message_iter_get_arg_type(&streams_iter) != DBUS_TYPE_INVALID) {
					bool baddmonitor=true;
					DBusMonitorInfo* curmonitor = new DBusMonitorInfo();
					curmonitor->lastFrameSize=0;
					curmonitor->pwstream=NULL;

					DBusMessageIter struct_iter;
					dbus_message_iter_recurse(&streams_iter, &struct_iter);

					dbus_message_iter_get_basic(&struct_iter, &curmonitor->nodeid);
					dbus_message_iter_next(&struct_iter);


					DBusMessageIter props_iter;
					dbus_message_iter_recurse(&struct_iter, &props_iter);

					while (dbus_message_iter_get_arg_type(&props_iter) != DBUS_TYPE_INVALID) {
						DBusMessageIter entry_iter;
						dbus_message_iter_recurse(&props_iter, &entry_iter);

						const char* prop_key;
						dbus_message_iter_get_basic(&entry_iter, &prop_key);
						dbus_message_iter_next(&entry_iter);

						DBusMessageIter prop_val_iter;
						dbus_message_iter_recurse(&entry_iter, &prop_val_iter);

						if (strcmp(prop_key, "id") == 0) {
							if (dbus_message_iter_get_arg_type(&prop_val_iter) == DBUS_TYPE_STRING) {
								dbus_message_iter_get_basic(&prop_val_iter, &curmonitor->id);
							}
						}else if (strcmp(prop_key, "source_type") == 0) {
							if (dbus_message_iter_get_arg_type(&prop_val_iter) == DBUS_TYPE_UINT32) {
								uint32_t source_type_num;
								dbus_message_iter_get_basic(&prop_val_iter, &source_type_num);
								if (source_type_num != 1) { //1 = monitor, 2 = window, 3 = area
									baddmonitor=false;
									break;
								}
							}else if (dbus_message_iter_get_arg_type(&prop_val_iter) == DBUS_TYPE_STRING) {
								const char* source_type;
								dbus_message_iter_get_basic(&variant_iter, &source_type);
								if (strcmp(source_type, "monitor") != 0) {
									baddmonitor=false;
									break;
								}
							}
						}else if (strcmp(prop_key, "position") == 0) {
							DBusMessageIter variant_iter;
							dbus_message_iter_recurse(&entry_iter, &variant_iter);
							if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_STRUCT) {
								DBusMessageIter variant_iter, struct_iter;
								dbus_message_iter_recurse(&entry_iter, &variant_iter);
								dbus_message_iter_recurse(&variant_iter, &struct_iter);

								dbus_message_iter_get_basic(&struct_iter, &curmonitor->x);
								dbus_message_iter_next(&struct_iter);
								dbus_message_iter_get_basic(&struct_iter, &curmonitor->y);

							} else {
								baddmonitor=false;
							}
						}else if (strcmp(prop_key, "size") == 0) {
							DBusMessageIter variant_iter;
							dbus_message_iter_recurse(&entry_iter, &variant_iter);
							if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_STRUCT) {
								DBusMessageIter variant_iter, struct_iter;
								dbus_message_iter_recurse(&entry_iter, &variant_iter);
								dbus_message_iter_recurse(&variant_iter, &struct_iter);

								dbus_message_iter_get_basic(&struct_iter, &curmonitor->width);
								dbus_message_iter_next(&struct_iter);
								dbus_message_iter_get_basic(&struct_iter, &curmonitor->height);

							} else {
								baddmonitor=false;
							}
						}
						dbus_message_iter_next(&props_iter);
					}

					if (baddmonitor) {
						dbusmonitors.push_back(curmonitor);
						/*
						std::cerr << "id: " << curmonitor->id << std::endl;
						std::cerr << "nodeid: " << curmonitor->nodeid << std::endl;
						std::cerr << "Position: x=" << curmonitor->x << ", y=" << curmonitor->y << std::endl;
						std::cerr << "Size: width=" << curmonitor->width << ", height=" << curmonitor->height << std::endl;
						*/
					}else{
						delete curmonitor;
					}
					dbus_message_iter_next(&streams_iter);
				}
				tdlock.unlock();
				bstartok=true;
			}else if (strcmp(key, "clipboard_enabled") == 0) {
				DBusMessageIter variant_iter;
				dbus_message_iter_recurse(&entry_iter, &variant_iter);
				if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_BOOLEAN) {
					dbus_message_iter_get_basic(&variant_iter, &dbusrequest.clipboard);
				}
			}else if (strcmp(key, "restore_token") == 0) {
				DBusMessageIter variant_iter;
				dbus_message_iter_recurse(&entry_iter, &variant_iter);
				if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_STRING) {
					dbus_message_iter_get_basic(&variant_iter, &restoreToken);
				}
			}
			dbus_message_iter_next(&array_iter);
		}
		tdlock.lock();
		if (bstartok){
			if (restoreToken!=NULL){
				if (strcmp(restoreToken, dbusrequest.restoreToken.c_str()) != 0){
					dbusrequest.restoreToken=string(restoreToken);
					dbusrequest.restoreTokenChanged=true;
				}
			}
			dbusCompleteRequest(NULL);
		}else{
			dbusCompleteRequest("#PermissionRejected");
		}
		tdlock.unlock();
	}
}

void dbusHandleClipboardSelectionOwnerChanged(DBusConnection* connection, DBusMessage* msg) {
	const char *session_path;
	if (!dbus_message_get_args(msg, nullptr,
	                           DBUS_TYPE_OBJECT_PATH, &session_path,
	                           DBUS_TYPE_INVALID)) {
		return;
	}
	DBusMessageIter main_iter, array_iter;
	dbus_message_iter_init(msg, &main_iter);
	dbus_message_iter_next(&main_iter);
	dbus_message_iter_recurse(&main_iter, &array_iter);
	dbus_bool_t sesisowner=false;
	bool mimetypeok=false;
	while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID) {
		DBusMessageIter dict_entry_iter, variant_iter;
		const char *key = NULL;
		dbus_message_iter_recurse(&array_iter, &dict_entry_iter);
		dbus_message_iter_get_basic(&dict_entry_iter, &key);
		dbus_message_iter_next(&dict_entry_iter);
		if (strcmp(key, "mime_types") == 0) {
			dbus_message_iter_recurse(&dict_entry_iter, &variant_iter);
			if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_STRUCT) {
				DBusMessageIter struct_iter;
				dbus_message_iter_recurse(&variant_iter, &struct_iter);
				int currentType;
				while ((currentType = dbus_message_iter_get_arg_type(&struct_iter)) != DBUS_TYPE_INVALID) {
					if (currentType==DBUS_TYPE_ARRAY){
						DBusMessageIter array_mime_iter;
						dbus_message_iter_recurse(&struct_iter, &array_mime_iter);
						int currentTypeArr;
						const char *mime_str;
						while ((currentTypeArr = dbus_message_iter_get_arg_type(&array_mime_iter)) != DBUS_TYPE_INVALID) {
							if (currentTypeArr==DBUS_TYPE_STRING){
								dbus_message_iter_get_basic(&array_mime_iter, &mime_str);
								//std::cerr << "mime_str: " << mime_str << std::endl;
								if ((mime_str!=NULL) && ((strcmp(mime_str,"text/plain;charset=utf-8")==0) || (strcmp(mime_str,"UTF8_STRING")==0))){
									mimetypeok=true;
									break;
								}
							}
							dbus_message_iter_next(&array_mime_iter);
						}
					}
					dbus_message_iter_next(&struct_iter);
				}
			}
		}else if (strcmp(key, "session_is_owner") == 0) {
			dbus_message_iter_recurse(&dict_entry_iter, &variant_iter);
			if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_BOOLEAN) {
				dbus_message_iter_get_basic(&variant_iter, &sesisowner);
				//std::cerr << "session_is_owner: " << sesisowner << std::endl;
			}
		}
		dbus_message_iter_next(&array_iter);
	}

	if (!sesisowner && mimetypeok){
		tdlock.lock();
		dbusclipboard.changed=true;
		tdlock.unlock();
	}

}

void dbusHandleClipboardSelectionTransfer(DBusConnection* connection, DBusMessage* msg) {
	const char *session_path;
	if (!dbus_message_get_args(msg, nullptr,
							   DBUS_TYPE_OBJECT_PATH, &session_path,
							   DBUS_TYPE_INVALID)) {
		return;
	}

	DBusMessageIter main_iter;
	dbus_message_iter_init(msg, &main_iter);
	const char *mime_type = NULL;
	dbus_message_iter_next(&main_iter);
	dbus_message_iter_get_basic(&main_iter, &mime_type);
	uint32_t userial;
	dbus_message_iter_next(&main_iter);
	dbus_message_iter_get_basic(&main_iter, &userial);
	dbusClipboardSelectionWrite(userial);
}


string dbusAddRequestAndWait(const char* name,void* argument){
	string sret="";
	tdlock.lock();
	if (dbusrequest.name!=""){
		std::unique_lock<std::mutex> utdlock(tdlock, std::adopt_lock);
		tdcondvar.wait(utdlock);
	}
	if (dbusrequest.name==""){
		dbusClearRequest();
		dbusrequest.name=name;
		dbusrequest.argument=argument;
		if (dbusrequest.name=="permission"){
			dbusPermissionVersionRemoteDesktop();
		}else if (dbusrequest.name=="getclipboard"){
			dbusClipboardSelectionRead();
		}else if (dbusrequest.name=="setclipboard"){
			dbusClipboardSelectionSet();
		}else if (dbusrequest.name=="message"){
			DBusMessage* msg = (DBusMessage*)argument;
			dbus_connection_send(dbusconn,msg,&dbusrequest.rserial);
			dbus_message_unref(msg);
		}else{
			dbusClearRequest();
			sret="Request name not valid.";
		}
	}
	tdlock.unlock();
	if (sret==""){
		tdlock.lock();
		if (dbusrequest.complete==false){
			std::unique_lock<std::mutex> utdlock(tdlock, std::adopt_lock);
			tdcondvar.wait(utdlock);
		}
		if (dbusrequest.complete){
			sret=dbusrequest.strerror;
		}else if (tdstatus==STATUS_EXIT){
			sret="Capture unloading.";
		}
		dbusClearRequest();
		tdlock.unlock();
	}
	return sret;
}

void dbusAppendInputMouseMove(DBusMonitorInfo* m, double x, double y, useconds_t vsleep){
	tdlock.lock();
	cursorX=m->x+x;
	cursorY=m->y+y;
	bool badd=true;
	if (!dbusinputs.empty()) {
		DBusInput* ilast = (DBusInput*)dbusinputs.back();
		if ((ilast->name=="mousemove") && (ilast->rserial==0) && (ilast->complete==false)){
			ilast->nodeid=m->nodeid;
			ilast->x=x;
			ilast->y=y;
			ilast->vsleep=vsleep;
			badd=false;
		}
	}
	if (badd){
		DBusInput* i = new DBusInput();
		i->name="mousemove";
		i->complete=false;
		i->rserial=0;
		i->nodeid=m->nodeid;
		i->x=x;
		i->y=y;
		i->vsleep=vsleep;
		dbusinputs.push(i);
	}
	tdlock.unlock();
}

void dbusAppendInputMouseButton(int32_t btn, uint32_t st, useconds_t vsleep){
	tdlock.lock();
	DBusInput* i = new DBusInput();
	i->name="mousebutton";
	i->complete=false;
	i->rserial=0;
	i->button=btn;
	i->state=st;
	i->vsleep=vsleep;
	dbusinputs.push(i);
	tdlock.unlock();
}

void dbusAppendInputMouseAxis(uint32_t axis, int32_t step, useconds_t vsleep){
	tdlock.lock();
	DBusInput* i = new DBusInput();
	i->name="mouseaxis";
	i->complete=false;
	i->rserial=0;
	i->axis=axis;
	i->step=step;
	i->vsleep=vsleep;
	dbusinputs.push(i);
	tdlock.unlock();
}

void dbusAppendInputKeyboardSym(int32_t ikey, uint32_t st, useconds_t vsleep){
	tdlock.lock();
	DBusInput* i = new DBusInput();
	i->name="keyboardsym";
	i->complete=false;
	i->rserial=0;
	i->ikey=ikey;
	i->state=st;
	i->vsleep=vsleep;
	dbusinputs.push(i);
	tdlock.unlock();
}

void dbusAppendInputKeyboardCode(int32_t ikey, uint32_t st, useconds_t vsleep){
	tdlock.lock();
	DBusInput* i = new DBusInput();
	i->name="keyboardcode";
	i->complete=false;
	i->rserial=0;
	i->ikey=ikey;
	i->state=st;
	i->vsleep=vsleep;
	dbusinputs.push(i);
	tdlock.unlock();
}

void* dbusThread(void *arg) {
	dbus_bus_add_match(dbusconn, "type='signal',sender='org.freedesktop.portal.Desktop',interface='org.freedesktop.portal.Request'", NULL);
	dbus_bus_add_match(dbusconn, "type='signal',sender='org.freedesktop.portal.Desktop',interface='org.freedesktop.portal.Clipboard'", NULL);
	useconds_t vsleep=0;
	bool binputok=false;
	struct timeval esleepinput;
	useconds_t elapsedinput;
	while (!dbusclose) {
    	dbus_connection_read_write(dbusconn, 0);
		DBusMessage *msg = dbus_connection_pop_message(dbusconn);
		if (!msg) {
			vsleep=10000;
			tdlock.lock();
			if (!dbusinputs.empty()) {
				DBusInput* i = (DBusInput*)dbusinputs.front();
				if (i->rserial!=0){
					binputok=false;
					if (i->complete){
						gettimeofday(&esleepinput, NULL);
						elapsedinput = (esleepinput.tv_sec - i->ssleep.tv_sec) * 1000000 + (esleepinput.tv_usec - i->ssleep.tv_usec);
						if ((elapsedinput<0) || (elapsedinput>=i->vsleep)){
							dbusinputs.pop();
							delete i;
							if (!dbusinputs.empty()) {
								i = (DBusInput*)dbusinputs.front();
								binputok=true;
							}
						}
					}
				}else{
					binputok=true;
				}
				if (binputok){
					if (i->name=="mousemove"){
						dbusNotifyPointerMotionAbsolute(i);
					}else if (i->name=="mousebutton"){
						dbusNotifyPointerButton(i);
					}else if (i->name=="mouseaxis"){
						dbusNotifyPointerAxisDiscrete(i);
					}else if (i->name=="keyboardsym"){
						dbusNotifyKeyboardKeysym(i);
					}else if (i->name=="keyboardcode"){
						dbusNotifyKeyboardKeycode(i);
					}

					if (i->rserial==0){
						dbusinputs.pop();
						delete i;
					}
					vsleep=0;
					/*
					struct timespec ts;
					clock_gettime(CLOCK_MONOTONIC, &ts);
					uint64_t timestamp = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
					std::cerr << "Timestamp: " << timestamp << " ms." << std::endl;
					*/
				}
			}
			tdlock.unlock();
			if (vsleep>0){
				usleep(vsleep);
			}
		}else{
			//std::cerr << "MSG TYPE: " << dbus_message_get_type(msg) << std::endl;
			if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_RETURN ||
			    dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_ERROR) {
			    dbus_uint32_t rserial = dbus_message_get_reply_serial(msg);
			    tdlock.lock();
			    if (rserial != 0) {
			    	//REQUESTS
			    	if (dbusrequest.rserial==rserial){
			    		dbusrequest.rserial=0;
			    		if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_ERROR){
							string errMsg = dbus_message_get_error_name(msg);
							DBusMessageIter args;
							if (dbus_message_iter_init(msg, &args)) {
								bool badd=false;
								do {
									if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
										const char* strarg;
										dbus_message_iter_get_basic(&args, &strarg);
										if (badd==false){
											badd=true;
											errMsg+=" (";
										}else{
											errMsg+=", ";
										}
										errMsg+=strarg;
									}
								} while (dbus_message_iter_next(&args));
								if (badd==true){
									errMsg+=")";
								}
							}
							dbusCompleteRequest(errMsg.c_str());
						}else if (dbusrequest.name=="permission"){
							if (dbusrequest.progress=="GetVersionRemoteDesktop"){
								DBusMessageIter iter,variant;
								dbus_message_iter_init(msg, &iter);
								dbus_message_iter_recurse(&iter, &variant);
								if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_UINT32) {
									dbus_uint32_t version;
									dbus_message_iter_get_basic(&variant, &version);
									dbusrequest.versionRemoteDesktop=version;
								}else{
									dbusrequest.versionRemoteDesktop=0;
								}
								dbusPermissionVersionScreenCast();
							}else if (dbusrequest.progress=="GetVersionScreenCast"){
								DBusMessageIter iter,variant;
								dbus_message_iter_init(msg, &iter);
								dbus_message_iter_recurse(&iter, &variant);
								if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_UINT32) {
									dbus_uint32_t version;
									dbus_message_iter_get_basic(&variant, &version);
									dbusrequest.versionScreenCast=version;
								}else{
									dbusrequest.versionScreenCast=0;
								}
								if (dbusrequest.versionScreenCast>=2){
									dbusPermissionAvailableCursorModesScreenCast();
								}else{
									dbusPermissionCreateSession();
								}
							}else if (dbusrequest.progress=="GetAvailableCursorModesScreenCast"){
								DBusMessageIter iter,variant;
								dbus_message_iter_init(msg, &iter);
								dbus_message_iter_recurse(&iter, &variant);
								if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_UINT32) {
									dbus_uint32_t availableCursorModes;
									dbus_message_iter_get_basic(&variant, &availableCursorModes);
									dbusrequest.availableCursorModesScreenCast=availableCursorModes;
								}else{
									dbusrequest.availableCursorModesScreenCast=0;
								}
								dbusPermissionCreateSession();
							}else{
								dbusrequest.response=dbusrequest.progress;
							}
						}else if (dbusrequest.name=="getclipboard"){
							dbusClipboardSelectionReadReply(msg,(CLIPBOARD_DATA*)dbusrequest.argument);
							dbusCompleteRequest(NULL);
						}else if (dbusrequest.name=="setclipboard"){
							dbusClipboardSelectionSetReply(msg,(CLIPBOARD_DATA*)dbusrequest.argument);
							dbusCompleteRequest(NULL);
						}else if (dbusrequest.name=="message"){
							dbusCompleteRequest(NULL);
						}
					}
			    	//INPUTS
			    	if (!dbusinputs.empty()) {
						DBusInput* i = (DBusInput*)dbusinputs.front();
						if (i->rserial==rserial){
							i->complete=true;
							gettimeofday(&i->ssleep, NULL);
						}
					}
			    	//CLIPBOARD
			    	if ((dbusclipboard.busy) && (dbusclipboard.rserial==rserial)){
			    		if (dbusclipboard.state=="set"){
			    			if (dbusclipboard.progress=="ClipboardSelectionWrite"){
			    				dbusSetClipboard(msg);
			    			}else if (dbusclipboard.progress=="ClipboardSelectionWriteDone"){
			    				dbusClipboardReady();
			    			}
			    		}
			    	}
			    }
			    tdlock.unlock();
			} else if (dbus_message_is_signal(msg, "org.freedesktop.portal.Request", "Response")) {
				dbusHandleResponse(dbusconn,msg);
			}else if (dbus_message_is_signal(msg, "org.freedesktop.portal.Clipboard", "SelectionOwnerChanged")) {
				dbusHandleClipboardSelectionOwnerChanged(dbusconn,msg);
			}else if (dbus_message_is_signal(msg, "org.freedesktop.portal.Clipboard", "SelectionTransfer")) {
				dbusHandleClipboardSelectionTransfer(dbusconn,msg);
			}
			dbus_message_unref(msg);
		}
    }
    dbus_bus_remove_match(dbusconn, "type='signal',sender='org.freedesktop.portal.Desktop',interface='org.freedesktop.portal.Clipboard'", NULL);
    dbus_bus_remove_match(dbusconn, "type='signal',sender='org.freedesktop.portal.Desktop',interface='org.freedesktop.portal.Request'", NULL);
    return NULL;
}

void onProcessFrame(void* userdata) {
	DBusMonitorInfo* m = static_cast<DBusMonitorInfo*>(userdata);
	pw_buffer* buf = pw_stream_dequeue_buffer(m->pwstream);
	if (!buf) {
        return;
    }
    tdlock.lock();
	struct spa_buffer *spa_buf = buf->buffer;
	//struct spa_meta_header *spa_header = (struct spa_meta_header *)spa_buffer_find_meta(spa_buf, SPA_META_Header);

	//CURSOR
	struct spa_meta *spa_cursor = spa_buffer_find_meta(spa_buf, SPA_META_Cursor);
	if (spa_cursor) {
		struct spa_meta_cursor *cursor = (struct spa_meta_cursor *)spa_cursor->data;
		if (spa_meta_cursor_is_valid(cursor)) {
			cursorX=m->x+cursor->position.x;
			cursorY=m->y+cursor->position.y;
			if (cursor->bitmap_offset>0){
				struct spa_meta_bitmap *bitmap = SPA_MEMBER(cursor, cursor->bitmap_offset, struct spa_meta_bitmap);
				if (bitmap){
					int iformat = 0; //0 none; 1 RGB; 2 BGR
					int palfa = 0; //0 none; 1 begin; 2 end
					switch (bitmap->format) {
						case SPA_VIDEO_FORMAT_RGBA:
							iformat=1;
							palfa=2;
							break;
						case SPA_VIDEO_FORMAT_BGRA:
							iformat=2;
							palfa=2;
							break;
						case SPA_VIDEO_FORMAT_ARGB:
							iformat=1;
							palfa=1;
							break;
						case SPA_VIDEO_FORMAT_ABGR:
							iformat=2;
							palfa=1;
							break;
					}
					if (iformat>0){
						cursorChanged=true;
						cursorHotspotX=cursor->hotspot.x;
						cursorHotspotY=cursor->hotspot.y;
						cursorWidth=bitmap->size.width;
						cursorHeight=bitmap->size.height;
						if (cursorData){
							free(cursorData);
						}
						cursorDataSize=bitmap->size.width * bitmap->size.height * 4;
						cursorData=(unsigned char*)malloc(cursorDataSize);
						uint32_t w = bitmap->size.width;
						uint32_t h = bitmap->size.height;
						const uint8_t *src_data = SPA_MEMBER(bitmap, bitmap->offset, uint8_t);
						for (uint32_t y = 0; y < h; y++) {
							for (uint32_t x = 0; x < w; x++) {
								const uint32_t btsrc = y * bitmap->stride + x * 4;
								const uint32_t btdst = (y * (w) + x) * 4;
								if (palfa==1){
									if (iformat==0){//ARGB
										cursorData[btdst + 0] = src_data[btsrc + 1];
										cursorData[btdst + 1] = src_data[btsrc + 2];
										cursorData[btdst + 2] = src_data[btsrc + 3];
										cursorData[btdst + 3] = src_data[btsrc + 0];
									}else if (iformat==1){//ABRG
										cursorData[btdst + 0] = src_data[btsrc + 3];
										cursorData[btdst + 1] = src_data[btsrc + 2];
										cursorData[btdst + 2] = src_data[btsrc + 1];
										cursorData[btdst + 3] = src_data[btsrc + 0];
									}
								}else if (palfa==2){
									if (iformat==0){//RGBA
										memcpy(&cursorData[btdst], &src_data[btsrc], 4);
									}else if (iformat==1){//BRGA
										cursorData[btdst + 0] = src_data[btsrc + 2];
										cursorData[btdst + 1] = src_data[btsrc + 1];
										cursorData[btdst + 2] = src_data[btsrc + 0];
										cursorData[btdst + 3] = src_data[btsrc + 3];
									}
								}
							}
						}
					}
				}
			}
			//if (spa_meta_cursor_is_visible(cursor)) {
			//}
		}
	}

	//FRAME
	bool bok=true;
	struct spa_data* spa_data = NULL;
	if (!spa_buf || spa_buf->n_datas < 1){
		bok=false;
	}else{
		spa_data = &spa_buf->datas[0];
		if (!spa_data->data || !spa_data->chunk || spa_data->chunk->size == 0 || (spa_data->chunk->flags & SPA_CHUNK_FLAG_CORRUPTED)){
			bok=false;
		}
	}
	if (bok) {
		int sz = spa_data->chunk->size;
		int bpr = spa_data->chunk->stride;
		if (m->pwstream!=NULL){
			/*if (spa_header) {
				spa_header->pts>m->lastFramePts
			}
			if (bok){*/
			if (m->lastFrameSize!=sz){
				if (m->lastFrameSize>0){
					free(m->lastFrameData);
				}
				m->lastFrameData=(unsigned char *)malloc(sz);
				m->lastFrameSize=sz;
			}
			m->lastFrameBPR=bpr;
			if (spa_data->type == SPA_DATA_MemPtr) {
				memcpy(m->lastFrameData, (uint8_t*)spa_data->data+spa_data->chunk->offset, sz);
			}else if ((spa_data->type == SPA_DATA_MemFd) || (spa_data->type == SPA_DATA_DmaBuf))  {
				int fd = spa_data->fd;
				void *mapped_data = mmap(NULL, spa_data->maxsize, PROT_READ, MAP_SHARED, fd, spa_data->mapoffset);
				if (mapped_data != MAP_FAILED) {
					memcpy(m->lastFrameData, (uint8_t*)mapped_data+spa_data->chunk->offset, sz);
					munmap(mapped_data, spa_data->maxsize);
				}else{
					bok=false;
				}
			}else{
				bok=false;
			}
			if (!bok){
				if (m->lastFrameSize>0){
					free(m->lastFrameData);
				}
				m->lastFrameSize=0;
				m->lastFrameData=NULL;
			}

			//std::cerr << "onProcessFrame node: " << m->nodeid << "   sz: " << m->lastFrameSize << "   m->lastFrameSize: " << sz << std::endl;

			//}
		}
	}
	tdlock.unlock();
	pw_stream_queue_buffer(m->pwstream, buf);
}

void onStateChanged(void* userdata, pw_stream_state old, pw_stream_state state, const char* error) {
	//std::cout << "Status stream: " << pw_stream_state_as_string(state) << std::endl;
    if (state == PW_STREAM_STATE_ERROR) {
        std::cerr << "Error stream: " << error << std::endl;
    }
}

void onParamChanged(void *userData, uint32_t id, const struct spa_pod *param) {
	//TODO DETECT MONITORS CHANGES

	/*
	struct spa_video_info_raw info;
    if (id != SPA_PARAM_Format || param == NULL)
        return;
    if (spa_format_video_raw_parse(param, &info) < 0)
        return;
    //info.format; //SPA_VIDEO_FORMAT_BGRA SPA_VIDEO_FORMAT_BGRx SPA_VIDEO_FORMAT_RGBA
    //info.size.width;
    //info.size.height;
    std::cerr << "onParamChanged: " << info.size.width << " " << info.size.height << std::endl;
    */
}


static void onRegistryGlobal(void *data, uint32_t id,
                                uint32_t permissions, const char *type,
                                uint32_t version, const struct spa_dict *props){
	//TODO DETECT MONITORS CHANGES
}

void onRegistryGlobalRemove(void *data, uint32_t id){
	//TODO DETECT MONITORS CHANGES

	/*
	bool bclose=false;
	tdlock.lock();
	for (size_t i = 0; i < dbusmonitors.size(); i++) {
		DBusMonitorInfo* m = dbusmonitors[i];
		if (m->nodeid==id){
			bclose=true;
		}
	}
	tdlock.unlock();
	if (bclose){
		pw_main_loop_quit(pwloop);
	}
	*/

	/*
	tdlock.lock();
	for (size_t i = 0; i < dbusmonitors.size(); i++) {
		DBusMonitorInfo* m = dbusmonitors[i];
		if (m->nodeid==id){
			if (m->pwstream!=NULL){
				pw_stream_destroy(m->pwstream);
				m->pwstream=NULL;
			}
			if (m->lastFrameSize>0){
				free(m->lastFrameData);
				m->lastFrameSize=0;
			}
			dbusmonitors.erase(dbusmonitors.begin()+i);
			delete m;
			monchanged=true;
			break;
		}
	}
	tdlock.unlock();
	*/
}

void dbusClearMonitorNotSync(){
	for (size_t i = 0; i < dbusmonitors.size(); i++) {
		DBusMonitorInfo* m = dbusmonitors[i];
		if (m->pwstream!=NULL){
			pw_stream_destroy(m->pwstream);
			m->pwstream=NULL;
		}
		if (m->lastFrameSize>0){
			free(m->lastFrameData);
			m->lastFrameSize=0;
		}
		delete m;
	}
	dbusmonitors.clear();
	monchanged=false;
}

void pwStreamDestroyMonitors(){
	for (size_t i = 0; i < dbusmonitors.size(); i++) {
		DBusMonitorInfo* m = dbusmonitors[i];
		if (m->pwstream!=NULL){
			pw_stream_destroy(m->pwstream);
			m->pwstream=NULL;
		}
	}
}

static const struct pw_registry_events RegistryEvents = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = onRegistryGlobal,
    .global_remove = onRegistryGlobalRemove,
};

const struct pw_stream_events StreamEvents = {
    .version = PW_VERSION_STREAM_EVENTS,
	.state_changed = onStateChanged,
    .param_changed = onParamChanged,
    .process = onProcessFrame,
};

string startCapture(){
	string sret="";
	pw_init(NULL, NULL);
	pwloop = pw_main_loop_new(NULL);
	if (!pwloop) {
		sret="Unable to create main loop.";
		return sret;
	}
	pw_context *pwcontext = pw_context_new(pw_main_loop_get_loop(pwloop), NULL, 0);
	if (!pwcontext) {
		sret="Unable to create PipeWire context.";
		pw_main_loop_destroy(pwloop);
		return sret;
	}
	pw_core *pwcore = pw_context_connect(pwcontext, NULL, 0);
	if (!pwcore) {
		sret="Unable to connect to PipeWire.";
		pw_context_destroy(pwcontext);
		pw_main_loop_destroy(pwloop);
		return sret;
	}


	pw_registry *pwregistry = pw_core_get_registry(pwcore, PW_VERSION_REGISTRY, 0);
	spa_hook registryListener;
	pw_registry_add_listener(pwregistry, &registryListener, &RegistryEvents, NULL);


	uint32_t n_params=1;
	if (dbusrequest.versionScreenCast>=2){
		if (dbusrequest.availableCursorModesScreenCast & 4) {
			n_params++;
		}
	}
	const spa_pod* params[n_params];
	uint8_t spa_pod_builder_data[1024];
	struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(spa_pod_builder_data, sizeof(spa_pod_builder_data));

	params[0] = static_cast<const struct spa_pod*>(spa_pod_builder_add_object(&builder,
		SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
		SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
		SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
		SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(3,
			SPA_VIDEO_FORMAT_BGRx,
			SPA_VIDEO_FORMAT_BGRA,
			SPA_VIDEO_FORMAT_BGRA),
		0));



	if (dbusrequest.versionScreenCast>=2){
		if (dbusrequest.availableCursorModesScreenCast & 4) {
			params[1] = (struct spa_pod *)spa_pod_builder_add_object(
					&builder,
					SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
					SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Cursor)
				);
		}
	}

	/*
	params[1] = static_cast<const struct spa_pod*>(spa_pod_builder_add_object(&builder,
		 SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
		 SPA_PARAM_META_type, SPA_POD_Id      (SPA_META_Header),
		 SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_header))));
	*/


	for (size_t i = 0; i < dbusmonitors.size(); i++) {
		DBusMonitorInfo* m = dbusmonitors[i];
		std::string nmstream = "dwagent-capture-" + std::to_string(m->nodeid);
		m->pwstream=pw_stream_new(pwcore, nmstream.c_str(),
								pw_properties_new(
									PW_KEY_MEDIA_TYPE, "Video",
									PW_KEY_MEDIA_CATEGORY, "Capture",
									PW_KEY_MEDIA_ROLE, "Screen",
									NULL));
		if (!m->pwstream) {
			sret="Unable to create stream.";
			pwStreamDestroyMonitors();
			pw_core_disconnect(pwcore);
			pw_context_destroy(pwcontext);
			pw_main_loop_destroy(pwloop);
			return sret;
		}
		pw_stream_add_listener(m->pwstream, &m->streamlistener, &StreamEvents, m);
		int iret = pw_stream_connect(
			m->pwstream,
			PW_DIRECTION_INPUT,
			m->nodeid,
			static_cast<enum pw_stream_flags>(
				PW_STREAM_FLAG_AUTOCONNECT |
				PW_STREAM_FLAG_MAP_BUFFERS
			),
			params,
			n_params
		);
		if (iret < 0) {
			sret="Unable to connect to stream: " + to_string(iret) + ".";
			pwStreamDestroyMonitors();
			pw_core_disconnect(pwcore);
			pw_context_destroy(pwcontext);
			pw_main_loop_destroy(pwloop);
			return sret;
		}
	}

	//std::cerr << "Start capture" << std::endl;
	tdstatusSet(STATUS_CAPTURE);
	pw_main_loop_run(pwloop);
	//std::cerr << "End capture" << std::endl;

	pwStreamDestroyMonitors();
	pw_core_disconnect(pwcore);
	pw_context_destroy(pwcontext);
	pw_main_loop_destroy(pwloop);
	return sret;
}

void *captureThread(void *arg) {
	//std::cerr << "Start captureThread" << std::endl;
	while (true){
		int st=tdstatusGet();
		if ((st==STATUS_ERROR) || (st==STATUS_EXIT)){
			break;
		}else if (st==STATUS_NONE){
			tdstatusSet(STATUS_PERMISSION);
			string sret=dbusAddRequestAndWait("permission",NULL);
			if (sret==""){
				sret=startCapture();
				tdlock.lock();
				dbusClearMonitorNotSync();
				if (tdstatus!=STATUS_EXIT){
					if (sret==""){
						tdstatus=STATUS_NONE;
					}else{
						tdstatus=STATUS_ERROR;
						tderror=sret;
					}
				}
				tdlock.unlock();
			}else{
				tdlock.lock();
				dbusClearMonitorNotSync();
				if (tdstatus!=STATUS_EXIT){
					tdstatus=STATUS_ERROR;
					tderror=sret;
				}
				tdlock.unlock();
			}
		}
	}
	//std::cerr << "END captureThread" << std::endl;
	return NULL;
}

int DWAScreenCaptureGetPermissionToken(char* bf, int sz){
	tdlock.lock();
	int iret=dbusrequest.restoreToken.length();
	if (iret>0){
		if (iret>sz){
			iret=sz;
		}
		strncpy(bf, dbusrequest.restoreToken.c_str(), iret);
	}
	tdlock.unlock();
	return iret;
}

void DWAScreenCaptureSetPermissionToken(char* bf, int sz){
	tdlock.lock();
	if (sz>0){
		dbusrequest.restoreToken=string(bf, sz);
	}else{
		dbusrequest.restoreToken="";
	}
	tdlock.unlock();
}

int DWAScreenCaptureGetCpuUsage(){
    return (int)cpuUsage->getValue();
}

void DWAScreenCaptureFreeMemory(void* pnt){
	free(pnt);
}

int DWAScreenCaptureIsChanged(){
	int iret=0;
	tdlock.lock();
	int st=tdstatus;
	if (dbusrequest.restoreTokenChanged){
		iret=3; //PERMISSION TOKEN
		dbusrequest.restoreTokenChanged=false;
	}else if (monchanged){
		monchanged=false;
		iret=1;
	}
	tdlock.unlock();
	if (iret==3){
		return 3;
	}else if (st == STATUS_CAPTURE){
		return iret;
	}else if (st == STATUS_ERROR){
		return 4;
	}
	return 2;
}

int DWAScreenCaptureErrorMessage(char* bf, int sz){
	int iret=tderror.length();
	if (iret>0){
		if (iret>sz){
			iret=sz;
		}
		strncpy(bf, tderror.c_str(), iret);
	}
	return iret;
}

int DWAScreenCaptureInitMonitor(MONITORS_INFO_ITEM* moninfoitem, RGB_IMAGE* capimage, void** capses){
	ScreenCaptureInfo* sci = new ScreenCaptureInfo();
	sci->dbusmonitor=(DBusMonitorInfo*)moninfoitem->internal;
	sci->x=moninfoitem->x;
	sci->y=moninfoitem->y;
	sci->w=moninfoitem->width;
	sci->h=moninfoitem->height;
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
	delete sci;
}

int DWAScreenCaptureGetImage(void* capses){
	ScreenCaptureInfo* sci = (ScreenCaptureInfo*)capses;
	if (sci->status==0){
		return -1; //NOT INIT
	}
	int szdata=0;
	unsigned char *data;
	tdlock.lock();
	DBusMonitorInfo* m = sci->dbusmonitor;
	if (m->lastFrameSize>0){
		szdata=m->lastFrameSize;
		data=m->lastFrameData;
		m->lastFrameSize=0;
		m->lastFrameData=NULL;
	}
	tdlock.unlock();
	RGB_IMAGE* rgbimage=sci->rgbimage;
	rgbimage->sizechangearea=0;
	rgbimage->sizemovearea=0;
	if (szdata>0){
		int imgw = m->width;
		int imgh = m->height;
		int bpr = m->lastFrameBPR;
		int offsetSrc = 0;
		int offsetDst = 0;
		int rowOffset = bpr % imgw;
		for (int row = 0; row < imgh; ++row){
			for (int col = 0; col < imgw; ++col){
				unsigned char r=0;
				unsigned char g=0;
				unsigned char b=0;
				r = data[offsetSrc+2];
				g = data[offsetSrc+1];
				b = data[offsetSrc];
				offsetSrc += 4;
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
		free(data);
	}
	sci->status=2;
	return 0;
}

int DWAScreenCaptureCursor(CURSOR_IMAGE* curimage){
	tdlock.lock();
	curimage->changed=0;
	if (cursorChanged){
		cursorChanged=false;
		if (cursorDataSize>0){
			curimage->changed=1;
			curimage->offx=cursorHotspotX;
			curimage->offy=cursorHotspotY;
			curimage->width=cursorWidth;
			curimage->height=cursorHeight;
			curimage->sizedata=cursorDataSize;
			if (curimage->data!=NULL){
				free(curimage->data);
			}
			curimage->data = cursorData;
			cursorData = NULL;
			cursorDataSize = 0;
		}
	}else if (curimage->data==NULL){
		curimage->changed=1;
		setCursorImage(CURSOR_TYPE_ARROW_18_18,curimage);
	}
	curimage->visible=1;
	curimage->x=cursorX;
	curimage->y=cursorY;
	tdlock.unlock();
	return 0;
}

int32_t getEvdevKeyCode(const char* key) {
	if (strcmp(key,"CONTROL")==0){
		return XK_Control_L;
	}else if (strcmp(key,"LCONTROL")==0){
		return XK_Control_L;
	}else if (strcmp(key,"RCONTROL")==0){
		return XK_Control_R;
	}else if (strcmp(key,"ALT")==0){
		return XK_Alt_L;
	}else if (strcmp(key,"LALT")==0){
		return XK_Alt_L;
	}else if (strcmp(key,"RALT")==0){
		return XK_Alt_R;
	}else if (strcmp(key,"SHIFT")==0){
		return XK_Shift_L;
	}else if (strcmp(key,"LSHIFT")==0){
		return XK_Shift_L;
	}else if (strcmp(key,"RSHIFT")==0){
		return XK_Shift_R;
	}else if (strcmp(key,"TAB")==0){
		return XK_Tab;
	}else if (strcmp(key,"ENTER")==0){
		return XK_Return;
	}else if (strcmp(key,"BACKSPACE")==0){
		return XK_BackSpace;
	}else if (strcmp(key,"CLEAR")==0){
		return XK_Clear;
	}else if (strcmp(key,"PAUSE")==0){
		return XK_Pause;
	}else if (strcmp(key,"ESCAPE")==0){
		return XK_Escape;
	}else if (strcmp(key,"SPACE")==0){
		return XK_space;
	}else if (strcmp(key,"DELETE")==0){
		return XK_Delete;
	}else if (strcmp(key,"INSERT")==0){
		return XK_Insert;
	}else if (strcmp(key,"HELP")==0){
		return XK_Help;
	}else if (strcmp(key,"LSUPER")==0){
		return XK_Super_L;
	}else if (strcmp(key,"RSUPER")==0){
		return XK_Super_R;
	}else if (strcmp(key,"SELECT")==0){
		return XK_Select;
	}else if (strcmp(key,"PAGE_UP")==0){
		return XK_Page_Up;
	}else if (strcmp(key,"PAGE_DOWN")==0){
		return XK_Page_Down;
	}else if (strcmp(key,"END")==0){
		return XK_End;
	}else if (strcmp(key,"HOME")==0){
		return XK_Home;
	}else if (strcmp(key,"LEFT_ARROW")==0){
		return XK_Left;
	}else if (strcmp(key,"UP_ARROW")==0){
		return XK_Up;
	}else if (strcmp(key,"DOWN_ARROW")==0){
		return XK_Down;
	}else if (strcmp(key,"RIGHT_ARROW")==0){
		return XK_Right;
	}else if (strcmp(key,"F1")==0){
		return XK_F1;
	}else if (strcmp(key,"F2")==0){
		return XK_F2;
	}else if (strcmp(key,"F3")==0){
		return XK_F3;
	}else if (strcmp(key,"F4")==0){
		return XK_F4;
	}else if (strcmp(key,"F5")==0){
		return XK_F5;
	}else if (strcmp(key,"F6")==0){
		return XK_F6;
	}else if (strcmp(key,"F7")==0){
		return XK_F7;
	}else if (strcmp(key,"F8")==0){
		return XK_F8;
	}else if (strcmp(key,"F9")==0){
		return XK_F9;
	}else if (strcmp(key,"F10")==0){
		return XK_F10;
	}else if (strcmp(key,"F11")==0){
		return XK_F11;
	}else if (strcmp(key,"F12")==0){
		return XK_F12;
	}else if (strlen(key) == 1) {
		char c = key[0];
		if (c >= 'A' && c <= 'Z') {
			c += 32;
		}
		return c;
	}
	return 0;
}

void ctrlaltshift(bool ctrl, bool alt, bool shift, bool command){

	if ((command) && (!commandDown)){
		commandDown=true;
		int32_t ikey=getEvdevKeyCode("LSUPER");
		dbusAppendInputKeyboardSym(ikey,1,0);
	}else if ((!command) && (commandDown)){
		commandDown=false;
		int32_t ikey=getEvdevKeyCode("LSUPER");
		dbusAppendInputKeyboardSym(ikey,0,0);
	}

	if ((ctrl) && (!ctrlDown)){
		ctrlDown=true;
		int32_t ikey=getEvdevKeyCode("LCONTROL");
		dbusAppendInputKeyboardSym(ikey,1,0);
	}else if ((!ctrl) && (ctrlDown)){
		ctrlDown=false;
		int32_t ikey=getEvdevKeyCode("LCONTROL");
		dbusAppendInputKeyboardSym(ikey,0,0);
	}

	if ((alt) && (!altDown)){
		altDown=true;
		int32_t ikey=getEvdevKeyCode("LALT");
		dbusAppendInputKeyboardSym(ikey,1,0);
	}else if ((!alt) && (altDown)){
		altDown=false;
		int32_t ikey=getEvdevKeyCode("LALT");
		dbusAppendInputKeyboardSym(ikey,0,0);
	}

	if ((shift) && (!shiftDown)){
		shiftDown=true;
		int32_t ikey=getEvdevKeyCode("LSHIFT");
		dbusAppendInputKeyboardSym(ikey,1,0);
	}else if ((!shift) && (shiftDown)){
		shiftDown=false;
		int32_t ikey=getEvdevKeyCode("LSHIFT");
		dbusAppendInputKeyboardSym(ikey,0,0);
	}
}

void DWAScreenCaptureInputKeyboard(const char* type, const char* key, bool ctrl, bool alt, bool shift, bool command){
	if (strcmp(type,"CHAR")==0){
		int32_t ikey=atoi(key);
		dbusAppendInputKeyboardSym(ikey,1,0);
		usleep(10000);
		dbusAppendInputKeyboardSym(ikey,0,0);
	}else if (strcmp(type,"KEY")==0){
		int32_t ikey=getEvdevKeyCode(key);
		//std::cerr << "key: " << key << "  ikey: " << ikey << std::endl;
		if (ikey!=0){
			ctrlaltshift(ctrl,alt,shift,command);
			usleep(10000);
			dbusAppendInputKeyboardSym(ikey,1,0);
			usleep(10000);
			dbusAppendInputKeyboardSym(ikey,0,0);
			usleep(10000);
			ctrlaltshift(false,false,false,false);
		}
	}else if (strcmp(type,"CTRLALTCANC")==0){

	}
}

void DWAScreenCaptureInputMouse(MONITORS_INFO_ITEM* moninfoitem, int x, int y, int button, int wheel, bool ctrl, bool alt, bool shift, bool command){
	ctrlaltshift(ctrl,alt,shift,command);
	if ((x!=-1) && (y!=-1)){
		DBusMonitorInfo* m = (DBusMonitorInfo*)moninfoitem->internal;
		dbusAppendInputMouseMove(m, x, y, 10000);
	}
	if (button==64) { //CLICK
		dbusAppendInputMouseButton(mousebtn1Code, 1, 10000);
		dbusAppendInputMouseButton(mousebtn1Code, 0, 0);
	}else if (button==128) { //DBLCLICK
		dbusAppendInputMouseButton(mousebtn1Code, 1, 10000);
		dbusAppendInputMouseButton(mousebtn1Code, 0, 200000);
		dbusAppendInputMouseButton(mousebtn1Code, 1, 10000);
		dbusAppendInputMouseButton(mousebtn1Code, 0, 0);
	}else if (button!=-1) {
		int appbtn=-1;
		if ((button & 1) && (mousebtn1Down==0)){
			appbtn=mousebtn1Code;
			mousebtn1Down=1;
		}else if (mousebtn1Down==1){
			appbtn=mousebtn1Code;
			mousebtn1Down=0;
		}
		if (appbtn!=-1){
			dbusAppendInputMouseButton(appbtn, mousebtn1Down, 0);
		}
		appbtn=-1;
		if ((button & 2) && (!mousebtn2Down)){
			appbtn=mousebtn2Code;
			mousebtn2Down=true;
		}else if (mousebtn2Down){
			appbtn=mousebtn2Code;
			mousebtn2Down=false;
		}
		if (appbtn!=-1){
			dbusAppendInputMouseButton(appbtn, mousebtn2Down, 0);
		}
		appbtn=-1;
		if ((button & 4) && (!mousebtn3Down)){
			appbtn=mousebtn3Code;
			mousebtn3Down=true;
		}else if (mousebtn3Down){
			appbtn=mousebtn3Code;
			mousebtn3Down=false;
		}
		if (appbtn!=-1){
			dbusAppendInputMouseButton(appbtn, mousebtn3Down, 0);
		}
	}
	if (wheel>0){
		dbusAppendInputMouseAxis(0,-1,0);
	}else if (wheel<0){
		dbusAppendInputMouseAxis(0,1,0);
	}
}

void DWAScreenCaptureCopy(){
	DWAScreenCaptureInputKeyboard("KEY","C",true,false,false,false);
	bool bexit=false;
	for (int i=1;i<=10;i++){
		usleep(50000);
		tdlock.lock();
		if (dbusclipboard.changed){
			bexit=true;
		}
		tdlock.unlock();
		if (bexit){
			break;
		}
	}
}

void DWAScreenCapturePaste(){
	DWAScreenCaptureInputKeyboard("KEY","V",true,false,false,false);
}

void DWAScreenCaptureGetClipboardChanges(CLIPBOARD_DATA* clipboardData){
	clipboardData->type=0;
	bool bchange=false;
	tdlock.lock();
	if ((dbusrequest.clipboard) && (dbusclipboard.changed)){
		dbusclipboard.changed=false;
		bchange=true;
	}
	tdlock.unlock();
	if (bchange){
		dbusAddRequestAndWait("getclipboard",clipboardData);
	}
}

void DWAScreenCaptureSetClipboard(CLIPBOARD_DATA* clipboardData){
	bool bok=false;
	tdlock.lock();
	bok=dbusrequest.clipboard;
	tdlock.unlock();
	if (bok){
		dbusAddRequestAndWait("setclipboard",clipboardData);
	}
}

bool DWAScreenCaptureLoad() {
	ctrlDown=false;
	altDown=false;
	shiftDown=false;
	commandDown=false;
	mousebtn1Code=272;
	mousebtn2Code=273;
	mousebtn3Code=274;
	mousebtn1Down=0;
	mousebtn2Down=0;
	mousebtn3Down=0;
	cursorX=0;
	cursorY=0;
	cursorHotspotX=0;
	cursorHotspotY=0;
	cursorWidth=0;
	cursorHeight=0;
	cursorChanged=false;
	cursorData=NULL;
	cursorDataSize=0;
	dbusClearRequest();
	dbusrequest.cnttoken=0;
	dbusrequest.clipboard=false;
	dbusrequest.restoreToken="";
	dbusrequest.restoreTokenChanged=false;
	dbusClipboardReady();
	dbusclipboard.changed=false;
	dbusclipboard.sizedata=0;
	dbusclipboard.data=NULL;
	DBusError error;
	dbus_error_init(&error);
	dbusconn = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (dbus_error_is_set(&error)) {
		dbusconn = NULL;
		std::cerr << "D-Bus Error: " << error.message << std::endl;
		dbus_error_free(&error);
	}
	cpuUsage=new LinuxCPUUsage();
	tdstatusSet(STATUS_NONE);
	monchanged=false;
	pthread_create(&tddbus, NULL, dbusThread, NULL);
	pthread_create(&tdcapture, NULL, captureThread, NULL);
	return true;
}

void DWAScreenCaptureUnload() {
	int curst=tdstatusSet(STATUS_EXIT);
	dbusCompleteRequest("Capture unloading.");
	if (curst==STATUS_CAPTURE){
		pw_main_loop_quit(pwloop);
	}
	tdjoinwait(tdcapture,5);
	delete cpuUsage;

	dbusclose=true;
	tdjoinwait(tddbus,5);
	if (dbusconn != NULL) {
		dbus_connection_unref(dbusconn);
		dbusconn = NULL;
	}
	if (dbusclipboard.sizedata>0){
		free(dbusclipboard.data);
		dbusclipboard.data=NULL;
		dbusclipboard.sizedata=0;
	}
	if (cursorData){
		free(cursorData);
		cursorData=NULL;
		cursorDataSize=0;
	}
}

int clearMonitorsInfo(MONITORS_INFO* moninfo){
	moninfo->changed=0;
	for (int i=0;i<=MONITORS_INFO_ITEM_MAX-1;i++){
		moninfo->monitor[i].changed=-1;
		moninfo->monitor[i].internal=NULL;
	}
	for (int i=0;i<=moninfo->count-1;i++){
		moninfo->monitor[i].changed=0;
	}
	int oldmc=moninfo->count;
	moninfo->count=0;
	return oldmc;
}

void addMonitorsInfo(MONITORS_INFO* moninfo, DBusMonitorInfo* dbusmon){
	int x=dbusmon->x;
	int y=dbusmon->y;
	int w=dbusmon->width;
	int h=dbusmon->height;
	int p=moninfo->count;
	moninfo->count+=1;
	moninfo->monitor[p].internal=dbusmon;
	if (moninfo->monitor[p].changed==-1){
		moninfo->monitor[p].index=p;
		moninfo->monitor[p].x=x;
		moninfo->monitor[p].y=y;
		moninfo->monitor[p].width=w;
		moninfo->monitor[p].height=h;
		moninfo->monitor[p].changed=1;
		moninfo->changed=1;
	}else{
		if ((moninfo->monitor[p].x!=x) || (moninfo->monitor[p].y!=y) || (moninfo->monitor[p].width!=w) || (moninfo->monitor[p].height!=h)){
			moninfo->monitor[p].index=p;
			moninfo->monitor[p].x=x;
			moninfo->monitor[p].y=y;
			moninfo->monitor[p].width=w;
			moninfo->monitor[p].height=h;
			moninfo->monitor[p].changed=1;
			moninfo->changed=1;
		}else{
			moninfo->monitor[p].changed=0;
		}
	}
}

int DWAScreenCaptureGetMonitorsInfo(MONITORS_INFO* moninfo){
	int iret=0;
	int oldmc=clearMonitorsInfo(moninfo);
	tdlock.lock();
	for (size_t i = 0; i < dbusmonitors.size(); i++) {
		DBusMonitorInfo* m = dbusmonitors[i];
	    addMonitorsInfo(moninfo,m);
	}
	tdlock.unlock();
	if (oldmc!=moninfo->count){
		moninfo->changed=1;
	}
	return iret;
}

#endif
