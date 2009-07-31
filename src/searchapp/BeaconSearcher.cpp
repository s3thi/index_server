/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "BeaconSearcher.h"

#include <cstring>

#include <Alert.h>
#include <VolumeRoster.h>

using namespace lucene::document ;
using namespace lucene::search ;
using namespace lucene::index ;
using namespace lucene::queryParser ;


BeaconSearcher::BeaconSearcher()
{
	BVolumeRoster volumeRoster ;
	BVolume volume ;
	IndexSearcher *indexSearcher ;
	char* indexPath ;
	while(volumeRoster.GetNextVolume(&volume) == B_OK) {
		indexPath = GetIndexPath(&volume) ;
		
		if(indexPath != NULL) {
			indexSearcher = new IndexSearcher(indexPath) ;
			fSearcherList.AddItem(indexSearcher) ;
		}
	}
}


BeaconSearcher::~BeaconSearcher()
{
	IndexSearcher *indexSearcher ;
	while(fSearcherList.CountItems() > 0) {
		indexSearcher = (IndexSearcher*)fSearcherList.ItemAt(0) ;
		indexSearcher->close() ;
		fSearcherList.RemoveItem((int32)0) ;
	}
}


char*
BeaconSearcher::GetIndexPath(BVolume *volume)
{
	BDirectory dir ;
	volume->GetRootDirectory(&dir) ;
	BPath path(&dir) ;
	path.Append("index/") ;
	
	if(IndexReader::indexExists(path.Path())) {
		char *indexPath = new char[B_PATH_NAME_LENGTH] ;
		strcpy(indexPath, path.Path()) ;
		return indexPath ;
	}
	else
		return NULL ;
}


void
BeaconSearcher::Search(const char* stringQuery)
{
	IndexSearcher *indexSearcher ;
	Hits *hits ;
	Query *luceneQuery ;
	Document doc ;
	Field *field ;
	char *path ;
	for(int i = 0 ; (indexSearcher = (IndexSearcher*)fSearcherList.ItemAt(i))
		!= NULL ; i++) {
			luceneQuery = QueryParser::parse(stringQuery, "contents",
				&fStandardAnalyzer) ;

			hits = indexSearcher->search(luceneQuery) ;

			for(int j = 0 ; j < hits->length() ; j++) {
				doc = hits->doc(j) ;
				field = doc.getField("path") ;
				path = new char[B_PATH_NAME_LENGTH] ;
				strcpy(path, field->stringValue()) ;
				fHits.AddItem(path) ;
			}
	}
}


char*
BeaconSearcher::GetNextHit()
{
	if(fHits.CountItems() != 0) {
		char* path = (char*)fHits.ItemAt(0) ;
		fHits.RemoveItem((int32)0) ;
		return path ;
	}
	
	return NULL ;
}

