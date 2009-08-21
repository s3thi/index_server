/*
 * Copyright 2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ankur Sethi (get.me.ankur@gmail.com)
 */

#include "StringPositionIO.h"

#include <cstring>
#include <cstdio>


StringPositionIO::StringPositionIO()
	: offset(0)
{
}


ssize_t
StringPositionIO::Read(void* buffer, size_t numBytes)
{
	content.CopyInto((char*)buffer, offset, numBytes) ;
}


ssize_t
StringPositionIO::ReadAt(off_t position, void* buffer, size_t numBytes)
{
	content.CopyInto((char*)buffer, position, numBytes) ;
}


off_t
StringPositionIO::Seek(off_t position, uint32 mode)
{
	if (mode == SEEK_SET) {
		if (position < content.Length())
			offset = position ;
		else
			offset = content.Length() - 1 ;
	}
	else if (mode == SEEK_CUR) {
		if ((offset + position) < content.Length()) 
			offset += position ;
		else
			offset = content.Length() - 1 ;
	}
	else if (mode == SEEK_END) {
		if ((content.Length() - position) < 0)
			offset = 0 ;
		else
			offset = content.Length() - position - 1 ;
	}

	return offset ;
}


off_t
StringPositionIO::Position() const
{
	return offset ;
}


status_t
StringPositionIO::SetSize(off_t numBytes)
{
	content.Truncate(numBytes) ;
	if (content.Length() < offset)
		offset = content.Length() - 1 ;
	
	return B_OK ;
}


ssize_t
StringPositionIO::Write(const void* buffer, size_t numBytes)
{
	content.Insert((char*)buffer, numBytes, offset) ;
	offset += numBytes ;
}


ssize_t
StringPositionIO::WriteAt(off_t position, const void* buffer, size_t numBytes)
{
	content.Insert((char*)buffer, numBytes, position) ;
}


const char*
StringPositionIO::Content()
{
	return content.String() ;
}


uint32
StringPositionIO::Length()
{
	return content.Length() ;
}
