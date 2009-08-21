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
#include <StringPositionIO.h>
#include <TranslatorFormats.h>

#include <cstring>

using namespace lucene::util ;
using namespace lucene::search ;
using namespace lucene::queryParser ;


BeaconIndex::BeaconIndex(const BVolume *volume)
	: fStatus(B_NO_INIT),
	  fIndexQueue(10),
	  fDeleteQueue(10)
{
	fTranslatorRoster = BTranslatorRoster::Default() ;
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
	
	fIndexVolume.SetTo(volume->Device()) ;
	BDirectory dir ;
	volume->GetRootDirectory(&dir) ;
	fIndexPath.SetTo(&dir) ;
	fIndexPath.Append("index") ;

	// Empty the queues.
	fIndexQueueLocker.Lock() ;
	fDeleteQueueLocker.Lock() ;
	fIndexQueue.MakeEmpty() ;
	fDeleteQueue.MakeEmpty() ;
	fDeleteQueueLocker.Unlock() ;
	fIndexQueueLocker.Unlock() ;

	if (!IndexReader::indexExists(fIndexPath.Path())) {
		fStatus = BEACON_FIRST_RUN ;
		fStatus = FirstRun() ;
	}
	else
		fStatus = fIndexPath.InitCheck() ;
	
	return fStatus ;
}


IndexWriter*
BeaconIndex::OpenIndexWriter()
{
	IndexWriter* indexWriter = NULL ;
	if (IndexReader::indexExists(fIndexPath.Path())) {
		try {
			indexWriter = new IndexWriter(fIndexPath.Path(), &fStandardAnalyzer, 
				false) ;
		} catch (CLuceneError &error) {
			logger->Error("Failed to open IndexWriter on device %d",
				fIndexVolume.Device()) ;
			logger->Error("Try deleting the indexes.") ;
			logger->Error("Failed with CLuceneError: %s", error.what()) ;
		}
	} else
		indexWriter = new IndexWriter(fIndexPath.Path(), &fStandardAnalyzer,
			true) ;
	
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
	fIndexQueueLocker.Lock() ;
	fDeleteQueueLocker.Lock() ;
	
	logger->Verbose("Calling commit on device %d", fIndexVolume.Device()) ;
	logger->Verbose("%d items in index queue, %d items in delete queue",
		fIndexQueue.CountItems(), fDeleteQueue.CountItems()) ;
	
	char* path ;
	Term* term ;
	wchar_t *wPath ;

	IndexReader *reader = OpenIndexReader() ;
	if (reader == NULL && IndexReader::indexExists(fIndexPath.Path()))
		return ;
	else if (reader != NULL && IndexReader::indexExists(fIndexPath.Path())) {
		// First, remove all duplicates (if they exist).
		for (int i = 0 ; (path = (char*)fIndexQueue.ItemAt(i)) != NULL ;
			i++) {
			wPath = to_wchar(path) ;
			if (wPath == NULL)
				continue ;
			
			term = new Term(_T("path"), wPath) ;
			reader->deleteDocuments(term) ;
			delete term ;
		}

		for (int i = 0 ; (path = (char*)fDeleteQueue.ItemAt(i)) != NULL ;
			i++) {
			wPath = to_wchar(path) ;
			if (wPath == NULL)
				continue ;
			
			term = new Term(_T("path"), wPath) ;
			reader->deleteDocuments(term) ;
			
			delete term ;
			delete path ;
			delete wPath ;
		}

		fDeleteQueue.MakeEmpty() ;
		reader->close() ;
		delete reader ;
	}

	IndexWriter *writer = OpenIndexWriter() ;
	if (writer == NULL) {
		fStatus = B_ERROR ;
		return ;
	}
	
	FileReader *fileReader ;
	BFile inFile, outFile ;
	Document *doc ;
	char tempPath[] = "/boot/var/tmp/index_server" ;
	
	for (int i = 0 ; (path = (char*)fIndexQueue.ItemAt(i)) != NULL ; i++) {
		inFile.SetTo(path, B_READ_ONLY) ;
		outFile.SetTo(tempPath, B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE) ;
		
		if (fTranslatorRoster->Translate(&inFile, NULL, NULL, &outFile, 'TEXT') == B_OK) {
			inFile.Unset() ; 
			outFile.Unset() ;
			
			fileReader = new FileReader(tempPath, "UTF-8") ;

			wPath = to_wchar(path) ;
			if (wPath == NULL)
				continue ;

			doc = new Document ;
			doc->add(*(new Field(_T("contents"), fileReader, 
				Field::STORE_NO | Field::INDEX_TOKENIZED))) ;
			doc->add(*(new Field (_T("path"), wPath,
				Field::STORE_YES | Field::INDEX_UNTOKENIZED))) ;

			try {
				writer->addDocument(doc) ;
			} catch (CLuceneError &error) {
				logger->Error("Could not index %s", path) ;
				logger->Error("%s", error.what()) ;
			}

			delete path ;
			delete wPath ;
		}
	}


	fIndexQueue.MakeEmpty() ;
	writer->close() ;
	delete writer ;

	fDeleteQueueLocker.Unlock() ;
	fIndexQueueLocker.Unlock() ;
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

	BFile file(e_ref, B_READ_ONLY) ;
	translator_info translatorInfo ;

	if (fTranslatorRoster->Identify(&file, NULL,
		&translatorInfo, 0, NULL, B_TRANSLATOR_TEXT) == B_OK)
		return true ;
	
	return false ;
}


status_t
BeaconIndex::AddDocument(const entry_ref *e_ref)
{
	if (!(fStatus == B_OK || fStatus == BEACON_FIRST_RUN))
		return fStatus ;
	else if (!e_ref)
		return B_BAD_VALUE ;
	else if (!TranslatorAvailable(e_ref))
		return BEACON_NOT_SUPPORTED ;
	else if (InIndexDirectory(e_ref))
		return BEACON_FILE_EXCLUDED ;
	
	fIndexQueueLocker.Lock() ;

	BPath path(e_ref) ;
	char *str_path = new char[B_PATH_NAME_LENGTH] ;
	strcpy(str_path, path.Path()) ;
	fIndexQueue.AddItem(str_path) ;
	
	fIndexQueueLocker.Unlock() ;

	return B_OK ;
}


status_t
BeaconIndex::RemoveDocument(const entry_ref* e_ref)
{
	fDeleteQueueLocker.Lock() ;

	status_t ret = B_BAD_VALUE ;
	
	if (!e_ref)
		return ret ;
	
	BPath path(e_ref) ;
	if ((ret = path.InitCheck()) != B_OK)
		return ret ;

	char* stringPath = new char[B_PATH_NAME_LENGTH] ;
	strcpy(stringPath, path.Path()) ;
	fDeleteQueue.AddItem(stringPath) ;
	ret = B_OK ;
	
	fDeleteQueueLocker.Unlock() ;

	return ret ;
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


status_t
BeaconIndex::FirstRun()
{
	logger->Always("Calling FirstRun() on device ID %d", fIndexVolume.Device()) ;
	status_t err ;
	if ((err = create_directory(fIndexPath.Path(), 0777)) != B_OK) {
		logger->Error("Could not create directory: %s", fIndexPath.Path()) ;
		return err ;
	}

	BDirectory dir ;
	fIndexVolume.GetRootDirectory(&dir) ;
	err = AddAllDocuments(&dir) ;
	Commit() ;
	return err ;
}


status_t
BeaconIndex::AddAllDocuments(BDirectory *dir)
{
	entry_ref ref ;
	BEntry entry ;
	BDirectory d ;
	status_t err = B_OK ;
	
	while (dir->GetNextRef(&ref) == B_OK) {
		entry.SetTo(&ref) ;
		
		if ((err = entry.InitCheck()) != B_OK)
			break ;
		else if (is_hidden(&ref) || entry.IsSymLink())
			continue ;

		if (entry.IsFile())
			err = AddDocument(&ref) ;
		else {
			d.SetTo(&ref) ;
			AddAllDocuments(&d) ;
		}
	}

	return err ;
}


dev_t
BeaconIndex::Device()
{
	return fIndexVolume.Device() ;
}

