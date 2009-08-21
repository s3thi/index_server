/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "Logger.h"

#include <ctime>


Logger::Logger(const char* path, DebugLevel level, bool replace)
	: fStatus(B_NO_INIT),
	  fDebugLevel(level)
{
	if(replace)
		fLogFile = fopen(path, "w") ;
	else
		fLogFile = fopen(path, "a") ;
	
	if (fLogFile)
		fStatus = B_OK ;
	else
		fStatus = B_ERROR ;
}


Logger::~Logger()
{
	fclose(fLogFile) ;
}


status_t
Logger::InitCheck()
{
	return fStatus ;
}


void
Logger::Close()
{
	fclose(fLogFile) ;
}


void
Logger::Always(const char* format, ...)
{
	if(fStatus != B_OK)
		return ;

	va_list args ;
	va_start(args, format) ;
	SimpleMesssage(format, args) ;
}


void
Logger::Debug(const char* format, ...)
{
	if(fStatus != B_OK)
		return ;

	if(fDebugLevel != BEACON_DEBUG_NORMAL) {
		va_list args ;
		va_start(args, format) ;
		SimpleMesssage(format, args) ;
	}
}


void
Logger::MessageClass(const char* msgclass, const char* format, va_list args)
{
	if(fStatus != B_OK)
		return ;

	WriteTime() ;
	fprintf(fLogFile, "%s: ", msgclass) ;
	vfprintf(fLogFile, format, args) ;
	fprintf(fLogFile, "\n") ;
	fflush(fLogFile) ;
}


void
Logger::Error(const char* format, ...)
{
	if(fStatus != B_OK)
		return ;
	
	va_list args ;
	va_start(args, format) ;
	MessageClass("ERROR", format, args) ;
}


void
Logger::Verbose(const char* format, ...)
{
	if(fStatus != B_OK)
		return ;
	
	if(fDebugLevel == BEACON_DEBUG_VERBOSE) {
		va_list args ;
		va_start(args, format) ;
		SimpleMesssage(format, args) ;
	}
}


void
Logger::WriteTime()
{
	time_t unixTime = real_time_clock() ;
	struct tm *timeInfo = localtime(&unixTime) ;

	fprintf(fLogFile, "[%02d:%02d:%02d] ", timeInfo->tm_hour, timeInfo->tm_min,
		timeInfo->tm_sec) ;
	fflush(fLogFile) ;
}


void
Logger::SimpleMesssage(const char* format, va_list args)
{
	WriteTime() ;
	vfprintf(fLogFile, format, args) ;
	fprintf(fLogFile, "\n") ;
	fflush(fLogFile) ;
}
