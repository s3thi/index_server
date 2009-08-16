/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "Indexer.h"
#include "Logger.h"
#include "support.h"

Logger* logger ;

int main()
{
	logger = open_log(BEACON_DEBUG_VERBOSE, true) ;
	Indexer beaconApp ;
	beaconApp.Run() ;
	logger->Close() ;
	return 0 ;
}
