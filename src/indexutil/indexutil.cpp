/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "../constants.h"

#include <cstdio>
#include <unistd.h>

#include <Message.h>
#include <Messenger.h>


void usage()
{
	printf("usage: indexutil [ argument ] [ option ]\n"
		"  -p\t\t\tpause/resume the indexer\n"
		"  -q\t\t\tquit the indexer\n"
		"  -call\t\t\tcall commit on all volumes\n"
		"  -c <path-to-volume>\tcall commit on <path-to-volume>\n"
		"  -rall\t\t\treindex all\n"
		"  -r <path-to-volume>\treindex <path-to-volume>\n"
		"  -e <path-to-volume>\texclude (for this session only)\n"
		"  -E <path-to-volume>\texclude permanently\n"
		"  -h\t\t\tprint this message\n"
	) ;
}


void pauseIndexer()
{
	BMessenger messenger(APP_SIGNATURE) ;
	BMessage pauseMessage(BEACON_PAUSE), reply ;
	status_t err ;
	
	if ((err = messenger.SendMessage(&pauseMessage, &reply)) == B_OK)
		printf("BEACON_PAUSE sent\n") ;
	else if (err == B_BAD_PORT_ID)
		printf("index_server not running\n") ;
}


void quitIndexer()
{
	BMessenger messenger(APP_SIGNATURE) ;
	BMessage reply ;
	status_t err ;
	
	if ((err = messenger.SendMessage(B_QUIT_REQUESTED, &reply)) == B_OK)
		printf("B_QUIT_REQUESTED sent\n") ;
	else if (err == B_BAD_PORT_ID)
		printf("index_server not running\n") ;
}


void reindex(const char* optarg)
{	
	BMessenger messenger(APP_SIGNATURE) ;
	BMessage reindexMessage(BEACON_REINDEX), reply ;
	status_t err ;

	if (optarg != NULL)
		reindexMessage.AddString("volume", optarg) ;
	
	if ((err = messenger.SendMessage(&reindexMessage, &reply)) == B_OK)
		printf("BEACON_REINDEX sent\n") ;
	else if (err == B_BAD_PORT_ID)
		printf("index_server not running\n") ;
}


void commit(const char* optarg)
{	
	BMessenger messenger(APP_SIGNATURE) ;
	BMessage commitMessage(BEACON_COMMIT), reply ;
	status_t err ;

	if (optarg != NULL)
		commitMessage.AddString("volume", optarg) ;
	
	if ((err = messenger.SendMessage(&commitMessage, &reply)) == B_OK)
		printf("BEACON_COMMIT sent\n") ;
	else if (err == B_BAD_PORT_ID)
		printf("index_server not running\n") ;
}


void exclude(const char* optarg, bool forever=false)
{	
	BMessenger messenger(APP_SIGNATURE) ;
	BMessage excludeMessage(BEACON_EXCLUDE), reply ;
	status_t err ;

	if (optarg != NULL)
		excludeMessage.AddString("volume", optarg) ;
	
	if (forever == false)
		excludeMessage.AddBool("forever", false) ;
	else
		excludeMessage.AddBool("forever", true) ;
	
	if ((err = messenger.SendMessage(&excludeMessage, &reply)) == B_OK)
		printf("BEACON_EXCLUDE sent\n") ;
	else if (err == B_BAD_PORT_ID)
		printf("index_server not running\n") ;
}


int main(int argc, char **argv)
{
	if (argc < 2) {
		usage() ;
		return 0 ;
	}
	
	int opt ;
	opt = getopt(argc, argv, "pqr:c:e:E:") ;
	switch (opt) {
		case 'p':
			pauseIndexer() ;
			break ;
		case 'q':
			quitIndexer() ;
			break ;
		case 'r':
			reindex(optarg) ;
			break ;
		case 'c':
			commit(optarg) ;
			break ;
		case 'e':
			exclude(optarg) ;
			break ;
		case 'E':
			exclude(optarg, true) ;
			break ;
		case 'h':
		default:
			usage() ;
			break ;
	}
	
	return 0 ;
}
