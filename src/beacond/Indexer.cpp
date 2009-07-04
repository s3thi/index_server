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

#include <iostream>
using namespace std ;

using namespace lucene::document ;

enum {
	BEACON_UPDATE_INDEX = 'updt'
} ;

struct index_writer_ref {
	IndexWriter *indexWriter ;
	dev_t device ;
} ;


Indexer :: Indexer()
	: BApplication("application/x-vnd.Haiku-BeaconDaemon"),
	  fUpdateInterval(30 * 1000000),
	  fIndexWriterList(1)
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
	

	fQueryFeeder->StartWatching() ;
	InitIndexes() ;
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
	
	index_writer_ref *ref ;
	for (int i = 0 ; (ref = (index_writer_ref*)fIndexWriterList.ItemAt(0))
		!= NULL ; i++) {
			ref->indexWriter->optimize() ;
			ref->indexWriter->close() ;
	}

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
	// See the device ID of the ref, and write to that specific
	// index.
	
	if (!TranslatorAvailable(ref))
		return ;
	
	IndexWriter *indexWriter = NULL ;
	index_writer_ref *iter_ref ;
	for (int i = 0 ; (iter_ref = (index_writer_ref*)fIndexWriterList.ItemAt(i))
		!= NULL ; i++) {
			if (iter_ref->device == ref->device)
				indexWriter = iter_ref->indexWriter ;
	}

	if (indexWriter == NULL)
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
	indexWriter->addDocument(doc, &fStandardAnalyzer) ;
}


void
Indexer :: InitIndexes()
{
	BVolume *volume ;
	BList *volumeList = fQueryFeeder->GetVolumeList() ;
	index_writer_ref *ref ;
	BDirectory dir ;
	for (int i = 0 ; (volume = (BVolume*)volumeList->ItemAt(i)) != NULL ; i++)
	{
		ref = new index_writer_ref ;
		ref->device = volume->Device() ;
		volume->GetRootDirectory(&dir) ;
		ref->indexWriter = OpenIndex(&dir) ;
		if (ref->indexWriter != NULL)
			fIndexWriterList.AddItem((void*)ref) ;
		else
			delete ref ;
	}
}


IndexWriter*
Indexer :: OpenIndex(BDirectory *dir)
{	
	IndexWriter *indexWriter = NULL ;
	BPath path(dir) ;
	path.Append("index") ;
	
	if(create_directory(path.Path(), 0777) != B_OK)
		return NULL ;
	
	try {
		if (IndexReader::indexExists(path.Path()))
			indexWriter = new IndexWriter(path.Path(), &fStandardAnalyzer, 
				false) ;
		else
			indexWriter = new IndexWriter(path.Path(), &fStandardAnalyzer,
				true) ;
	} catch (CLuceneError) {
		// Index is unreadable. Delete it and call OpenIndex() again.
		BEntry entry ;
		BDirectory indexDirectory(path.Path()) ;
		while (indexDirectory.GetNextEntry(&entry, false) == B_OK)
			entry.Remove() ;

		indexWriter = OpenIndex(dir) ;
	}

	return indexWriter ;
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


