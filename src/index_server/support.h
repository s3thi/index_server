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
#include "../constants.h"

#include <cstring>
#include <cstdlib>

#include <Message.h>


extern Logger *logger ;

status_t load_settings(BMessage *message) ;
status_t save_settings(BMessage *message) ;
Logger* open_log(DebugLevel level, bool replace) ;
wchar_t* to_wchar(const char *str) ;
bool is_hidden(entry_ref *ref) ;

#endif /* _SUPPORT_H */
