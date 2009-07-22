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

#include <Message.h>

enum {
	BEACON_UPDATE_INDEX = 'updt',
} ;

enum ErrorCode {
	BEACON_NOT_SUPPORTED,
	BEACON_FILE_EXCLUDED
} ;

extern Logger *logger ;

status_t load_settings(BMessage *message) ;
status_t save_settings(BMessage *message) ;
Logger* open_log(DebugLevel level, bool replace) ;

#endif /* _SUPPORT_H */
