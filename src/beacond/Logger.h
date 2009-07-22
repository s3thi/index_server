/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <Locker.h>
#include <SupportDefs.h>
#include <OS.h>

#include <stdio.h>


typedef enum _DebugLevel {
	BEACON_DEBUG_NORMAL,
	BEACON_DEBUG,
	BEACON_DEBUG_VERBOSE
} DebugLevel ;

class Logger {
	public:
		Logger(const char* path, DebugLevel level = BEACON_DEBUG_NORMAL,
			bool replace = true) ;
		~Logger() ;

		status_t InitCheck() ;
		void Close() ;
		void Always(const char* format, ...) ;
		void Debug(const char* format, ...) ;
		void Verbose(const char* format, ...) ;
		void Error(const char* format, ...) ;
		void Warning(const char* format, ...) ;

	private:
		void WriteTime() ;
		void SimpleMesssage(const char* format, va_list args) ;
		void MessageClass(const char* msgclass, const char* format, va_list args) ;
		
		FILE		*fLogFile ;
		status_t	fStatus ;
		DebugLevel	fDebugLevel ;
		BLocker		fLogFileLocker ;
} ;

#endif /* _LOGGER_H_ */
