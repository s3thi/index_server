/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "SearchApp.h"

#include <Entry.h>
#include <Roster.h>


SearchApp::SearchApp()
	: BApplication(kAppSignature)
{
	fSearchWindow = new SearchWindow(BRect(100, 100, 500, 500)) ;
	fSearchWindow->Show() ;
}


void
SearchApp::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case 'lnch':
			LaunchFile(message) ;
		default :
			BApplication::MessageReceived(message) ;
	}
}


void
SearchApp::LaunchFile(BMessage *message)
{
	BListView *searchResults ;
	int32 index ;
	
	message->FindPointer("source", (void**)&searchResults) ;
	message->FindInt32("index", &index) ;
	BStringItem *result = (BStringItem*)searchResults->ItemAt(index) ;
	
	entry_ref ref ;
	BEntry entry(result->Text()) ;
	entry.GetRef(&ref) ;
	be_roster->Launch(&ref) ;
}

int main()
{
	SearchApp app ;
	app.Run() ;

	return 0 ;
}

