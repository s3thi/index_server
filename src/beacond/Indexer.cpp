/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "Indexer.h"
#include "support.h"
#include "Logger.h"

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
	logger->Always("Starting application.") ;
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
	index_ref *i_ref = NULL ; 
	dev_t device ;
	
	switch (message->what) {
		case BEACON_UPDATE_INDEX :
			UpdateIndex() ;
			break ;
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

	index_ref *ref ;
	status_t exitValue ;
	for (int i = 0 ; (ref = (index_ref*)fIndexWriterList.ItemAt(i))
		!= NULL ; i++) {
			delete_sem(ref->sem) ;
			wait_for_thread(ref->thread, &exitValue) ;
			ref->index->Close() ;
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
	index_ref *i_ref = NULL ;
	entry_ref* e_ref = new entry_ref ;
	dev_t device = -1 ;
	while (fQueryFeeder->GetNextRef(e_ref) == B_OK) {
		if (i_ref == NULL || i_ref->device != e_ref->device)
			i_ref = FindIndexWriterRef(e_ref->device) ;

		if (i_ref != NULL) {
			i_ref->entryListLocker.Lock() ;
			i_ref->entryList->AddItem(e_ref) ;
			i_ref->entryListLocker.Unlock() ;
			e_ref = new entry_ref ;
		}
	}

	// Resume the add_document threads after the list of files has been
	// updated.
	for (int i = 0 ; (i_ref = (index_ref*)fIndexWriterList.ItemAt(i))
		!= NULL ; i++)
			release_sem(i_ref->sem) ;
}


status_t
Indexer::InitIndex(BVolume *volume)
{
	status_t error = B_BAD_VALUE ;

	if (!volume)
		return error ;
	
	index_ref *i_ref = new index_ref ;
	char volumeName[B_FILE_NAME_LENGTH] ;
	BDirectory dir ;
	
	i_ref->device = volume->Device() ;
	
	i_ref->index = new BeaconIndex(volume) ;
	
	volume->GetName(volumeName) ;
	if ((i_ref->sem = create_sem(1, volumeName)) < B_NO_ERROR) {
		delete i_ref ;
		return i_ref->sem ;
	}

	if ((error = acquire_sem(i_ref->sem)) < B_OK) {
		delete i_ref ;
		return error ;
	}
	
	if ((i_ref->thread = spawn_thread(add_document, volumeName, B_LOW_PRIORITY,
		i_ref)) < B_NO_ERROR) {
			delete i_ref ;
			return i_ref->thread ;
	} else
		resume_thread(i_ref->thread) ;

	i_ref->entryList = new BList(10) ;
	fIndexWriterList.AddItem(i_ref) ;
	
	return B_OK ;
}


void
Indexer::HandleDeviceUpdate(BMessage *message)
{
	int32 opcode ;
	dev_t device ;
	BVolume volume ;
	index_ref *i_ref ;
	message->FindInt32("opcode", &opcode) ;

	switch (opcode) {
		case B_DEVICE_MOUNTED :
			message->FindInt32("new device", &device) ;
			volume.SetTo(device) ;
			InitIndex(&volume) ;
			break ;
		
		case B_DEVICE_UNMOUNTED :
			message->FindInt32("device", &device) ;
			i_ref = FindIndexWriterRef(device) ;
			fIndexWriterList.RemoveItem(i_ref) ;
			kill_thread(i_ref->thread) ;
			delete_sem(i_ref->sem) ;
			delete i_ref ;
			break ;
	}
}


index_ref*
Indexer::FindIndexWriterRef(dev_t device)
{
	index_ref *iter_ref ;
	for (int i = 0 ; (iter_ref = (index_ref*)fIndexWriterList.ItemAt(i))
		!= NULL ; i++) {
			if (iter_ref->device == device)
				return iter_ref ;
	}

	return NULL ;
}


int32 add_document(void *data)
{
	index_ref *i_ref = (index_ref*)data ;
	BList *entryList = i_ref->entryList ;
	entry_ref *iter_ref ;
	BeaconIndex *index = i_ref->index ;

	while (acquire_sem(i_ref->sem) ==B_OK) {
		i_ref->entryListLocker.Lock() ;
		while ((iter_ref = (entry_ref*)entryList->ItemAt(0)) != NULL) {
			index->AddDocument(iter_ref) ;
			entryList->RemoveItem((int32)0) ;
			delete iter_ref ;
		}

		index->Commit() ;
		entryList->MakeEmpty() ;
		i_ref->entryListLocker.Unlock() ;
	}

	return B_OK ;
}
