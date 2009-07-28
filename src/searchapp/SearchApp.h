/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _SEARCH_APP_H
#define _SEARCH_APP_H

#include "SearchWindow.h"

#include <Application.h>

#define APP_SIGNATURE "application/x-vnd.Haiku-BeaconSearch"


class SearchApp : public BApplication {
	public:
		SearchApp() ;
	
	private:
		SearchWindow	*fSearchWindow ;
} ;

#endif
