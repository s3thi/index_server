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
	  fIndexRefList(1)
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
			BApplication::MessageReceived(message) ;
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
	for (int i = 0 ; (ref = (index_ref*)fIndexRefList.ItemAt(i))
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
	index_ref *i_ref = NULL ;
	entry_ref* e_ref = new entry_ref ;
	dev_t device = -1 ;
	
	// Get updates.
	while(fQueryFeeder->GetNextUpdate(e_ref) == B_OK) {
		if(i_ref == NULL || i_ref->device != e_ref->device)
			i_ref = FindIndexRef(e_ref->device) ;

		if(i_ref != NULL) {
			AddEntry(e_ref) ;
			e_ref = new entry_ref ;
		}
	}

	i_ref = NULL ;
	e_ref = new entry_ref ;
	device = -1 ;

	// Get removals.
	while(fQueryFeeder->GetNextRemoval(e_ref) == B_OK) {
		if(i_ref == NULL || i_ref->device != e_ref->device)
			i_ref = FindIndexRef(e_ref->device) ;

		if(i_ref != NULL) {
			RemoveEntry(e_ref) ;
			e_ref = new entry_ref ;
		}
	}

	// Resume the add_document threads after the list of files has been
	// updated.
	for (int i = 0 ; (i_ref = (index_ref*)fIndexRefList.ItemAt(i))
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
	
	// A new BeaconIndex is created later in the add_document()
	// thread.
	i_ref->index = NULL ;
	
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

	// Allocate large chunks of memory to the queues
	// to cut down on frequent memory allocations.
	i_ref->indexQueue = new BList(50) ;
	i_ref->deleteQueue = new BList(50) ;
	
	fIndexRefList.AddItem(i_ref) ;
	
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
			logger->Always("Device mounted. Device ID %d", device) ;
			volume.SetTo(device) ;
			InitIndex(&volume) ;
			break ;
		
		case B_DEVICE_UNMOUNTED :
			message->FindInt32("device", &device) ;
			logger->Always("Device unmounted. Device ID %d", device) ;
			i_ref = FindIndexRef(device) ;
			fIndexRefList.RemoveItem(i_ref) ;
			kill_thread(i_ref->thread) ;
			delete_sem(i_ref->sem) ;
			delete i_ref ;
			break ;
	}
}


index_ref*
Indexer::FindIndexRef(dev_t device)
{
	index_ref *i_ref ;
	for (int i = 0 ; (i_ref = (index_ref*)fIndexRefList.ItemAt(i))
		!= NULL ; i++)
		if (i_ref->device == device)
			return i_ref ;

	return NULL ;
}


int32 add_document(void *data)
{
	// Initialize the BeaconIndex.
	index_ref *i_ref = (index_ref*)data ;
	BVolume volume(i_ref->device) ;
	i_ref->index = new BeaconIndex(&volume) ;

	// Making things a little easier.
	BList *indexQueue = i_ref->indexQueue ;
	BList *deleteQueue = i_ref->deleteQueue ;
	BeaconIndex *index = i_ref->index ;
	
	entry_ref *e_ref ;

	while(acquire_sem(i_ref->sem) == B_OK && index->InitCheck() == B_OK) {
		i_ref->locker.Lock() ;
		
		while((e_ref = (entry_ref*)indexQueue->ItemAt(0)) != NULL) {
			index->AddDocument(e_ref) ;
			indexQueue->RemoveItem(0L) ;
			delete e_ref ;
		}

		while((e_ref = (entry_ref*)deleteQueue->ItemAt(0)) != NULL) {
			index->RemoveDocument(e_ref) ;
			deleteQueue->RemoveItem(0L) ;
			delete e_ref ;
		}

		// Changes aren't written to disk unless Commit() is called.
		index->Commit() ;
		i_ref->locker.Unlock() ;
	}

	return B_OK ;
}

void
Indexer::RemoveEntry(entry_ref* e_ref)
{
	index_ref* i_ref = FindIndexRef(e_ref->device) ;

	if (i_ref != NULL) {
		i_ref->locker.Lock() ;
		i_ref->deleteQueue->AddItem(e_ref) ;
		i_ref->locker.Unlock() ;
	}
}


void
Indexer::AddEntry(entry_ref* e_ref)
{
	index_ref* i_ref = FindIndexRef(e_ref->device) ;

	if (i_ref != NULL) {
		i_ref->locker.Lock() ;
		i_ref->indexQueue->AddItem(e_ref) ;
		i_ref->locker.Unlock() ;
	}
}
