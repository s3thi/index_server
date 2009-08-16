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
	: BApplication(APP_SIGNATURE)
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

	BeaconIndex *index ;
	BVolume *volume ;
	BList *volumeList = fQueryFeeder->GetVolumeList() ;
	for (int i = 0 ; (volume = (BVolume*)volumeList->ItemAt(i)) != NULL ;
		i++) {
		index = new BeaconIndex(volume) ;
		fIndexList.AddItem(index) ;
	}

	UpdateIndex() ;
}


void
Indexer::MessageReceived(BMessage *message)
{
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

	BeaconIndex *index ;
	status_t exitValue ;
	for (int i = 0 ; (index = (BeaconIndex*)fIndexList.ItemAt(i))
		!= NULL ; i++)
		index->Close() ;

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
	BeaconIndex *index = NULL ;
	entry_ref* e_ref = new entry_ref ;
	dev_t device = -1 ;
	
	LockAllIndexes() ;

	// Get updates.
	while(fQueryFeeder->GetNextUpdate(e_ref) == B_OK) {
		if(index == NULL || index->Device() != e_ref->device)
			index = FindIndex(e_ref->device) ;

		if(index != NULL) {
			index->AddDocument(e_ref) ;
			e_ref = new entry_ref ;
		}
	}

	index = NULL ;
	e_ref = new entry_ref ;
	device = -1 ;

	// Get removals.
	while(fQueryFeeder->GetNextRemoval(e_ref) == B_OK) {
		if(index == NULL || index->Device() != e_ref->device)
			index = FindIndex(e_ref->device) ;

		if(index != NULL) {
			index->RemoveDocument(e_ref) ;
			e_ref = new entry_ref ;
		}
	}

	UnlockAllIndexes() ;
	
	// Call Commit() on all indexes.
	for (int i = 0 ; (index = (BeaconIndex*)fIndexList.ItemAt(i)) ; i++)
		index->Commit() ;
}


void
Indexer::HandleDeviceUpdate(BMessage *message)
{
	int32 opcode ;
	dev_t device ;
	BVolume volume ;
	BeaconIndex *index ;
	message->FindInt32("opcode", &opcode) ;

	switch (opcode) {
		case B_DEVICE_MOUNTED :
			message->FindInt32("new device", &device) ;
			logger->Always("Device mounted. Device ID %d", device) ;
			volume.SetTo(device) ;
			index = new BeaconIndex(&volume) ;
			fIndexList.AddItem(index) ;
			break ;
		
		case B_DEVICE_UNMOUNTED :
			message->FindInt32("device", &device) ;
			logger->Always("Device unmounted. Device ID %d", device) ;
			index = FindIndex(device) ;
			fIndexList.RemoveItem(index) ;
			delete index ;
			break ;
	}
}


BeaconIndex*
Indexer::FindIndex(dev_t device)
{
	BeaconIndex *index ;
	for (int i = 0 ; (index = (BeaconIndex*)fIndexList.ItemAt(i))
		!= NULL ; i++)
		if (index->Device() == device)
			return index ;

	return NULL ;
}


void
Indexer::LockAllIndexes()
{
	BeaconIndex* index ;
	for (int i = 0 ; (index = (BeaconIndex*)fIndexList.ItemAt(i)) != NULL ; 
		i++)
		index->Lock() ;
}


void
Indexer::UnlockAllIndexes()
{
	BeaconIndex *index ;
	for (int i = 0 ; (index = (BeaconIndex*)fIndexList.ItemAt(i)) != NULL ; 
		i++)
		index->Unlock() ;
}
