/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _SEARCH_APP_H_
#define _SEARCH_APP_H_

#include "SearchWindow.h"

#include <Application.h>


const char* kAppSignature = "application/x-vnd.Haiku-BeaconSearch" ;

class SearchApp : public BApplication {
	public:
		SearchApp() ;
		virtual void MessageReceived(BMessage *message) ;
		void LaunchFile(BMessage *message) ;
	
	private:
		SearchWindow	*fSearchWindow ;
} ;

#endif /* _SEARCH_APP_H_ */
