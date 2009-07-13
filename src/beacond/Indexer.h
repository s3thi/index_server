/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _INDEXER_H_
#define _INDEXER_H_

#include "BeaconIndex.h"
#include "Feeder.h"

#include <Application.h>
#include <Locker.h>

#include <CLucene.h>
using namespace lucene::index ;
using namespace lucene::analysis::standard ;
using namespace lucene::util ;


struct index_ref {
	BeaconIndex *index ;
	BList *entryList ;
	dev_t device ;
	thread_id thread ;
	sem_id sem ;
	BLocker entryListLocker ;
} ;

int32 add_document(void *data) ;

class Indexer : public BApplication {
	public :
		Indexer() ;
		virtual void ReadyToRun() ;
		virtual void MessageReceived(BMessage *message) ;
		virtual bool QuitRequested() ;
		
	private :
		void SaveSettings(BMessage *message) ;
		void LoadSettings(BMessage *message) ;
		void UpdateIndex() ;
		status_t InitIndex(BVolume *volume) ;
		IndexWriter* OpenIndex(BDirectory *dir) ;
		bool TranslatorAvailable(entry_ref *ref) ;
		void HandleDeviceUpdate(BMessage *message) ;
		void CloseIndex(BVolume *volume) ;
		index_ref* FindIndexWriterRef(dev_t device) ;

		Feeder 				*fQueryFeeder ;
		StandardAnalyzer 	fStandardAnalyzer ;
		FileReader			*fFileReader ;
		BList				fIndexWriterList ;
} ;


#endif /* _INDEXER_H_ */
