/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "support.h"

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

status_t load_settings(BMessage* message)
{
	status_t ret = B_BAD_VALUE ;
	
	if (message) {
		BPath settingsPath ;
		if ((ret = find_directory(B_USER_CONFIG_DIRECTORY, &settingsPath)) == B_OK) {
			settingsPath.Append("settings/Beacon/main_settings") ;

			BFile settingsFile(settingsPath.Path(), B_READ_ONLY) ;
			if ((ret = settingsFile.InitCheck()) == B_OK) {
				ret = message->Unflatten(&settingsFile) ;
				settingsFile.Unset() ;
			}
		}
	}

	return ret ;
}


status_t save_settings(BMessage *message)
{
	status_t ret = B_BAD_VALUE ;
	
	if (message) {
		BPath settingsPath ;
		if ((ret = find_directory(B_USER_CONFIG_DIRECTORY, &settingsPath)) == B_OK) {	
			settingsPath.Append("settings/Beacon") ;
			if ((ret = create_directory(settingsPath.Path(), 0777)) == B_OK) {
				settingsPath.Append("main_settings") ;

				BFile settingsFile(settingsPath.Path(),
					B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE) ;
				if ((ret = settingsFile.InitCheck()) == B_OK) {
					ret = message->Flatten(&settingsFile) ;
					settingsFile.Unset() ;
				}
			}
		}
	}

	return ret ;
}


Logger* open_log(DebugLevel level, bool replace)
{
	Logger *logger = NULL ;
	BPath logPath ;
	if(find_directory(B_USER_CONFIG_DIRECTORY, &logPath) == B_OK) {
		logPath.Append("settings/Beacon/") ;
		if(create_directory(logPath.Path(), 0777) == B_OK) {
			logPath.Append("log.txt") ;

			logger = new Logger(logPath.Path(), level, replace) ;
		}
	}

	return logger ;
}

