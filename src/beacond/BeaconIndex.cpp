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
	if(writer == NULL)
		return ;
	
	FileReader *fileReader ;
	Document *doc ;
	
	for(int i = 0 ; (path = (char*)fIndexQueue.ItemAt(i)) != NULL ; i++) {
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
	logger->Verbose("Adding %s.", str_path) ;
	fIndexQueue.AddItem(str_path) ;
	
	/*
	// Remove the document from the index.
	Term *term = new Term("path", path.Path()) ;
	int count ;
	try {
		count = fIndexReader->deleteDocuments(term) ;
		logger->Verbose("Found and removed %d duplicates.", count) ;
	} catch (CLuceneError &error) {
		logger->Error("Could not check for duplicates of %s", path.Path()) ;
		logger->Error("Failed with error: %s", error.what()) ;
	}

	delete term ;
	
	// Now add the new document.
	status_t status = B_ERROR ;
	FileReader *fileReader = new FileReader(path.Path(), "ASCII") ;
	char* lastModified = GetLastModified(e_ref) ;

	Document *doc = new Document ;
	doc->add(*(new Field("contents", fileReader, 
		Field::STORE_NO | Field::INDEX_TOKENIZED))) ;
	doc->add(*(new Field ("path", path.Path(),
		Field::STORE_YES | Field::INDEX_UNTOKENIZED))) ;
	doc->add(*(new Field ("last_modified", lastModified,
		Field::STORE_YES | Field::INDEX_UNTOKENIZED))) ;

	try {
		fIndexWriter->addDocument(doc) ;
		status = B_OK ;
	} catch (CLuceneError &error) {
		status = B_ERROR ;
	}

	delete lastModified ;
	delete fileReader ;
	return status ;
	*/
}


void
BeaconIndex::RemoveDocument(const entry_ref* e_ref)
{
	BPath path(e_ref) ;
	char* stringPath = new char[B_PATH_NAME_LENGTH] ;
	strcpy(stringPath, path.Path()) ;
	fDeleteQueue.AddItem(stringPath) ;
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


Document*
BeaconIndex::DocumentForRef(const entry_ref *e_ref)
{
	BEntry entry(e_ref) ;
	BPath filePath(&entry) ;
	
	IndexSearcher indexSearcher(fIndexPath.Path()) ;
	
	Term term("path", filePath.Path()) ;
	Query *query = new TermQuery(&term) ;
	Hits *hits = indexSearcher.search(query) ;

	if (hits->length() == 0)
		return NULL ;
	else {
		Document *doc = new Document(hits->doc(0)) ;
		return doc ;
	}
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
