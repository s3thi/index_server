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
#include <NodeInfo.h>
#include <Path.h>

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
	
	fQueryFeeder = new Feeder ;
}


void
Indexer ::ReadyToRun()
{
	fMessageRunner = new BMessageRunner(this, new BMessage(BEACON_UPDATE_INDEX),
		fUpdateInterval) ;
	
	OpenIndex() ;

	fQueryFeeder->StartWatching() ;
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
	fQueryFeeder->SaveSettings(&settings) ;
	SaveSettings(&settings) ;
	save_settings(&settings) ;
	
	fQueryFeeder->PostMessage(B_QUIT_REQUESTED) ;
	
	fIndexWriter->optimize() ;
	fIndexWriter->close() ;

	return true ;
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
	while (fQueryFeeder->GetNextRef(&ref) == B_OK) {
		AddDocument(&ref) ;
	}
}


void
Indexer :: AddDocument(entry_ref* ref)
{
	if (!TranslatorAvailable(ref))
		return ;

	// This will have to be changed once we add support for
	// formats other than plain text.
	
	BPath path(ref) ;
	// TODO: figure out if FileReaders delete themselves after
	// they are done. It certainly looks that way.
	fFileReader = new FileReader(path.Path(), "ASCII") ;

	// The document has to be on the heap. See
	// the CLucene API documentation.
	Document *doc = new Document ;
	Field contentsField("contents", fFileReader,
		Field::STORE_NO | Field::INDEX_TOKENIZED) ;
	Field pathField("path", path.Path(),
		Field::STORE_YES | Field::INDEX_UNTOKENIZED) ;
	doc->add(contentsField) ;
	doc->add(pathField) ;
	fIndexWriter->addDocument(doc, &fStandardAnalyzer) ;
}


void
Indexer :: OpenIndex()
{
	BPath indexPath ;
	find_directory(B_COMMON_DIRECTORY, &indexPath) ;
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
}


bool
Indexer :: TranslatorAvailable(entry_ref *ref)
{
	// For now, just return true if the file is a plain text file.
	// Will change in the future when we have more translators.
	BNode node(ref) ;
	BNodeInfo nodeInfo(&node) ;

	char MIMEString[B_MIME_TYPE_LENGTH] ;
	nodeInfo.GetType(MIMEString) ;
	if (!strcmp(MIMEString, "text/plain"))
		return true ;
	
	return false ;
}


