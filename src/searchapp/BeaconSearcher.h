/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _BEACON_SEARCHER_H_
#define _BEACON_SEARCHER_H_

#include <CLucene.h>

#include <Directory.h>
#include <List.h>
#include <Path.h>
#include <Volume.h>

using namespace lucene::analysis::standard ;


class BeaconSearcher {
	public:
		BeaconSearcher() ;
		~BeaconSearcher() ;
		wchar_t* GetNextHit() ;
		void Search(const char* query) ;
	
	private:
		char* GetIndexPath(BVolume *volume) ;

		BList				fSearcherList ;
		BList				fHits ;
		StandardAnalyzer	fStandardAnalyzer ;
} ;

#endif /* _BEACON_SEARCHER_H_ */

