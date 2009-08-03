/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "BeaconIndex.h"
#include "support.h"

#include <Node.h>
#include <NodeInfo.h>
#include <string.h>

using namespace lucene::util ;
using namespace lucene::search ;
using namespace lucene::queryParser ;


BeaconIndex::BeaconIndex()
	: fStatus(B_NO_INIT)
{
}


BeaconIndex::BeaconIndex(const BVolume *volume)
	: fStatus(B_NO_INIT),
	  fIndexQueue(10),
	  fDeleteQueue(10)
{
	SetTo(volume) ;
}


BeaconIndex::~BeaconIndex()
{
	Close() ;
}


status_t
BeaconIndex::InitCheck()
{
	return fStatus ;
}


status_t
BeaconIndex::SetTo(const BVolume *volume)
{
	if (fStatus == B_OK)
		Close() ;
	
	BDirectory dir ;
	volume->GetRootDirectory(&dir) ;
	fIndexPath.SetTo(&dir) ;
	fIndexPath.Append("index") ;
	
	fStatus = fIndexPath.InitCheck() ;
	return fStatus ;
}


IndexWriter*
BeaconIndex::OpenIndexWriter()
{
	IndexWriter* indexWriter = NULL ;
	
	if(create_directory(fIndexPath.Path(), 0777) < B_OK) {
		logger->Error("Could not create directory: %s", fIndexPath.Path()) ;
		return indexWriter ;
	}
	
	if (IndexReader::indexExists(fIndexPath.Path())) {
		try {
			indexWriter = new IndexWriter(fIndexPath.Path(), &fStandardAnalyzer, 
				false) ;
		} catch (CLuceneError &error) {
			logger->Error("Failed to open IndexWriter.") ;
			logger->Error("Try deleting the indexes and running the program again.") ;
			logger->Error("Failed with error: %s", error.what()) ;
		}
	} else {
		indexWriter = new IndexWriter(fIndexPath.Path(), &fStandardAnalyzer,
			true) ;
	}
	
	return indexWriter ;
}


IndexReader*
BeaconIndex::OpenIndexReader()
{
	IndexReader* indexReader = NULL ;
	
	BEntry entry(fIndexPath.Path(), NULL) ;
	if (!entry.Exists())
		return indexReader ;
	
	if (IndexReader::indexExists(fIndexPath.Path())) {
		try {
			indexReader = IndexReader::open(fIndexPath.Path()) ;
		} catch (CLuceneError &error) {
			logger->Error("Could not open IndexReader.") ;
			logger->Error("Failed with error: %s", error.what()) ;
		}
	}

	return indexReader ;
}


void
BeaconIndex::Commit()
{
	char* path ;
	Term* term ;
	int count ;
	
	// First, remove all duplicates (if they exist).
	IndexReader *reader = OpenIndexReader() ;
	if(reader == NULL && IndexReader::indexExists(fIndexPath.Path()))
		return ;
	else if (IndexReader::indexExists(fIndexPath.Path())) {
		for(int i = 0 ; (path = (char*)fIndexQueue.ItemAt(i)) != NULL ; i++) {
			term = new Term("path", path) ;
			count = reader->deleteDocuments(term) ;
			delete term ;
		}

		for(int i = 0 ; (path = (char*)fDeleteQueue.ItemAt(i)) != NULL ; i++) {
			logger->Verbose("Deleting: %s", path) ;
			term = new Term("path", path) ;
			reader->deleteDocuments(term) ;
			delete term ;
			delete path ;
		}

		fDeleteQueue.MakeEmpty() ;
		reader->close() ;
		delete reader ;
	}

	IndexWriter *writer = OpenIndexWriter() ;
	if(writer == NULL) {
		// TODO: Close and delete this BeaconIndex if opening
		// the IndexWriter fails.
		fStatus = B_ERROR ;
		return ;
	}
	
	FileReader *fileReader ;
	Document *doc ;
	
	for(int i = 0 ; (path = (char*)fIndexQueue.ItemAt(i)) != NULL ; i++) {
		logger->Verbose("Adding: %s", path) ;
		fileReader = new FileReader(path, "ASCII") ;

		doc = new Document ;
		doc->add(*(new Field("contents", fileReader, 
			Field::STORE_NO | Field::INDEX_TOKENIZED))) ;
		doc->add(*(new Field ("path", path,
			Field::STORE_YES | Field::INDEX_UNTOKENIZED))) ;

		try {
			writer->addDocument(doc) ;
		} catch (CLuceneError &error) {
			logger->Error("Could not index %s", path) ;
		}

		delete fileReader ;
		delete path ;
	}


	fIndexQueue.MakeEmpty() ;
	writer->close() ;
	delete writer ;
}


void
BeaconIndex::Close()
{
	fStatus = B_NO_INIT ;
}


bool
BeaconIndex::TranslatorAvailable(const entry_ref *e_ref)
{
	if (!e_ref)
		return false ;

	// For now, just return true if the file is a plain text file.
	// Will change in the future when we have more translators.
	BNode node(e_ref) ;
	BNodeInfo nodeInfo(&node) ;

	char MIMEString[B_MIME_TYPE_LENGTH] ;
	nodeInfo.GetType(MIMEString) ;
	if (!strcmp(MIMEString, "text/plain"))
		return true ;
	
	return false ;
}


status_t
BeaconIndex::AddDocument(const entry_ref *e_ref)
{
	if(fStatus != B_OK)
		return fStatus ;
	else if(!e_ref)
		return B_BAD_VALUE ;
	else if(!TranslatorAvailable(e_ref))
		return BEACON_NOT_SUPPORTED ;
	else if(InIndexDirectory(e_ref))
		return BEACON_FILE_EXCLUDED ;
	
	BPath path(e_ref) ;
	char *str_path = new char[B_PATH_NAME_LENGTH] ;
	strcpy(str_path, path.Path()) ;
	fIndexQueue.AddItem(str_path) ;
	
	return B_OK ;
}


status_t
BeaconIndex::RemoveDocument(const entry_ref* e_ref)
{
	if(!e_ref)
		return B_BAD_VALUE ;
	
	BPath path(e_ref) ;
	char* stringPath = new char[B_PATH_NAME_LENGTH] ;
	strcpy(stringPath, path.Path()) ;
	fDeleteQueue.AddItem(stringPath) ;
	
	return B_OK ;
}


bool
BeaconIndex::InIndexDirectory(const entry_ref *e_ref)
{
	BEntry entry(e_ref) ;
	BDirectory indexDir(fIndexPath.Path()) ;
	if (indexDir.Contains(&entry))
		return true ;
	
	return false ;
}


char*
BeaconIndex::GetLastModified(const entry_ref *e_ref)
{
	BEntry entry(e_ref) ;
	struct stat e_stat ;
	entry.GetStat(&e_stat) ;
	time_t lastModified = e_stat.st_mtime ;
	char* lastModifiedString = new char[11] ;
	snprintf(lastModifiedString, sizeof(lastModifiedString), "%d",
		lastModified) ;
	
	return lastModifiedString ;
}
