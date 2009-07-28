/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "SearchWindow.h"

#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>


SearchWindow::SearchWindow(BRect frame)
	: BWindow(BRect(50, 50, 100, 100), "Beacon Search", B_TITLED_WINDOW,
		B_QUIT_ON_WINDOW_CLOSE | B_AUTO_UPDATE_SIZE_LIMITS)
{
	CreateWindow() ;
}


void
SearchWindow::CreateWindow()
{
	fSearchButton = new BButton("Search", new BMessage('srch')) ;
	fSearchField = new BTextControl("", "", new BMessage('srch')) ;
	fSearchResults = new BListView() ;

	SetLayout(new BGroupLayout(B_VERTICAL)) ;

	AddChild(BGroupLayoutBuilder(B_VERTICAL, 10)
		.Add(BGroupLayoutBuilder(B_HORIZONTAL, 10)
			.Add(fSearchField)
			.Add(fSearchButton)
			.SetInsets(5, 5, 5, 5)
		)
	.Add(fSearchResults)
	.SetInsets(5, 5, 5, 5)
	) ;

	ResizeTo(500, 250) ;
}


void
SearchWindow::MessageReceived(BMessage *message)
{
	switch(message->what) {
		case 'srch':
			Search() ;
			break ;
		default:
			BWindow::MessageReceived(message) ;
	}
}


void
SearchWindow::Search()
{
	BMessage reply ;
}
