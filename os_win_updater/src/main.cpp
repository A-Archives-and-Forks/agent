/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "main.h"

using namespace std;

wstring workPath = L"";

CallbackType g_callback_wlog = NULL;

wchar_t* towchar_t(wstring& str) {
    wchar_t* apps = new wchar_t[str.size() + 1];
	wcscpy(apps, str.c_str());
    return apps;
}

wstring getDWAgentPath(){
	wchar_t strPathName[_MAX_PATH];
	GetModuleFileNameW(NULL, strPathName, _MAX_PATH);
	wstring newPath(strPathName);
	int fpos = newPath.find_last_of('\\');
	if (fpos != -1)
		newPath = newPath.substr(0,(fpos));
	fpos = newPath.find_last_of('\\');
	if (fpos != -1)
		newPath = newPath.substr(0,(fpos));
	return newPath;
}

void WriteToLog(const wchar_t* str) {
	if (g_callback_wlog!=NULL){
		g_callback_wlog(str);
	}
}

bool compareFileOLD(wchar_t* fn1,wchar_t* fn2) {
	int BUFFERSIZE=1024*16;
	HANDLE hFile1;
	DWORD  dwBytesRead1 = 0;
	char ReadBuffer1[1024*16];
	HANDLE hFile2;
	DWORD  dwBytesRead2 = 0;
	char ReadBuffer2[1024*16];

	hFile1 = CreateFileW(fn1,
					   GENERIC_READ,
					   FILE_SHARE_READ,
					   NULL,
					   OPEN_EXISTING,
					   FILE_ATTRIBUTE_NORMAL,
					   NULL);
	if (hFile1 == INVALID_HANDLE_VALUE) {
		return false;
	}
	hFile2 = CreateFileW(fn2,
						   GENERIC_READ,
						   FILE_SHARE_READ,
						   NULL,
						   OPEN_EXISTING,
						   FILE_ATTRIBUTE_NORMAL,
						   NULL);
	if (hFile2 == INVALID_HANDLE_VALUE) {
		CloseHandle(hFile1);
		return false;
	}
	bool bret = true;
	while(true){
		if(ReadFile(hFile1, ReadBuffer1, BUFFERSIZE, &dwBytesRead1, NULL)==FALSE){
			bret = false;
			break;
		}
		if(ReadFile(hFile2, ReadBuffer2, BUFFERSIZE, &dwBytesRead2, NULL)==FALSE){
			bret = false;
			break;
		}
		if ((dwBytesRead1 == 0) && (dwBytesRead2 == 0)){
			bret = true;
			break;
		}
		if ((dwBytesRead1 != dwBytesRead2) || dwBytesRead1==0 || dwBytesRead2==0){
			bret = false;
			break;
		}else{
			for (long unsigned int i=0;i<dwBytesRead1;i++){
				if (strcmp(&ReadBuffer1[i],&ReadBuffer2[i])!=0){
					bret = false;
					break;
				}
			}
		}
	}
	CloseHandle(hFile1);
	CloseHandle(hFile2);
	return bret;
}

bool existsFile(wstring fileName) {
	return GetFileAttributesW(fileName.c_str())!=INVALID_FILE_ATTRIBUTES;
}


BOOL existsDir(wstring file){
	DWORD returnvalue;
	returnvalue = GetFileAttributesW(file.c_str());
	if(returnvalue == ((DWORD)-1)){
		return false;
	}
	else{
		return true;
	}
}

bool deleteDir(const wchar_t *path){
	bool bret=true;
    WIN32_FIND_DATAW FindFileData;
    HANDLE hFind;
    DWORD Attributes;
    wchar_t str[MAX_PATH];
	wcscpy(str,path);
	wcscat(str,L"\\*.*");
    hFind = FindFirstFileW(str, &FindFileData);
    do{
        if (wcscmp(FindFileData.cFileName, L".") != 0 && wcscmp(FindFileData.cFileName, L"..") != 0)
        {
            wcscpy(str, path);
            wcscat(str,L"\\");
            wcscat (str,FindFileData.cFileName);
            Attributes = GetFileAttributesW(str);
			if (Attributes & FILE_ATTRIBUTE_DIRECTORY){
                if (!deleteDir(str)){
					bret=false;
					break;
				}
            }else{
				if (!DeleteFileW(str)){
					bret=false;
					break;
				}
            }
        }
    }while(FindNextFileW(hFind, &FindFileData));
    FindClose(hFind);
    RemoveDirectoryW(path);
    return bret;
}

bool updateFilesOLD(wstring dsub){
	bool bret=true;
	wstring dwkr=workPath;
	wstring dupd=workPath;
	dupd.append(L"\\update");
	WIN32_FIND_DATAW FindFileData;
    HANDLE hFind;
    DWORD Attributes;
    wchar_t strupd[MAX_PATH];
	wchar_t strwkr[MAX_PATH];
	wcscpy(strupd,dupd.c_str());
	if (wcscmp(dsub.c_str(),L"")!=0){;
		wcscat(strupd,L"\\");
		wcscat(strupd,dsub.c_str());
	}
    wcscat(strupd,L"\\*.*");
    hFind = FindFirstFileW(strupd, &FindFileData);
    do{
        if (wcscmp(FindFileData.cFileName, L".") != 0 && wcscmp(FindFileData.cFileName, L"..") != 0){
			wcscpy(strwkr, dwkr.c_str());
			if (wcscmp(dsub.c_str(),L"")!=0){
				wcscat(strwkr,L"\\");
				wcscat(strwkr,dsub.c_str());
			}
            wcscat(strwkr,L"\\");
            wcscat(strwkr,FindFileData.cFileName);

            wcscpy(strupd, dupd.c_str());
			if (wcscmp(dsub.c_str(),L"")!=0){
				wcscat(strupd,L"\\");
				wcscat(strupd,dsub.c_str());
			}
            wcscat(strupd,L"\\");
            wcscat(strupd,FindFileData.cFileName);

	        Attributes = GetFileAttributesW(strupd);
			if ((Attributes & FILE_ATTRIBUTE_DIRECTORY)){
				if (!existsDir(strwkr)){
					CreateDirectoryW(strwkr,NULL);
				}
				wstring dsubapp;
				if (wcscmp(dsub.c_str(),L"")!=0){;
					dsubapp.append(dsub);
					dsubapp.append(L"\\");
				}
				dsubapp.append(FindFileData.cFileName);
				if (!updateFilesOLD(dsubapp)){
					bret=false;
				}
            }else{
				if (existsFile(strwkr)){
					if (!DeleteFileW(strwkr)){
						wchar_t strmsg[1000];
						wcscpy(strmsg, L"ERROR:Coping file ");
						wcscat(strmsg, strupd);
						wcscat(strmsg, L" .");
						WriteToLog(strmsg);
						bret=false;
					}
				}
				if (!existsFile(strwkr)){
					if ((CopyFileW(strupd,strwkr,TRUE)!=0) && (compareFileOLD(strupd,strwkr))){
						wchar_t strmsg[1000];
						wcscpy(strmsg, L"Copied file ");
						wcscat(strmsg, strupd);
						wcscat(strmsg, L" .");
						WriteToLog(strmsg);
						DeleteFileW(strupd);
					}else{
						wchar_t strmsg[1000];
						wcscpy(strmsg, L"ERROR:Coping file ");
						wcscat(strmsg, strupd);
						wcscat(strmsg, L" .");
						WriteToLog(strmsg);
						bret=false;
					}
				}
			}
        }
    }while(FindNextFileW(hFind, &FindFileData));
    FindClose(hFind);
    return bret;
}

bool checkUpdateOLD(){
	workPath=getDWAgentPath();
	wstring dupd=workPath;
	if (updateFilesOLD(L"")){
		return deleteDir(dupd.c_str());
	}else{
		return false;
	}
}

void trim(wstring& str, wchar_t c) {
    string::size_type pos = str.find_last_not_of(c);
    if (pos != string::npos) {
        str.erase(pos + 1);
        pos = str.find_first_not_of(c);
        if (pos != string::npos) str.erase(0, pos);
    } else str.erase(str.begin(), str.end());
}

void trimAll(wstring& str) {
    trim(str, ' ');
    trim(str, '\r');
    trim(str, '\n');
    trim(str, '\t');
}

void setCallbackWriteLog(CallbackType callback){
	g_callback_wlog=callback;
}

bool checkUpdate(){
	workPath=getDWAgentPath();
	wstring dupd=workPath;
	dupd.append(L"\\update");
	if(existsDir(dupd)){
		wstring pythonPath = L"";
		wstring pythonHome = L"";
		wstring serviceName = L"";

		//GET PYTHON EXE PATH
		wstring appfn = workPath;
		appfn.append(L"\\native\\service.properties");
		HANDLE hFile = CreateFileW(appfn.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile!=INVALID_HANDLE_VALUE){
			DWORD  dwBytesRead;
			char buff[16*1024];
			ReadFile(hFile, buff, sizeof(buff), &dwBytesRead, NULL);
			CloseHandle(hFile);
			wstring apps;
			int numc = MultiByteToWideChar(CP_UTF8, 0, buff, dwBytesRead, NULL, 0);
			if (numc){
				wchar_t *wszTo = new wchar_t[numc + 1];
				wszTo[numc] = L'\0';
				MultiByteToWideChar(CP_UTF8, 0, buff, -1, wszTo, numc);
				apps = wszTo;
				delete[] wszTo;
			}
			int pel = 0;
			while(pel>=0){
				int prepel=pel;
				pel = apps.find(L"\n",prepel);
				wstring line;
				if (pel<0){
					line = apps.substr(prepel);
				}else{
					line = apps.substr(prepel, pel-prepel);
					pel++;
				}
				trimAll(line);

				//Legge le proprietà necessarie
				int endpart1 = line.find_first_of(L"=");
				wstring part1 = line.substr(0, endpart1);
				trimAll(part1);
				wstring part2 = line.substr(endpart1 + 1);
				trimAll(part2);

				if (part1.compare(L"serviceName") == 0) {
					serviceName = part2;
				}

				if (part1.compare(L"pythonHome") == 0) {
					pythonHome = part2;
				}

				if (part1.compare(L"pythonPath") == 0) {
					pythonPath = part2;
				}
			}
		}

		//UPDATE updater.py
		wchar_t updaternew[MAX_PATH];
		wcscpy(updaternew, dupd.c_str());
		wcscat(updaternew,L"\\updater.py");
		wchar_t updaterorig[MAX_PATH];
		wcscpy(updaterorig, workPath.c_str());
		wcscat(updaterorig,L"\\updater.py");

		if (existsFile(updaternew)){
			if (existsFile(updaterorig)){
				if (!DeleteFileW(updaterorig)){
					wchar_t strmsg[1000];
					wcscpy(strmsg, L"ERROR:Delete file ");
					wcscat(strmsg, updaterorig);
					wcscat(strmsg, L" .");
					WriteToLog(strmsg);
					return false;
				}
			}
			if (!(MoveFileW(updaternew,updaterorig))){
				wchar_t strmsg[1000];
				wcscpy(strmsg, L"ERROR:Move file ");
				wcscat(strmsg, updaternew);
				wcscat(strmsg, L" .");
				WriteToLog(strmsg);
				return false;
			}

		}

		//RUN updater.py
		if ((wcscmp(pythonPath.c_str(),L"")!=0) && existsFile(updaterorig)){
			wchar_t updatestatus[MAX_PATH];
			wcscpy(updatestatus, workPath.c_str());
			wcscat(updatestatus,L"\\updater.status");
			if (existsFile(updatestatus)){
				if (!DeleteFileW(updatestatus)){
					wchar_t strmsg[1000];
					wcscpy(strmsg, L"ERROR:Delete file ");
					wcscat(strmsg, updatestatus);
					wcscat(strmsg, L" .");
					WriteToLog(strmsg);
					return false;
				}
			}

			wstring args = L"\"";
			args.append(pythonPath);
			args.append(L"\" -S -m updater");

			STARTUPINFOW siStartupInfo;
			PROCESS_INFORMATION piProcessInfo;
			memset(&siStartupInfo, 0, sizeof (siStartupInfo));
			memset(&piProcessInfo, 0, sizeof (piProcessInfo));
			siStartupInfo.cb = sizeof (siStartupInfo);
			siStartupInfo.lpReserved=NULL;
			siStartupInfo.lpDesktop=NULL;
			wstring titName;
			titName.append(serviceName);
			titName.append(L"Upd");
			siStartupInfo.lpTitle=(LPWSTR)titName.c_str();
			siStartupInfo.dwX=0;
			siStartupInfo.dwY=0;
			siStartupInfo.dwXSize=0;
			siStartupInfo.dwYSize=0;
			siStartupInfo.dwXCountChars=0;
			siStartupInfo.dwYCountChars=0;
			siStartupInfo.dwFillAttribute=0;
			siStartupInfo.wShowWindow = SW_HIDE;
			siStartupInfo.dwFlags = STARTF_USESHOWWINDOW;

			SECURITY_ATTRIBUTES sa = {0};
			sa.nLength = sizeof (SECURITY_ATTRIBUTES);
			sa.bInheritHandle = FALSE;
			sa.lpSecurityDescriptor = NULL;

			if (pythonHome.compare(L"") != 0) {
				SetEnvironmentVariableW(TEXT(L"PYTHONHOME"),pythonHome.c_str());
			}

			if (CreateProcessW(NULL,
					towchar_t(args),
					NULL,
					NULL,
					sa.bInheritHandle,
					HIGH_PRIORITY_CLASS | CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT,
					NULL,
					towchar_t(workPath),
					&siStartupInfo,
					&piProcessInfo) == TRUE) {
				WaitForSingleObject(piProcessInfo.hProcess, INFINITE);
			}
			if (!existsFile(updatestatus) && existsDir(dupd)){
				deleteDir(dupd.c_str());
				wchar_t strmsg[1000];
				wcscpy(strmsg, L"Run updater.py failed.");
				WriteToLog(strmsg);
			}
			return true;
		}else{
			return checkUpdateOLD();
		}
    }
	return true;
}

int main(int argc, char** argv ){
	return 0;
}
