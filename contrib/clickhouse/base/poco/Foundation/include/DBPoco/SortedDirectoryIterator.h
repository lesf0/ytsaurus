//
// SortedDirectoryIterator.h
//
// Library: Foundation
// Package: Filesystem
// Module:  DirectoryIterator
//
// Definition of the SortedDirectoryIterator class.
//
// Copyright (c) 2004-2012, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef DB_Foundation_SortedDirectoryIterator_INCLUDED
#define DB_Foundation_SortedDirectoryIterator_INCLUDED

#include <deque>
#include "DBPoco/DirectoryIterator.h"
#include "DBPoco/File.h"
#include "DBPoco/Foundation.h"
#include "DBPoco/Path.h"


namespace DBPoco
{

class Foundation_API SortedDirectoryIterator : public DirectoryIterator
/// The SortedDirectoryIterator class is similar to
/// DirectoryIterator class, but places directories before files
/// and sorts content alphabetically.
{
public:
    SortedDirectoryIterator();
    /// Creates the end iterator.

    SortedDirectoryIterator(const std::string & path);
    /// Creates a directory iterator for the given path.

    SortedDirectoryIterator(const DirectoryIterator & iterator);
    /// Creates a directory iterator for the given path.

    SortedDirectoryIterator(const File & file);
    /// Creates a directory iterator for the given file.

    SortedDirectoryIterator(const Path & path);
    /// Creates a directory iterator for the given path.

    virtual ~SortedDirectoryIterator();
    /// Destroys the DirsFirstDirectoryIterator.

    virtual SortedDirectoryIterator & operator++(); // prefix

private:
    bool _is_finished;
    std::deque<std::string> _directories;
    std::deque<std::string> _files;

    void next();
    /// Take next item
    void scan();
    /// Scan directory to collect its children directories and files
};


} // namespace DBPoco

#endif //DB_Foundation_SortedDirectoryIterator_INCLUDED
