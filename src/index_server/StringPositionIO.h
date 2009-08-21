/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#ifndef _STRING_POSITION_IO_H
#define _STRING_POSITION_IO_H

#include <DataIO.h>
#include <String.h>


class StringPositionIO : public BPositionIO {
	public:
		StringPositionIO() ;

		ssize_t Read(void* buffer, size_t numBytes) ;
		ssize_t ReadAt(off_t position, void* buffer, size_t numBytes) ;

		off_t Seek(off_t position, uint32 mode) ;
		off_t Position() const ;

		status_t SetSize(off_t numBytes) ;

		ssize_t Write(const void* buffer, size_t numBytes) ;
		ssize_t WriteAt(off_t position, const void* buffer,
			size_t numBytes) ;

		const char* Content() ;
		uint32 Length() ;

	private:
		BString		content ;
		off_t		offset ;
} ;

#endif /* _STRING_POSITION_IO_H */

