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
#include <List.h>
#include <Path.h>
#include <Volume.h>

#include <CLucene.h>
using namespace lucene::index ;
using namespace lucene::analysis::standard ;
using namespace lucene::document ;


class BeaconIndex {
	public:
		BeaconIndex() ;
		BeaconIndex(const BVolume *volume) ;
		~BeaconIndex() ;

		status_t SetTo(const BVolume *volume) ;
		status_t AddDocument(const entry_ref *e_ref) ;
		void RemoveDocument(const entry_ref *e_ref) ;
		void RemoveDocument(const Document *doc) ;
		void Commit() ;
		void Close() ;
		status_t InitCheck() ;

	private:
		IndexWriter* OpenIndexWriter() ;
		IndexReader* OpenIndexReader() ;
		bool TranslatorAvailable(const entry_ref *e_ref) ;
		bool InIndexDirectory(const entry_ref *e_ref) ;
		Document* DocumentForRef(const entry_ref *e_ref) ;
		char* GetLastModified(const entry_ref *e_ref) ;

		status_t			fStatus ;
		StandardAnalyzer	fStandardAnalyzer ;
		BPath				fIndexPath ;
		BList				fIndexQueue ;
		BList				fDeleteQueue ;
} ;


#endif /* _BEACON_INDEX_H */

