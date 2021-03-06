/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _SEARCH_WINDOW_H_
#define _SEARCH_WINDOW_H

#include <Button.h>
#include <ListView.h>
#include <ScrollView.h>
#include <TextControl.h>
#include <Window.h>


class SearchWindow : public BWindow {
	public:
		SearchWindow(BRect frame) ;
	
	private:
		void CreateWindow() ;
		void MessageReceived(BMessage *message) ;
		void Search() ;

		// Window controls.
		BButton			*fSearchButton ;
		BTextControl	*fSearchField ;
		BListView		*fSearchResults ;
		BScrollView		*fScrollView ;
} ;

#endif /* _SEARCH_WINDOW_H_ */
