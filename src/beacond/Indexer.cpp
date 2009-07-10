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


Indexer::Indexer()
	: BApplication("application/x-vnd.Haiku-BeaconDaemon"),
	  fIndexWriterList(1)
{
	BMessage settings('sett') ;
	if (load_settings(&settings) == B_OK)
		LoadSettings(&settings) ;
}


void
Indexer::ReadyToRun()
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
Indexer::MessageReceived(BMessage *message)
{
	index_writer_ref *iw_ref = NULL ; 
	dev_t device ;
	
	switch (message->what) {
		case BEACON_UPDATE_INDEX :
			UpdateIndex() ;
			break ;
		case BEACON_THREAD_DONE :
			message->FindInt32("device", &device) ;
			iw_ref = FindIndexWriterRef(device) ;
			acquire_sem(iw_ref->sem) ;
		case B_NODE_MONITOR :
			HandleDeviceUpdate(message) ;
			break ;
		default :
			BApplication :: MessageReceived(message) ;
	}
}


bool
Indexer::QuitRequested()
{
	BMessage settings('sett') ;
	fQueryFeeder->SaveSettings(&settings) ;
	SaveSettings(&settings) ;
	save_settings(&settings) ;
	
	fQueryFeeder->PostMessage(B_QUIT_REQUESTED) ;

	index_writer_ref *ref ;
	status_t exitValue ;
	for (int i = 0 ; (ref = (index_writer_ref*)fIndexWriterList.ItemAt(i))
		!= NULL ; i++) {
			delete_sem(ref->sem) ;
			wait_for_thread(ref->thread, &exitValue) ;
	}

	return true ;
}


void
Indexer::SaveSettings(BMessage *settings)
{
}


void
Indexer::LoadSettings(BMessage *settings)
{
}


void
Indexer::UpdateIndex()
{
	index_writer_ref *iw_ref = NULL ;
	entry_ref* e_ref = new entry_ref ;
	dev_t device = -1 ;
	while (fQueryFeeder->GetNextRef(e_ref) == B_OK) {
		if (iw_ref == NULL || iw_ref->device != e_ref->device)
			iw_ref = FindIndexWriterRef(e_ref->device) ;

		if (iw_ref != NULL)
			iw_ref->entryList->AddItem(e_ref) ;
	}

	// Resume the add_document threads after the list of files has been
	// updated.
	for (int i = 0 ; (iw_ref = (index_writer_ref*)fIndexWriterList.ItemAt(i))
		!= NULL ; i++) {
			release_sem(iw_ref->sem) ;
			resume_thread(iw_ref->thread) ;
	}
}


status_t
Indexer::InitIndex(BVolume *volume)
{
	status_t error = B_BAD_VALUE ;

	if (!volume)
		return error ;
	
	index_writer_ref *iw_ref = new index_writer_ref ;
	char volume_name[B_FILE_NAME_LENGTH] ;
	BDirectory dir ;
	
	iw_ref->device = volume->Device() ;
	
	volume->GetRootDirectory(&dir) ;
	/*
	if ((iw_ref->indexWriter = OpenIndex(&dir)) == NULL) {
		delete iw_ref ;
		return B_ERROR ;
	}*/
	iw_ref->index = new BeaconIndex(&dir) ;
	
	volume->GetName(volume_name) ;
	if ((iw_ref->sem = create_sem(1, volume_name)) < B_NO_ERROR) {
		delete iw_ref ;
		return iw_ref->sem ;
	}

	if ((error = acquire_sem(iw_ref->sem)) < B_OK) {
		delete iw_ref ;
		return error ;
	}
	
	if ((iw_ref->thread = spawn_thread(add_document, volume_name, B_LOW_PRIORITY,
		iw_ref)) < B_NO_ERROR) {
			delete iw_ref ;
			return iw_ref->thread ;
	}

	iw_ref->entryList = new BList(10) ;
	fIndexWriterList.AddItem(iw_ref) ;
	
	return B_OK ;
}


void
Indexer::HandleDeviceUpdate(BMessage *message)
{
	int32 opcode ;
	dev_t device ;
	BVolume volume ;
	index_writer_ref *iw_ref ;
	message->FindInt32("opcode", &opcode) ;

	switch (opcode) {
		case B_DEVICE_MOUNTED :
			message->FindInt32("new device", &device) ;
			volume.SetTo(device) ;
			InitIndex(&volume) ;
			break ;
		
		case B_DEVICE_UNMOUNTED :
			message->FindInt32("device", &device) ;
			iw_ref = FindIndexWriterRef(device) ;
			fIndexWriterList.RemoveItem(iw_ref) ;
			kill_thread(iw_ref->thread) ;
			delete_sem(iw_ref->sem) ;
			delete iw_ref ;
			break ;
	}
}


index_writer_ref*
Indexer::FindIndexWriterRef(dev_t device)
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
	index_writer_ref *iw_ref = (index_writer_ref*)data ;
	BMessage doneMessage(BEACON_THREAD_DONE) ;
	doneMessage.AddInt32("device", iw_ref->device) ;
	BList *entryList = iw_ref->entryList ;
	entry_ref *iter_ref ;
	BPath path ;
	BeaconIndex *index = iw_ref->index ;
	FileReader *fileReader ;
	Document *doc ;

	// Get path to the index directory on this particular
	// volume.
	BVolume volume(iw_ref->device) ;
	BDirectory dir ;
	volume.GetRootDirectory(&dir) ;
	path.SetTo(&dir) ;
	path.Append("index") ;
	dir.SetTo(path.Path()) ;

	while (acquire_sem(iw_ref->sem) >= B_NO_ERROR) {
		for (int i = 0 ; (iter_ref = (entry_ref*)entryList->ItemAt(i)) != NULL
			; i++)
				index->AddDocument(iter_ref) ;

		entryList->MakeEmpty() ;
		be_app->PostMessage(&doneMessage) ;
		release_sem(iw_ref->sem) ;
		suspend_thread(find_thread(NULL)) ;
	}

	return B_OK ;
}
