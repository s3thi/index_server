/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "SearchApp.h"


SearchApp::SearchApp()
	: BApplication(APP_SIGNATURE)
{
	fSearchWindow = new SearchWindow(BRect(100, 100, 500, 500)) ;
	fSearchWindow->Show() ;
}


int main()
{
	SearchApp app ;
	app.Run() ;

	return 0 ;
}
