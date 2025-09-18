/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <iostream>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <cstdlib>
#include <libgen.h>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <dirent.h>
#include <syslog.h>
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <cstdio>
#include <sys/sysctl.h>

void writeError(const char* format, ...) {
	openlog("net.dwservice.DWAgent", LOG_PID | LOG_CONS, LOG_USER);
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    std::cerr << buffer << std::endl;
    vsyslog(LOG_ERR, buffer, args);
    closelog();
}

std::string getUserLocale() {
	std::string userLocale = "";
	CFLocaleRef locale = CFLocaleCopyCurrent();
	CFStringRef localeIdentifier = CFLocaleGetIdentifier(locale);
	const char* localeCStr = CFStringGetCStringPtr(localeIdentifier, kCFStringEncodingUTF8);
	if (localeCStr != NULL) {
		userLocale=std::string(localeCStr);
	} else {
		char buffer[256];
		if (CFStringGetCString(localeIdentifier, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
			userLocale=std::string(buffer);
		}
	}
	CFRelease(locale);
	return userLocale;
}

void setUserLocale(std::string userLocale) {
    if (!userLocale.empty()) {
        setenv("LC_ALL", userLocale.c_str(), 1);
        setenv("LC_CTYPE", userLocale.c_str(), 1);
        setenv("LC_TIME", userLocale.c_str(), 1);
        setenv("LC_NUMERIC", userLocale.c_str(), 1);
        setenv("LANG", userLocale.c_str(), 1);
    }
}

bool isMacOSVersionOrLater(int major, int minor) {
	NSOperatingSystemVersion osVersion;
	size_t sz;
	sysctlbyname("kern.osrelease", NULL, &sz, NULL, 0);	
	char *appc = (char *)malloc(sz);
	sysctlbyname("kern.osrelease", appc, &sz, NULL, 0);	
	NSString *sver = [NSString stringWithCString:appc encoding:NSUTF8StringEncoding];
	free(appc);
	NSArray *ar = [sver componentsSeparatedByString:@"."];
	NSInteger majorVersion = [ar[0] integerValue];
	NSInteger minorVersion = [ar[1] integerValue];
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
    if (osVersion.majorVersion > major) {
        return true;
    } else if (osVersion.majorVersion == major) {
        if (osVersion.minorVersion > minor) {
            return true;
        }
    }
    return false;
}

bool executeWithPrivileges(const char* path, char* const argscmd[]) {	
	if (isMacOSVersionOrLater(10, 11)) {
		AuthorizationRef authRef;
		OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authRef);
		if (status != errAuthorizationSuccess) {
			writeError("Error create authorization: %d", status);
			return false;
		}
		status = AuthorizationExecuteWithPrivileges(authRef, path, kAuthorizationFlagDefaults, argscmd, NULL);
		AuthorizationFree(authRef, kAuthorizationFlagDefaults);
		if (status != errAuthorizationSuccess) {
			writeError("Error execute command with privileges: %d", status);
			return false;
		}
		return true;
	}else{
		std::string scmd = std::string(path);
		for (int i = 0; argscmd[i] != NULL; ++i) {
			scmd += " ";
			scmd += argscmd[i];
		}
		std::string command = "osascript -e 'do shell script \"" + scmd + "\" with administrator privileges'";
		int r = system(command.c_str());
		return (r==0);
	}    
}

bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool removeFile(const std::string& path) {
    return (remove(path.c_str()) == 0);
}

bool copyFile(const std::string& source, const std::string& destination) {
    const size_t bufferSize = 65536;
    char buffer[bufferSize];
    FILE* src = std::fopen(source.c_str(), "rb");
    if (!src) {
        writeError("Error copyFile open source: %s", source.c_str());
        return false;
    }
    FILE* dst = std::fopen(destination.c_str(), "wb");
    if (!dst) {
        std::fclose(src);
        writeError("Error copyFile create destination: %s", destination.c_str());
        return false;
    }
    size_t bytesRead;
    while ((bytesRead = std::fread(buffer, 1, bufferSize, src)) > 0) {
        if (std::fwrite(buffer, 1, bytesRead, dst) != bytesRead) {
            std::fclose(src);
            std::fclose(dst);
            writeError("Error writing to destination file: %s", destination.c_str());
            return false;
        }
    }
    std::fclose(src);
    std::fclose(dst);
    return true;
}

bool deleteDirectory(const std::string& dirPath) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) {
    	writeError("Error deleteDirectory open: %s", dirPath.c_str());
        return false;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        std::string entryPath = dirPath + "/" + entry->d_name;
        struct stat entryStat;
        if (stat(entryPath.c_str(), &entryStat) == 0) {
            if (S_ISDIR(entryStat.st_mode)) {
                if (!deleteDirectory(entryPath)) {
                    closedir(dir);
                    return false;
                }
            } else {
                if (!removeFile(entryPath.c_str())) {
                	writeError("Error remove file: %s", entryPath.c_str());
                    closedir(dir);
                    return false;
                }
            }
        } else {
        	if (!removeFile(entryPath.c_str())) {
        		writeError("Error access file: %s", entryPath.c_str());
				closedir(dir);
				return false;
        	}
        }
    }
    closedir(dir);
    if (rmdir(dirPath.c_str()) != 0) {
    	writeError("Error remove directory: %s", dirPath.c_str());
        return false;
    }
    return true;
}

bool extract(const std::string& srcPath, const std::string& tempPath){
	//COPY FILES
	std::string sourceFile = srcPath + "/extract";
	std::string destinationFile = tempPath + "/extract";
	if (!copyFile(sourceFile, destinationFile)) {
		deleteDirectory(tempPath);
		return false;
	}

	sourceFile = srcPath + "/extract.7z";
	destinationFile = tempPath + "/extract.7z";
	if (!copyFile(sourceFile, destinationFile)) {
		deleteDirectory(tempPath);
		return false;
	}

	//RUN EXTRACT
	if (chdir(tempPath.c_str()) != 0) {
		writeError("Error change directory: %s", tempPath.c_str());
		deleteDirectory(tempPath);
		return false;
	}
	if (chmod("extract", 0755) != 0) {
		writeError("Error assign execute permission to extract");
		deleteDirectory(tempPath);
		return false;
	}

	pid_t pid = fork();
	if (pid == 0) {
		if (execl("./extract", "extract", (char*)NULL) == -1) {
			deleteDirectory(tempPath);
			return false;
		}
	} else if (pid < 0) {
		writeError("Error extract (fork)");
		deleteDirectory(tempPath);
		return false;
	} else {
		int status;
		waitpid(pid, &status, 0);
		if (status!=0){
			writeError("Error extract (status)");
			deleteDirectory(tempPath);
			return false;
		}
	}
	return true;
}

void fixHighSierra(const std::string& tempPath){
	std::string libzpath = tempPath + "/runtime/lib/libz.1.2.8.dylib";
	std::string libzOriginalPath = "/usr/lib/libz.1.2.8.dylib";
	if (fileExists(libzOriginalPath)) {
		if (!removeFile(libzpath)) {
			writeError("Error fixHighSierra remove file: %s", libzpath.c_str());
		}
		if (!copyFile(libzOriginalPath, libzpath)) {
			writeError("Error fixHighSierra copy file");
		}
	}
}

bool copyCustom(std::string srcPath, const std::string& tempPath) {
    std::string custompath = srcPath + "/Custom";
    if (fileExists(custompath)) {
    	DIR *dir = opendir(custompath.c_str());
		if (dir != NULL) {
			struct dirent *entry;
			while ((entry = readdir(dir)) != NULL) {
				if (std::string(entry->d_name) != "." && std::string(entry->d_name) != "..") {
					const std::string &fileName = entry->d_name;
					std::string srcPath = custompath + "/" + fileName;
					std::string dstPath = tempPath + "/" + fileName;
					if (!copyFile(srcPath, dstPath)){
						writeError("Error copyCustom path.");
						return false;
					}
				}
			}
			closedir(dir);
		}
    }
    return true;
}

int main(int argc, char* argv[]) {

	if (argc == 0) {
		writeError("Error arguments missing executable path.");
        return 1;
    }

	setUserLocale(getUserLocale());

    //DETECT PATHS
	char absolutePath[PATH_MAX];
    if (realpath(argv[0], absolutePath) == NULL) {
    	writeError("Error read executable path.");
    	return 1;
    }
    std::string exePath = std::string(absolutePath);
    size_t appPos = exePath.rfind(".app");
	if (appPos == std::string::npos) {
		writeError("Error read app path.");
		return 1;
	}
	appPos += 4;
	std::string appPath = exePath.substr(0, appPos);
    char pathCopy[PATH_MAX];
    strncpy(pathCopy, absolutePath, PATH_MAX);
    std::string srcPath = std::string(dirname(pathCopy));

    //CHECK IF INSTALLER
    std::string instPath = srcPath + "/installer";
	bool binstaller=fileExists(instPath);
	if (binstaller){
		bool bbegin=true;
		if (argc > 1 && argv[1] != NULL && strlen(argv[1]) > 0) {
			if (strcmp(argv[1], "runasadmin") == 0) {
				bbegin=false;
				/*
				std::string concatenatedArgs;
				for (int i = 2; i <= argc-1; i++) {
					concatenatedArgs += argv[i];
					if (i < argc - 1) {
						concatenatedArgs += " ";
					}
				}
				//std::string command = "osascript -e 'do shell script \"open -n -a " + appPath + " --args run " + concatenatedArgs + "\" with administrator privileges'";
				std::string command = "osascript -e 'do shell script \"/Volumes/DWAgent/DWAgent.app/Contents/MacOS/DWAgent run " + concatenatedArgs + "\" with administrator privileges'";
				int r = system(command.c_str());
				if (r!=0){
					std::string tempPath = std::string(argv[3]);
					deleteDirectory(tempPath);
					writeError("Error osascript with administrator privileges.");
				}
				return r;
				*/

				int numArgs = argc - 1;
				char* argscmd[numArgs + 2];
				argscmd[0]=(char*)"run";
				for (int i = 2; i <= argc-1; i++) {
					argscmd[i-1] = argv[i];
				}
				std::string userLocale = "LC=" + getUserLocale();
				argscmd[numArgs] = (char*)userLocale.c_str();
				argscmd[numArgs+1] = NULL;
				if (!executeWithPrivileges(exePath.c_str(),argscmd)){
					std::string tempPath = std::string(argv[3]);
					deleteDirectory(tempPath);
					return 1;
				}
			}else if (strcmp(argv[1], "run") == 0) {
				bbegin=false;
				std::string tempPath = std::string(argv[3]);
				if (chdir(tempPath.c_str()) != 0) {
					writeError("Error change directory: %s",tempPath.c_str());
					deleteDirectory(tempPath);
					return 1;
				}
				if (binstaller){
					char cmd_to_execute[4096];
					strcpy(cmd_to_execute,"");
					strcat(cmd_to_execute, srcPath.c_str());
					strcat(cmd_to_execute, "/installer");
					int numArgs = argc - 1;
					std::string userLocale = std::string(argv[argc-1]);
					if (userLocale.find("LC=") == 0) {
						setUserLocale(userLocale.substr(3));
						numArgs = argc - 2;
					}
					char* argscmd[numArgs + 2];
					argscmd[0] = cmd_to_execute;
					for (int i = 2; i <= argc-1; i++) {
						argscmd[i-1] = argv[i];
					}
					argscmd[numArgs+1] = NULL;
					if (execvp(argscmd[0], argscmd) == -1) {
						deleteDirectory(tempPath);
						return 1;
					}
				}
			}
		}

		if (bbegin){
			//CREATE TMP DIR
			const char* tempDir = getenv("TMPDIR");
			if (!tempDir) {
				writeError("Error get TMPDIR.");
				return 1;
			}
			std::time_t now = std::time(NULL);
			std::tm* localTime = std::localtime(&now);
			if (!localTime) {
				writeError("Error get current time.");
				return 1;
			}
			char timestamp[16];
			std::strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", localTime);
			std::string tempPath = std::string(tempDir) + "dwagentinstall" + timestamp;
			if (mkdir(tempPath.c_str(), 0755) != 0) {
				writeError("Error create temp directory: %s",tempPath.c_str());
				return 1;
			}

			//EXTRACT
			if (!extract(srcPath, tempPath)){
				return 1;
			}

			//FIX
			fixHighSierra(tempPath);

			//CUSTOM
			if (!copyCustom(srcPath, tempPath)){
				return 1;
			}

			//RUN INSTALLER
			char cmd_to_execute[4096];
			strcpy(cmd_to_execute,"");
			strcat(cmd_to_execute, srcPath.c_str());
			strcat(cmd_to_execute, "/installer");
			char* argscmd[] = {cmd_to_execute, (char*)tempPath.c_str(), (char*)appPath.c_str() , NULL};
			if (execvp(argscmd[0], argscmd) == -1) {
				deleteDirectory(tempPath);
				return 1;
			}
		}
	}else{
		/*size_t p = appPath.find_last_of('/');
		std::string dwagentPath = appPath.substr(0, p) + "/dwagent";*/
		size_t p = exePath.find_last_of("/\\");
		std::string filename = (p == std::string::npos) ? exePath : exePath.substr(p + 1);
		/*if (filename == "DWAgent"){
			char* argscmd[] = {(char*)dwagentPath.c_str(), (char*)"monitor", NULL};
			if (execvp(argscmd[0], argscmd) == -1) {
				return 1;
			}
		}else*/
		if (filename == "Configure"){
			size_t p = appPath.find_last_of('/');
			std::string dwagentPath = appPath.substr(0, p) + "/dwagent";
			char* argscmd[] = {(char*)dwagentPath.c_str(), (char*)"configure", NULL};
			if (execvp(argscmd[0], argscmd) == -1) {
				return 1;
			}
		}else if (filename == "Uninstall"){
			if (geteuid() == 0){
				size_t p = appPath.find_last_of('/');
				std::string dwagentPath = appPath.substr(0, p) + "/dwagent";
				std::string userLocale = std::string(argv[argc-1]);
				if (userLocale.find("LC=") == 0) {
					setUserLocale(userLocale.substr(3));
				}
				char* argscmd[] = {(char*)dwagentPath.c_str(), (char*)"uninstall", NULL};
				if (execvp(argscmd[0], argscmd) == -1) {
					return 1;
				}
			}else{
				char* argscmd[3];
				argscmd[0]=(char*)"uninstall";
				std::string userLocale = "LC=" + getUserLocale();
				argscmd[1] = (char*)userLocale.c_str();
				argscmd[2] = NULL;
				if (!executeWithPrivileges(exePath.c_str(),argscmd)){
					return 1;
				}
			}
		}else{
			std::string dwagentPath = "/Library/" + filename + "/native/dwagent";
			if (fileExists(dwagentPath)){
				char* argscmd[] = {(char*)dwagentPath.c_str(), (char*)"monitor", NULL};
				if (execvp(argscmd[0], argscmd) == -1) {
					return 1;
				}
			}else{
				return 1;
			}
		}
	}
    return 0;
}


