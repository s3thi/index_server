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


Feeder :: Feeder(BHandler *target)
	: BLooper("feeder"),
	  fMonitorRemovableDevices(false),
	  fQueryList(1),
	  fEntryList(1),
	  fExcludeList(1),
	  fVolumeList(1),
	  fUpdateInterval(30 * 1000000)

{
	BMessage settings('sett') ;
	if (load_settings(&settings) == B_OK)
		LoadSettings(&settings) ;

	// Exclude the index directory.
	BPath indexPath ;
	find_directory(B_COMMON_DIRECTORY, &indexPath) ;
	indexPath.Append("data/index/") ;
	entry_ref *indexRef = new entry_ref ;
	get_ref_for_path(indexPath.Path(), indexRef) ;
	fExcludeList.AddItem((void *)indexRef) ;

	fTarget = target ;
	fMessageRunner = new BMessageRunner(fTarget, new BMessage(BEACON_UPDATE_INDEX),
		fUpdateInterval) ;
	
	Run() ;
}


void
Feeder :: MessageReceived(BMessage *message)
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
Feeder :: QuitRequested()
{
	return true ;
}


void
Feeder :: LoadSettings(BMessage *settings)
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
Feeder :: SaveSettings(BMessage *settings)
{
	if (settings->ReplaceBool("monitor_removable_devices",
		fMonitorRemovableDevices) != B_OK)
		settings->AddBool("monitor_removable_devices", fMonitorRemovableDevices) ;
	
	if(settings->ReplaceInt64("update_interval", fUpdateInterval) != B_OK)
		settings->AddInt64("update_interval", fUpdateInterval) ;

}


void
Feeder :: StartWatching()
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
Feeder :: AddQuery(BVolume *volume)
{
	fVolumeList.AddItem((void*)volume) ;

	BQuery *query = new BQuery ;	
	query->SetVolume(volume) ;
	query->SetPredicate("last_modified > %now%") ;
	query->SetTarget(this) ;
	
	if (query->Fetch() == B_OK) {
		RetrieveStaticRefs(query) ;
		fQueryList.AddItem((void *)query) ;
	} else
		delete query ;
}


void
Feeder :: AddEntry(entry_ref *ref)
{
	fEntryListLocker.Lock() ;
	BEntry entry(ref) ;
	entry_ref *ref_ptr ;

	if(entry.InitCheck() == B_OK && entry.IsFile() && !IsHidden(ref)
		&& !Excluded(ref)) {
			ref_ptr = new entry_ref(*ref) ;
			fEntryList.AddItem((void*)ref_ptr) ;
	}
	fEntryListLocker.Unlock() ;
}


void
Feeder :: RemoveQuery(BVolume *volume)
{
	BQuery *query ;
	BVolume* iter_volume ;
	
	for(int i = 0 ; (iter_volume = (BVolume*)fVolumeList.ItemAt(i)) != NULL ; i++) {
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
Feeder :: GetNextRef(entry_ref *ref)
{
	fEntryListLocker.Lock() ;
	if (!ref)
		return B_BAD_VALUE ;

	entry_ref *ref_ptr ;
	
	// Copy the next entry_ref into ref and delete the
	// one stored in the entry list.
	if (!fEntryList.IsEmpty()) {
		ref_ptr = (entry_ref *)fEntryList.ItemAt(0) ;
		fEntryList.RemoveItem((void*)ref_ptr) ;
		*ref = *ref_ptr ;
		delete ref_ptr ;

		fEntryListLocker.Unlock() ;
		return B_OK ;
	}
	
	fEntryListLocker.Unlock() ;
	ref = NULL ;
	return B_ENTRY_NOT_FOUND ;
}


BList*
Feeder :: GetVolumeList()
{
	return &fVolumeList ;
}


void
Feeder :: RetrieveStaticRefs(BQuery *query)
{
	entry_ref ref ;

	while (query->GetNextRef(&ref) == B_OK)
		AddEntry(&ref) ;
}


void
Feeder :: HandleQueryUpdate(BMessage *message)
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
			break ;
	}
}


void
Feeder :: HandleDeviceUpdate(BMessage *message)
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
Feeder :: Excluded(entry_ref *ref)
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


bool
Feeder :: IsHidden(entry_ref *ref)
{	
	if(ref->name[0] == '.')
		return true ;
	
	BEntry entry(ref) ;
	char name[B_FILE_NAME_LENGTH] ;
	while (entry.GetParent(&entry) != B_ENTRY_NOT_FOUND) {
		entry.GetName(name) ;
		if (strlen(name) > 1 && name[0] == '.')
			return true ;
	}

	return false ;
}

