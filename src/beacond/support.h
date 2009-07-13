/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _SUPPORT_H
#define _SUPPORT_H

#include <Message.h>

enum {
	BEACON_UPDATE_INDEX = 'updt',
} ;

status_t load_settings(BMessage *message) ;
status_t save_settings(BMessage *message) ;

#endif /* _SUPPORT_H */
