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
#include <NodeMonitor.h>
#include <Path.h>

using namespace lucene::document ;


Indexer :: Indexer()
	: BApplication("application/x-vnd.Haiku-BeaconDaemon"),
	  fIndexWriterList(1)
{
	BMessage settings('sett') ;
	if (load_settings(&settings) == B_OK)
		LoadSettings(&settings) ;
}


void
Indexer ::ReadyToRun()
{
	fQueryFeeder = new Feeder() ;
	fQueryFeeder->StartWatching() ;

	BVolume *volume ;
	BList *volumeList = fQueryFeeder->GetVolumeList() ;
	for (int i = 0 ; (volume = (BVolume*)volumeList->ItemAt(i)) != NULL ; i++)
		InitIndex(volume) ;

	UpdateIndex() ;
}


void
Indexer :: MessageReceived(BMessage *message)
{
	index_writer_ref *ref = NULL ; 
	dev_t device ;
	
	switch (message->what) {
		case BEACON_UPDATE_INDEX :
			UpdateIndex() ;
			break ;
		case BEACON_THREAD_DONE :
			message->FindInt32("device", &device) ;
			ref = FindIndexWriterRef(device) ;
			acquire_sem(ref->sem) ;
			break ;
		case B_NODE_MONITOR :
			HandleDeviceUpdate(message) ;
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
}


void
Indexer :: LoadSettings(BMessage *settings)
{
}


void
Indexer :: UpdateIndex()
{
	entry_ref* iter_ref = new entry_ref ;
	dev_t device = -1 ;
	index_writer_ref *ref = NULL ;
	while (fQueryFeeder->GetNextRef(iter_ref) == B_OK) {
		if (ref == NULL || ref->device != iter_ref->device)
			ref = FindIndexWriterRef(iter_ref->device) ;

		if (ref != NULL && TranslatorAvailable(iter_ref))
			ref->entryList->AddItem(iter_ref) ;
	}

	for (int i = 0 ; (ref = (index_writer_ref*)fIndexWriterList.ItemAt(i))
		!= NULL ; i++) {
			release_sem(ref->sem) ;
			resume_thread(ref->thread) ;
	}
}


void
Indexer :: InitIndex(BVolume *volume)
{
	// TODO: handle errors.
	
	index_writer_ref *ref = new index_writer_ref ;
	char volume_name[B_FILE_NAME_LENGTH] ;
	BDirectory dir ;
	
	ref->device = volume->Device() ;
	volume->GetRootDirectory(&dir) ;
	ref->indexWriter = OpenIndex(&dir) ;
	ref->entryList = new BList(10) ;
	
	volume->GetName(volume_name) ;
	ref->sem = create_sem(1, volume_name) ;
	acquire_sem(ref->sem) ;
	ref->thread = spawn_thread(add_document, volume_name, B_NORMAL_PRIORITY,
		(void*)ref) ;

	fIndexWriterList.AddItem((void*)ref) ;
}


IndexWriter*
Indexer :: OpenIndex(BDirectory *dir)
{	
	// TODO: handle errors.

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


void
Indexer :: HandleDeviceUpdate(BMessage *message)
{
	int32 opcode ;
	dev_t device ;
	BVolume volume ;
	message->FindInt32("opcode", &opcode) ;

	switch (opcode) {
		case B_DEVICE_MOUNTED :
			message->FindInt32("new device", &device) ;
			volume.SetTo(device) ;
			InitIndex(&volume) ;
			break ;
		
		case B_DEVICE_UNMOUNTED :
			message->FindInt32("device", &device) ;
			volume.SetTo(device) ;
			CloseIndex(&volume) ;
			break ;
	}
}


void
Indexer :: CloseIndex(BVolume *volume)
{
	// Just removes the index_writer_ref from fIndexWriterList for now.
	// This means unmounting or unplugging the USB will cause the index
	// to become corrupted. TODO.
	index_writer_ref *ref ;
	for (int i = 0 ; (ref = (index_writer_ref*)fIndexWriterList.ItemAt(i)) != NULL
		; i++) {
			if (ref->device == volume->Device()) {
				fIndexWriterList.RemoveItem(i) ;
				delete ref ;
				break ;
			}
	}
}


index_writer_ref*
Indexer :: FindIndexWriterRef(dev_t device)
{
	index_writer_ref *iter_ref ;
	for (int i = 0 ; (iter_ref = (index_writer_ref*)fIndexWriterList.ItemAt(i))
		!= NULL ; i++) {
			if (iter_ref->device == device)
				return iter_ref ;
	}

	return NULL ;
}


int32 add_document(void *data)
{
	index_writer_ref *ref = (index_writer_ref*)data ;
	entry_ref *iter_ref ;
	BList *entryList = ref->entryList ;
	BMessage doneMessage(BEACON_THREAD_DONE) ;
	doneMessage.AddInt32("device", ref->device) ;
	BPath path ;
	IndexWriter *indexWriter = ref->indexWriter ;
	FileReader *fileReader ;
	Document *doc ;
	StandardAnalyzer standardAnalyzer ;

	BVolume volume(ref->device) ;
	BDirectory dir ;
	volume.GetRootDirectory(&dir) ;
	path.SetTo(&dir) ;
	path.Append("index") ;
	dir.SetTo(path.Path()) ;

	while (acquire_sem(ref->sem) >= B_NO_ERROR) {
		for (int i = 0 ; (iter_ref = (entry_ref*)entryList->ItemAt(i)) != NULL
			; i++) {
				path.SetTo(iter_ref) ;
				if (dir.Contains(path.Path()))
					continue ;

				// TODO: figure out if FileReaders delete themselves after
				// they are done. It certainly looks that way.
				fileReader = new FileReader(path.Path(), "ASCII") ;

				doc = new Document ;
				doc->add(*(new Field("contents", fileReader, 
					Field::STORE_NO | Field::INDEX_TOKENIZED))) ;
				doc->add(*(new Field ("path", path.Path(),
					Field::STORE_YES | Field::INDEX_UNTOKENIZED))) ;
				indexWriter->addDocument(doc, &standardAnalyzer) ;

				delete iter_ref ;
		}

		entryList->MakeEmpty() ;
		be_app->PostMessage(&doneMessage) ;
		release_sem(ref->sem) ;
		suspend_thread(find_thread(NULL)) ;
	}
}
