/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "Indexer.h"
#include "support.h"

#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <Path.h>

#include <iostream>
using namespace std ;

using namespace lucene::document ;

enum {
	BEACON_UPDATE_INDEX = 'updt'
} ;


Indexer :: Indexer()
	: BApplication("application/x-vnd.Haiku-BeaconDaemon"),
	  fUpdateInterval(30 * 1000000)
{
	BMessage settings('sett') ;
	if (load_settings(&settings) == B_OK)
		LoadSettings(&settings) ;
}


void
Indexer ::ReadyToRun()
{
	fMessageRunner = new BMessageRunner(this, new BMessage(BEACON_UPDATE_INDEX),
		fUpdateInterval) ;
	
	OpenIndex() ;

	fQueryFeeder.StartWatching() ;
	UpdateIndex() ;
}


void
Indexer :: MessageReceived(BMessage *message)
{
	switch (message->what) {
		case BEACON_UPDATE_INDEX :
			UpdateIndex() ;
			break ;
		default :
			BApplication :: MessageReceived(message) ;
	}
}


bool
Indexer :: QuitRequested()
{
	BMessage settings('sett') ;
	fQueryFeeder.SaveSettings(&settings) ;
	SaveSettings(&settings) ;
	save_settings(&settings) ;
	
	fQueryFeeder.PostMessage(B_QUIT_REQUESTED) ;
	
	fIndexWriter->optimize() ;
	fIndexWriter->close() ;
}


void
Indexer :: SaveSettings(BMessage *settings)
{
	if(settings->ReplaceInt64("update_interval", fUpdateInterval) != B_OK)
		settings->AddInt64("update_interval", fUpdateInterval) ;
}


void
Indexer :: LoadSettings(BMessage *settings)
{
	bigtime_t updateInterval ;
	if (settings->FindInt64("update_interval", &updateInterval) == B_OK)
		fUpdateInterval = updateInterval ;
}


void
Indexer :: UpdateIndex()
{
	entry_ref ref ;
	while (fQueryFeeder.GetNextRef(&ref) == B_OK) {
		AddDocument(&ref) ;
	}
}


void
Indexer :: AddDocument(entry_ref *ref)
{
	if (Excluded(ref))
		return ;

	// This will have to be changed once we add support for
	// formats other than plain text.
	BFile f(ref, B_READ_ONLY) ;
	off_t size ;
	f.GetSize(&size) ;
	char *buf = new char[size+1] ;
	f.Read(buf, size) ;
	buf[size] = '\0' ;
	f.Unset() ;
	
	BPath p(ref) ;

	// The document has to be on the heap. See
	// the CLucene API documentation.
	Document *doc = new Document ;
	Field contentsField("contents", buf,
		Field::STORE_NO | Field::INDEX_TOKENIZED) ;
	Field pathField("path", p.Path(),
		Field::STORE_YES | Field::INDEX_UNTOKENIZED) ;
	doc->add(contentsField) ;
	doc->add(pathField) ;
	fIndexWriter->addDocument(doc, &fStandardAnalyzer) ;

	delete buf ;
}


bool
Indexer :: Excluded(entry_ref *ref)
{
	// Quick hack for now. This will change in the future to become
	// more flexible. Only checks for /boot/common/data/index/ to
	// prevent the indexer from recursively indexing its own indexes.
	BPath refDirectory(ref) ;
	refDirectory.GetParent(&refDirectory) ;
	BPath indexDirectory("/boot/common/data/index/") ;
	if (refDirectory == indexDirectory)
		return true ;
	
	return false ;
}


void
Indexer :: OpenIndex()
{
	BPath indexPath ;
	if (find_directory(B_COMMON_DIRECTORY, &indexPath) == B_OK) {
		indexPath.Append("data/index/") ;
		if (create_directory(indexPath.Path(), 0777) == B_OK) {
			try {
				if (IndexReader::indexExists(indexPath.Path()))
					fIndexWriter = new IndexWriter(indexPath.Path(),
						&fStandardAnalyzer, false) ;
				else
					fIndexWriter = new IndexWriter(indexPath.Path(),
						&fStandardAnalyzer, true) ;
			} catch (CLuceneError) {
				// Index is corrupted. Delete it.
				BDirectory indexDirectory(indexPath.Path()) ;
				BEntry entry ;

				while (indexDirectory.GetNextEntry(&entry, false) == B_OK)
					entry.Remove() ;

				OpenIndex() ;
			}
		} else
			Quit() ;
	} else
		Quit() ;
}
