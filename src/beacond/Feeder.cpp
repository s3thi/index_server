/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "Feeder.h"
#include "support.h"

#include <NodeInfo.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <string.h>

#include <iostream>
using namespace std ;

Feeder :: Feeder()
	: BLooper("feeder"),
	  fMonitorRemovableDevices(false),
	  fQueryList(1),
	  fEntryList(1)
{
	BMessage settings('sett') ;
	if(load_settings(&settings) == B_OK)
		LoadSettings(&settings) ;
	Run() ;
}


void Feeder :: MessageReceived(BMessage *message)
{
	switch(message->what) {
		case B_QUERY_UPDATE:
			HandleQueryUpdate(message) ;
			break ;
		case B_NODE_MONITOR :
			break ;
		default:
			BLooper :: MessageReceived(message) ;
	}
}


bool Feeder :: QuitRequested()
{
	return true ;
}


void Feeder :: LoadSettings(BMessage *settings)
{
	bool monitorRemovableDevices ;
	if(settings->FindBool("monitor_removable_devices",
		&monitorRemovableDevices) == B_OK)
		fMonitorRemovableDevices = monitorRemovableDevices ;
}


void Feeder :: SaveSettings(BMessage *settings)
{
	if(settings->ReplaceBool("monitor_removable_devices",
		fMonitorRemovableDevices) != B_OK)
		settings->AddBool("monitor_removable_devices", fMonitorRemovableDevices) ;
}


void Feeder :: StartWatching()
{
	BVolume *volume = new BVolume ;
	
	while(fVolumeRoster.GetNextVolume(volume) != B_BAD_VALUE) {
		if((volume->IsRemovable() && !fMonitorRemovableDevices)
			|| !volume->KnowsQuery())
			continue ;
		
		AddQuery(volume) ;
		volume = new BVolume ;
	}

	fVolumeRoster.StartWatching(this) ;
}


void Feeder :: AddQuery(BVolume *volume)
{
	BQuery *query = new BQuery ;
	
	query->SetVolume(volume) ;
	query->SetPredicate("last_modified > %now%") ;
	query->SetTarget(this) ;
	
	if(query->Fetch() == B_OK) {
		RetrieveStaticRefs(query) ;
		fQueryList.AddItem((void *)query) ;
	} else
		delete query ;
}


bool Feeder :: TranslatorAvailable(entry_ref *ref)
{
	// For now, just return true if the file is a plain text file.
	// Will change in the future when we have more translators.
	BNode node(ref) ;
	BNodeInfo nodeInfo(&node) ;

	char MIMEString[B_MIME_TYPE_LENGTH] ;
	nodeInfo.GetType(MIMEString) ;
	if(!strcmp(MIMEString, "text/plain"))
		return true ;
	
	return false ;
}


status_t Feeder :: GetNextRef(entry_ref *ref)
{
	entry_ref *ref_ptr ;
	
	// Copy the next entry_ref into ref and delete the
	// one stored in the entry list.
	if(!fEntryList.IsEmpty()) {
		ref_ptr = (entry_ref *)fEntryList.ItemAt(0) ;
		fEntryList.RemoveItem((void*)ref_ptr) ;
		*ref = *ref_ptr ;
		delete ref_ptr ;

		return B_OK ;
	}
	
	ref = NULL ;
	return B_ENTRY_NOT_FOUND ;
}


void Feeder :: RetrieveStaticRefs(BQuery *query)
{
	entry_ref *ref = new entry_ref ;
	BEntry entry ;

	while(query->GetNextRef(ref) == B_OK) {
		entry.SetTo(ref) ;

		if(entry.IsFile() && TranslatorAvailable(ref)) {
			fEntryList.AddItem((void*)ref) ;
			ref = new entry_ref ;
		}
	}
}


void Feeder :: HandleQueryUpdate(BMessage *message)
{
	int32 opcode ;
	entry_ref *ref ;
	const char *name ;
	message->FindInt32("opcode", &opcode) ;
	switch(opcode) {
		case B_ENTRY_CREATED :
			ref = new entry_ref ;
			message->FindInt32("device", &ref->device) ;
			message->FindInt64("directory", &ref->directory) ;
			message->FindString("name", &name) ;
			ref->set_name(name) ;
			fEntryList.AddItem((void*)ref) ;
			break ;
		case B_ENTRY_REMOVED:
			break ;
	}
}
