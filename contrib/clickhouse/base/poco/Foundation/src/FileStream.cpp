//
// FileStream.cpp
//
// Library: Foundation
// Package: Streams
// Module:  FileStream
//
// Copyright (c) 2007, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/FileStream.h"
#include "DBPoco/Exception.h"
#include "FileStream_POSIX.cpp"


namespace DBPoco {


FileIOS::FileIOS(std::ios::openmode defaultMode):
	_defaultMode(defaultMode)
{
	DB_poco_ios_init(&_buf);
}


FileIOS::~FileIOS()
{
}


void FileIOS::open(const std::string& path, std::ios::openmode mode)
{
	clear();
	_buf.open(path, mode | _defaultMode);
}


void FileIOS::close()
{
	if (!_buf.close())
	{
		setstate(ios_base::badbit);
	}
}


FileStreamBuf* FileIOS::rdbuf()
{
	return &_buf;
}


FileInputStream::FileInputStream():
	FileIOS(std::ios::in),
	std::istream(&_buf)
{
}


FileInputStream::FileInputStream(const std::string& path, std::ios::openmode mode):
	FileIOS(std::ios::in),
	std::istream(&_buf)
{
	open(path, mode);
}


FileInputStream::~FileInputStream()
{
}


FileOutputStream::FileOutputStream():
	FileIOS(std::ios::out),
	std::ostream(&_buf)
{
}


FileOutputStream::FileOutputStream(const std::string& path, std::ios::openmode mode):
	FileIOS(std::ios::out),
	std::ostream(&_buf)
{
	open(path, mode);
}


FileOutputStream::~FileOutputStream()
{
}


FileStream::FileStream():
	FileIOS(std::ios::in | std::ios::out),
	std::iostream(&_buf)
{
}


FileStream::FileStream(const std::string& path, std::ios::openmode mode):
	FileIOS(std::ios::in | std::ios::out),
	std::iostream(&_buf)
{
	open(path, mode);
}


FileStream::~FileStream()
{
}


} // namespace DBPoco
