/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <csignal>
#include <dirent.h>
#include <sys/stat.h>
#include <SystemConfiguration/SystemConfiguration.h>

volatile sig_atomic_t stop = 0;

void handle_signal(int signal) {
    if (signal == SIGTERM) {
    	stop = 1;
    }
}

void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {

    }
}

bool isCurrentUserActive() {
    uid_t activeUid;
    gid_t activeGid;
    uid_t currentUid = getuid();
    CFStringRef consoleUser = SCDynamicStoreCopyConsoleUser(NULL, &activeUid, &activeGid);
    if (!consoleUser) {
    	return currentUid == 0;
    }
    CFRelease(consoleUser);
    return currentUid == activeUid;
}

int main(int argc, char **argv) {
	if (argc>=3){
		if (strcmp(argv[2], "run") == 0) {
			char cmd_to_execute[4096];
			strcpy(cmd_to_execute,"");
			strcat(cmd_to_execute, argv[1]);
			strcat(cmd_to_execute, "/native/dwagsvc");
			char* argscmd[] = {cmd_to_execute, argv[2], NULL};
			execvp(argscmd[0], argscmd);
		}else if ((strcmp(argv[2], "systray") == 0) && (argc>=4)){
			char path_dwa[2048];
			char env_library[2048];
			char cmd_to_execute[2048];

			strcpy(path_dwa,"");
			strcat(path_dwa, argv[1]);

			strcpy(env_library,"DYLD_LIBRARY_PATH=");
			strcat(env_library, path_dwa);
			strcat(env_library, "/runtime/lib");

			strcpy(cmd_to_execute,"");
			strcat(cmd_to_execute, path_dwa);
			strcat(cmd_to_execute, "/runtime/bin/");
			strcat(cmd_to_execute, argv[3]);

			chdir(path_dwa);
			char *args[] = {cmd_to_execute, (char*)"monitor.py", (char*)"systray", NULL};
			char *env[] = {env_library, NULL};
			execve(cmd_to_execute, args , env);
		}else if ((strcmp(argv[2], "lac") == 0) && (argc>=4)){
			signal(SIGTERM, handle_signal);
			signal(SIGCHLD, handle_sigchld);
			char pth_to_check[4096];
			strcpy(pth_to_check,"");
			strcat(pth_to_check, argv[1]);
			strcat(pth_to_check, "/sharedmem");
			bool bchildprocess=false;
			while (!stop) {
				if (isCurrentUserActive()){
					DIR* dir = opendir(pth_to_check);
					if (dir) {
						struct dirent* entry;
						while ((entry = readdir(dir)) != NULL) {
							const char* filename = entry->d_name;
							size_t len = strlen(filename);
							if (len >= 4 && strcmp(filename + len - 4, ".lac") == 0){
								char file_to_check[4096];
								strcpy(file_to_check,"");
								strcat(file_to_check, argv[1]);
								strcat(file_to_check, "/sharedmem/");
								strcat(file_to_check, filename);
								struct stat file_stat;
								if (stat(file_to_check, &file_stat) == 0){
									if (file_stat.st_size == 0) {
										FILE* file = fopen(file_to_check, "w");
										if (file) {
											bchildprocess=true;
											char skey[(len - 4)+1];
											strncpy(skey, filename, len - 4);
											skey[len - 4]='\0';
											pid_t pid = fork();
											if (pid == 0) {
												char path_dwa[2048];
												char env_library[2048];
												char cmd_to_execute[2048];

												strcpy(path_dwa,"");
												strcat(path_dwa, argv[1]);

												strcpy(env_library,"DYLD_LIBRARY_PATH=");
												strcat(env_library, path_dwa);
												strcat(env_library, "/runtime/lib");

												strcpy(cmd_to_execute,"");
												strcat(cmd_to_execute, path_dwa);
												strcat(cmd_to_execute, "/runtime/bin/");
												strcat(cmd_to_execute, argv[3]);

												chdir(path_dwa);
												char *args[] = {cmd_to_execute, (char*)"agent.py", (char*)"app=ipc", (char*)skey, NULL};
												char *env[] = {env_library, NULL};
												execve(cmd_to_execute, args , env);
												exit(EXIT_FAILURE);
											}else if (pid > 0) {
												fprintf(file, "%d", pid);
											}else{
												fprintf(file, "ERROR");
											}
											fclose(file);
										}
									}
								}
							}
						}
						closedir(dir);
					}
				}else{
					if (bchildprocess){
						bchildprocess=false;
						kill(-getpid(), SIGTERM);
					}
				}
				sleep(1);
			}
			kill(-getpid(), SIGTERM);
		}
	}
	return 0;
}
