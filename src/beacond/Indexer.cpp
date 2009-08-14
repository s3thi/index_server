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
	
	LockAllIndexes() ;

	// Get updates.
	while(fQueryFeeder->GetNextUpdate(e_ref) == B_OK) {
		if(i_ref == NULL || i_ref->device != e_ref->device)
			i_ref = FindIndexRef(e_ref->device) ;

		if(i_ref != NULL) {
			i_ref->index->AddDocument(e_ref) ;
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
			i_ref->index->RemoveDocument(e_ref) ;
			e_ref = new entry_ref ;
		}
	}

	UnlockAllIndexes() ;
	
	// Call Commit() on all indexes.
	for (int i = 0 ; (i_ref = (index_ref*)fIndexRefList.ItemAt(i)) ; i++)
		i_ref->index->Commit() ;
}


status_t
Indexer::InitIndex(BVolume *volume)
{
	status_t error = B_BAD_VALUE ;

	if (!volume)
		return error ;
	
	index_ref *i_ref = new index_ref ;
	i_ref->device = volume->Device() ;
	i_ref->index = new BeaconIndex(volume) ;
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


void
Indexer::LockAllIndexes()
{
	index_ref* i_ref ;
	for (int i = 0 ; (i_ref = (index_ref*)fIndexRefList.ItemAt(i)) != NULL ; 
		i++)
		i_ref->index->Lock() ;
}


void
Indexer::UnlockAllIndexes()
{
	index_ref* i_ref ;
	for (int i = 0 ; (i_ref = (index_ref*)fIndexRefList.ItemAt(i)) != NULL ; 
		i++)
		i_ref->index->Unlock() ;
}
