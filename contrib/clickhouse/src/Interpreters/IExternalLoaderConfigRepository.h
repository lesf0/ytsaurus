#pragma once

#include <DBPoco/AutoPtr.h>
#include <DBPoco/Util/AbstractConfiguration.h>
#include <DBPoco/Timestamp.h>

#include <atomic>
#include <memory>
#include <string>
#include <set>

namespace DB
{
using LoadablesConfigurationPtr = DBPoco::AutoPtr<DBPoco::Util::AbstractConfiguration>;

/// Base interface for configurations source for Loadble objects, which can be
/// loaded with ExternalLoader. Configurations may came from filesystem (XML-files),
/// server memory (from database), etc. It's important that main result of this class
/// (LoadablesConfigurationPtr) may contain more than one loadable config,
/// each one with own key, which can be obtained with keys method,
/// for example multiple dictionaries can be defined in single .xml file.
class IExternalLoaderConfigRepository
{
public:
    /// Returns the name of the repository.
    virtual std::string getName() const = 0;

    /// Whether this repository is temporary:
    /// it's created and destroyed while executing the same query.
    virtual bool isTemporary() const { return false; }

    /// Return all available sources of loadables structure
    /// (all .xml files from fs, all entities from database, etc)
    virtual std::set<std::string> getAllLoadablesDefinitionNames() = 0;

    /// Checks that source of loadables configuration exist.
    virtual bool exists(const std::string & path) = 0;

    /// Returns entity last update time
    virtual std::optional<DBPoco::Timestamp> getUpdateTime(const std::string & /* path */) { return {}; }

    /// Load configuration from some concrete source to AbstractConfiguration
    virtual LoadablesConfigurationPtr load(const std::string & path) = 0;

    virtual ~IExternalLoaderConfigRepository() = default;
};

}
