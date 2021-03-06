/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "Feeder.h"
#include "support.h"

#include <Directory.h>
#include <FindDirectory.h>
#include <NodeInfo.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <string.h>


Feeder::Feeder(BHandler *target)
	: BLooper("feeder"),
	  fMonitorRemovableDevices(false),
	  fQueryList(1),
	  fIndexQueue(10),
	  fDeleteQueue(10),
	  fExcludeList(1),
	  fVolumeList(1),
	  fUpdateInterval(30 * 1000000)

{
	BMessage settings('sett') ;
	if (load_settings(&settings) == B_OK)
		LoadSettings(&settings) ;

	fTarget = target ;
	fMessageRunner = new BMessageRunner(fTarget,
		new BMessage(BEACON_UPDATE_INDEX), fUpdateInterval) ;
	
	Run() ;
}


void
Feeder::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_QUERY_UPDATE:
			HandleQueryUpdate(message) ;
			break ;
		case B_NODE_MONITOR :
			HandleDeviceUpdate(message) ;
			break ;
		default:
			BLooper :: MessageReceived(message) ;
	}
}


bool
Feeder::QuitRequested()
{
	return true ;
}


void
Feeder::LoadSettings(BMessage *settings)
{
	bool monitorRemovableDevices ;
	if (settings->FindBool("monitor_removable_devices",
		&monitorRemovableDevices) == B_OK)
		fMonitorRemovableDevices = monitorRemovableDevices ;
	
	bigtime_t updateInterval ;
	if (settings->FindInt64("update_interval", &updateInterval) == B_OK)
		fUpdateInterval = updateInterval ;

}


void
Feeder::SaveSettings(BMessage *settings)
{
	if (settings->ReplaceBool("monitor_removable_devices",
		fMonitorRemovableDevices) != B_OK)
		settings->AddBool("monitor_removable_devices", fMonitorRemovableDevices) ;
	
	if(settings->ReplaceInt64("update_interval", fUpdateInterval) != B_OK)
		settings->AddInt64("update_interval", fUpdateInterval) ;

}


void
Feeder::StartWatching()
{
	BVolume *volume = new BVolume ;
	
	while (fVolumeRoster.GetNextVolume(volume) != B_BAD_VALUE) {
		if ((volume->IsRemovable() && !fMonitorRemovableDevices)
			|| !volume->KnowsQuery())
				continue ;
		
		AddQuery(volume) ;
		volume = new BVolume ;
	}

	fVolumeRoster.StartWatching(this) ;
}


void
Feeder::AddQuery(BVolume *volume)
{
	fVolumeList.AddItem((void*)volume) ;

	BQuery *query = new BQuery ;	
	query->SetVolume(volume) ;
	
	// query->SetPredicate("name = *.txt") ;
	query->SetPredicate("last_modified > %now%") ;
	query->SetTarget(this) ;
	
	if (query->Fetch() == B_OK) {
		RetrieveStaticRefs(query) ;
		fQueryList.AddItem((void *)query) ;
	} else
		delete query ;
}


void
Feeder::AddEntry(entry_ref *ref)
{
	BEntry entry(ref) ;
	entry_ref *ref_ptr ;

	if(entry.InitCheck() == B_OK && entry.IsFile() && !is_hidden(ref)
		&& !Excluded(ref)) {
			ref_ptr = new entry_ref(*ref) ;
			fIndexQueue.AddItem((entry_ref*)ref_ptr) ;
	}
}


void
Feeder::RemoveQuery(BVolume *volume)
{
	BQuery *query ;
	BVolume* iter_volume ;
	
	for(int i = 0 ; (iter_volume = (BVolume*)fVolumeList.ItemAt(i)) != NULL
		; i++) {
			if (iter_volume->Device() == volume->Device()) {
				fVolumeList.RemoveItem(iter_volume) ;
				break ;
			}
	}

	for (int i = 0 ; (query = (BQuery*)fQueryList.ItemAt(i)) != NULL ; i++) {
		if (volume->Device() == query->TargetDevice()) {
			query->Clear() ;
			fQueryList.RemoveItem(query) ;
			delete query ;
			delete volume ;
			break ;
		}
	}
}


status_t
Feeder::GetNextUpdate(entry_ref *ref)
{
	return GetNextRef(&fIndexQueue, ref) ;
}


status_t
Feeder::GetNextRemoval(entry_ref *ref)
{
	return GetNextRef(&fDeleteQueue, ref) ;
}


status_t
Feeder::GetNextRef(BList* list, entry_ref *ref)
{
	status_t ret = B_BAD_VALUE ;
	if (!ref)
		return ret ;

	entry_ref *ref_ptr ;
	
	// Copy the next entry_ref into ref and delete the
	// one stored in the entry list.
	if (!list->IsEmpty()) {
		ref_ptr = (entry_ref *)list->ItemAt(0) ;
		list->RemoveItem(ref_ptr) ;
		*ref = *ref_ptr ;
		delete ref_ptr ;

		ret = B_OK ;
	} else {
		ref = NULL ;
		ret = B_ENTRY_NOT_FOUND ;
	}
	
	return ret ;
}


BList*
Feeder::GetVolumeList()
{
	return &fVolumeList ;
}


void
Feeder::RetrieveStaticRefs(BQuery *query)
{
	entry_ref ref ;

	while(query->GetNextRef(&ref) == B_OK)
		AddEntry(&ref) ;
}


void
Feeder::HandleQueryUpdate(BMessage *message)
{
	int32 opcode ;
	entry_ref *ref ;
	const char *name ;

	message->FindInt32("opcode", &opcode) ;

	switch (opcode) {
		case B_ENTRY_CREATED :
			ref = new entry_ref ;
			message->FindInt32("device", &ref->device) ;
			message->FindInt64("directory", &ref->directory) ;
			message->FindString("name", &name) ;
			ref->set_name(name) ;
			AddEntry(ref) ;
			break ;
		case B_ENTRY_REMOVED:
			ref = new entry_ref ;
			message->FindInt32("device", &ref->device) ;
			message->FindInt64("directory", &ref->directory) ;
			message->FindString("name", &name) ;
			ref->set_name(name) ;
			fDeleteQueue.AddItem((entry_ref*)ref) ;
			break ;
	}
}


void
Feeder::HandleDeviceUpdate(BMessage *message)
{
	int32 opcode ;
	BVolume *volume = new BVolume ;
	dev_t device ;

	message->FindInt32("opcode", &opcode) ;

	switch (opcode) {
		case B_DEVICE_MOUNTED :
			message->FindInt32("new device", &device) ;
			volume->SetTo(device) ;
			AddQuery(volume) ;
			// Forward the message to Indexer so that it can spawn
			// a new thread for volume.
			be_app->PostMessage(message) ;
			break ;
		
		case B_DEVICE_UNMOUNTED :
			message->FindInt32("device", &device) ;
			volume->SetTo(device) ;
			RemoveQuery(volume) ;
			be_app->PostMessage(message) ;
			break ;
	}

	delete volume ;
}


bool
Feeder::Excluded(entry_ref *ref)
{
	BEntry entry(ref) ;

	entry_ref* excludeRef ;
	BDirectory excludeDirectory ;
	BPath path ;
	for (int i = 0 ; (excludeRef = (entry_ref*)fExcludeList.ItemAt(i)) != NULL ; i++) {
		excludeDirectory.SetTo(excludeRef) ;
		if (excludeDirectory.Contains(&entry))
			return true ;
	}
	
	return false ;
}


