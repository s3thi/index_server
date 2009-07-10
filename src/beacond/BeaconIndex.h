/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _BEACON_INDEX_H_
#define _BEACON_INDEX_H_

#include <Directory.h>
#include <Volume.h>

#include <CLucene.h>
using namespace lucene::index ;
using namespace lucene::analysis::standard ;


class BeaconIndex {
	public:
		BeaconIndex(BDirectory *dir) ;
		~BeaconIndex() ;

		status_t SetTo(BDirectory *dir) ;
		status_t AddDocument(entry_ref *e_ref) ;
		void Close() ;
		status_t InitCheck() ;

	private:
		IndexWriter* OpenIndex(BDirectory *dir) ;
		bool TranslatorAvailable(entry_ref *e_ref) ;
		bool Excluded(entry_ref *e_ref) ;

		status_t			fStatus ;
		IndexWriter			*fIndexWriter ;
		StandardAnalyzer	fStandardAnalyzer ;
		BDirectory			fIndexDirectory ;
} ;


#endif /* _BEACON_INDEX_H */

