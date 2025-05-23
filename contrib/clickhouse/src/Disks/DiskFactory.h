#pragma once

#include <Disks/IDisk.h>
#include <Interpreters/Context_fwd.h>
#include <base/types.h>

#include <boost/noncopyable.hpp>
#include <DBPoco/Util/AbstractConfiguration.h>

#include <functional>
#include <map>
#include <unordered_map>


namespace DB
{

using DisksMap = std::map<String, DiskPtr>;
/**
 * Disk factory. Responsible for creating new disk objects.
 */
class DiskFactory final : private boost::noncopyable
{
public:
    using Creator = std::function<DiskPtr(
        const String & name,
        const DBPoco::Util::AbstractConfiguration & config,
        const String & config_prefix,
        ContextPtr context,
        const DisksMap & map,
        bool attach,
        bool custom_disk)>;

    static DiskFactory & instance();

    void registerDiskType(const String & disk_type, Creator creator);

    DiskPtr create(
        const String & name,
        const DBPoco::Util::AbstractConfiguration & config,
        const String & config_prefix,
        ContextPtr context,
        const DisksMap & map,
        bool attach = false,
        bool custom_disk = false,
        const std::unordered_set<String> & skip_types = {}) const;

private:
    using DiskTypeRegistry = std::unordered_map<String, Creator>;
    DiskTypeRegistry registry;
};

}
