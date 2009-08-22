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
		void HandleDeviceUpdate(BMessage *message) ;
		BeaconIndex* FindIndex(dev_t device) ;
		BeaconIndex* FindIndex(char* path) ;

		Feeder 				*fQueryFeeder ;
		BList				fIndexList ;
} ;

#endif /* _INDEXER_H_ */
