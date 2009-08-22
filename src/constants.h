/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

#define APP_SIGNATURE "application/x-vnd.Haiku-IndexServer"

enum BeaconMessage {
	BEACON_UPDATE_INDEX =	'updt',
	BEACON_DELETE_ENTRY =	'dlte',
	BEACON_PAUSE =			'paus',
	BEACON_REINDEX =		'ridx',
	BEACON_COMMIT =			'cmit',
	BEACON_EXCLUDE =		'xcld',
} ;

enum ErrorCode {
	BEACON_NOT_SUPPORTED = 	'nspt',
	BEACON_FILE_EXCLUDED = 	'excl',
	BEACON_FIRST_RUN = 		'frst',
} ;

#endif /* _CONSTANTS_H_ */
