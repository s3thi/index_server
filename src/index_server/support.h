/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _SUPPORT_H
#define _SUPPORT_H

#include "Logger.h"

#include <cstring>
#include <cstdlib>

#include <Message.h>

enum {
	BEACON_UPDATE_INDEX = 'updt',
} ;

enum ErrorCode {
	BEACON_NOT_SUPPORTED,
	BEACON_FILE_EXCLUDED,
	BEACON_FIRST_RUN
} ;

extern Logger *logger ;

status_t load_settings(BMessage *message) ;
status_t save_settings(BMessage *message) ;
Logger* open_log(DebugLevel level, bool replace) ;
wchar_t* to_wchar(char *str) ;

#endif /* _SUPPORT_H */
