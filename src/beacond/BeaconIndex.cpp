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
	if (InitCheck() == B_OK)
		Close() ;
	
	BDirectory dir ;
	volume->GetRootDirectory(&dir) ;
	fIndexPath.SetTo(&dir) ;
	fIndexPath.Append("index") ;
	logger->Verbose("Creating BeaconIndex on %s", fIndexPath.Path()) ;
	fIndexDirectory.SetTo(fIndexPath.Path()) ;

	if ((fIndexWriter = OpenIndexWriter()) != NULL)
		fStatus = B_OK ;
	else
		fStatus = B_ERROR ;

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
			// The index might be corrupted. Clean it out and create it again.
			// NOTE: This is a hack. Remove this from the final code.
			/*
			if (error.number() == CL_ERR_IO) {
				BEntry entry ;
				BDirectory indexDirectory(path.Path()) ;
				while (indexDirectory.GetNextEntry(&entry, false) == B_OK)
					entry.Remove() ;

				indexWriter = OpenIndexWriter() ;
				
			} else
				return indexWriter ;
			*/
			
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
	
	BEntry entry(&fIndexDirectory, NULL) ;
	if (!entry.Exists())
		return indexReader ;
	
	if (IndexReader::indexExists(fIndexPath.Path())) {
		try {
			indexReader = IndexReader::open(fIndexPath.Path()) ;
			return indexReader ;
		} catch (CLuceneError &error) {
			logger->Error("Could not open IndexReader.") ;
			logger->Error("Failed with error: %s", error.what()) ;
			return indexReader ;
		}
	} else
		return indexReader ;
}


void
BeaconIndex::Close()
{
	fIndexWriter->optimize() ;
	fIndexWriter->close() ;
	delete fIndexWriter ;
	fStatus = B_NO_INIT ;
}


bool
BeaconIndex::TranslatorAvailable(const entry_ref *ref)
{
	if (!ref)
		return false ;

	// For now, just return true if the file is a plain text file.
	// Will change in the future when we have more translators.
	BNode node(ref) ;
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
	if (!e_ref) {
		return B_BAD_VALUE ;
	}
	else if (!TranslatorAvailable(e_ref)) {
		return BEACON_NOT_SUPPORTED ;
	}
	else if (InIndexDirectory(e_ref)) {
		return BEACON_FILE_EXCLUDED ;
	}
	
	BPath path(e_ref) ;
	
	// Remove the document from the index.
	IndexReader *indexReader = OpenIndexReader() ;
	if(indexReader != NULL) {
		Term *term = new Term("path", path.Path()) ;
		indexReader->deleteDocuments(term) ;
		indexReader->close() ;
	} else {
		logger->Error("Did not check for duplicates of document: %s.",
			path.Path()) ;
	}
	// delete indexReader ;
	// delete term ;

	// Now add the new document.
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
	} catch (CLuceneError &error) {
		delete fileReader ;
		return B_ERROR ;
	}

	delete lastModified ;
	delete fileReader ;

	return B_OK ;
}


void
BeaconIndex::RemoveDocument(const Document *doc)
{
	fDeleteQueue.AddItem((void*)doc) ;
}


void
BeaconIndex::RemoveDocument(const entry_ref* e_ref)
{
	fDeleteQueue.AddItem((void*)DocumentForRef(e_ref)) ;
}


bool
BeaconIndex::InIndexDirectory(const entry_ref *e_ref)
{
	BEntry entry(e_ref) ;
	
	BPath path(&fIndexDirectory) ;
	if (fIndexDirectory.Contains(&entry))
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
