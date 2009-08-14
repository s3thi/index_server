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


int32 add_document(void *data) ;

struct index_ref {
	BeaconIndex *index ;
	dev_t device ;
	thread_id thread ;
	sem_id sem ;
	BLocker locker ;
} ;

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
		void HandleDeviceUpdate(BMessage *message) ;
		index_ref* FindIndexRef(dev_t device) ;
		void LockAllIndexes() ;
		void UnlockAllIndexes() ;

		Feeder 				*fQueryFeeder ;
		BList				fIndexRefList ;
} ;

#endif /* _INDEXER_H_ */
