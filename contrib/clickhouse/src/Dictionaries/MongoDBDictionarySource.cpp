#if 0
#include "MongoDBDictionarySource.h"
#include "DictionarySourceFactory.h"
#include "DictionaryStructure.h"
#include <Storages/StorageMongoDBSocketFactory.h>
#include <Storages/NamedCollectionsHelpers.h>

namespace DB
{

void registerDictionarySourceMongoDB(DictionarySourceFactory & factory)
{
    auto create_mongo_db_dictionary = [](
        const DictionaryStructure & dict_struct,
        const DBPoco::Util::AbstractConfiguration & config,
        const std::string & root_config_prefix,
        Block & sample_block,
        ContextPtr context,
        const std::string & /* default_database */,
        bool created_from_ddl)
    {
        const auto config_prefix = root_config_prefix + ".mongodb";
        auto named_collection = created_from_ddl ? tryGetNamedCollectionWithOverrides(config, config_prefix, context) : nullptr;

        String host, username, password, database, method, options, collection;
        UInt16 port;
        if (named_collection)
        {
            validateNamedCollection(
                *named_collection,
                /* required_keys */{"collection"},
                /* optional_keys */ValidateKeysMultiset<ExternalDatabaseEqualKeysSet>{
                "host", "port", "user", "password", "db", "database", "uri", "name", "method", "options"});

            host = named_collection->getOrDefault<String>("host", "");
            port = static_cast<UInt16>(named_collection->getOrDefault<UInt64>("port", 0));
            username = named_collection->getOrDefault<String>("user", "");
            password = named_collection->getOrDefault<String>("password", "");
            database = named_collection->getAnyOrDefault<String>({"db", "database"}, "");
            method = named_collection->getOrDefault<String>("method", "");
            collection = named_collection->getOrDefault<String>("collection", "");
            options = named_collection->getOrDefault<String>("options", "");
        }
        else
        {
            host = config.getString(config_prefix + ".host", "");
            port = config.getUInt(config_prefix + ".port", 0);
            username = config.getString(config_prefix + ".user", "");
            password = config.getString(config_prefix + ".password", "");
            database = config.getString(config_prefix + ".db", "");
            method = config.getString(config_prefix + ".method", "");
            collection = config.getString(config_prefix + ".collection");
            options = config.getString(config_prefix + ".options", "");
        }

        if (created_from_ddl)
            context->getRemoteHostFilter().checkHostAndPort(host, toString(port));

        return std::make_unique<MongoDBDictionarySource>(
            dict_struct,
            config.getString(config_prefix + ".uri", ""),
            host,
            port,
            username,
            password,
            method,
            database,
            collection,
            options,
            sample_block);
    };

    factory.registerSource("mongodb", create_mongo_db_dictionary);
}

}

#include <Common/logger_useful.h>
#error #include <DBPoco/MongoDB/Array.h>
#error #include <DBPoco/MongoDB/Connection.h>
#error #include <DBPoco/MongoDB/Cursor.h>
#error #include <DBPoco/MongoDB/Database.h>
#error #include <DBPoco/MongoDB/ObjectId.h>
#include <DBPoco/URI.h>
#include <DBPoco/Util/AbstractConfiguration.h>

// only after poco
// naming conflict:
// Poco/MongoDB/BSONWriter.h:54: void writeCString(const std::string & value);
// src/IO/WriteHelpers.h:146 #define writeCString(s, buf)
#include <IO/WriteHelpers.h>


namespace DB
{
namespace ErrorCodes
{
    extern const int NOT_IMPLEMENTED;
    extern const int UNSUPPORTED_METHOD;
    extern const int MONGODB_CANNOT_AUTHENTICATE;
}


static const UInt64 max_block_size = 8192;


MongoDBDictionarySource::MongoDBDictionarySource(
    const DictionaryStructure & dict_struct_,
    const std::string & uri_,
    const std::string & host_,
    UInt16 port_,
    const std::string & user_,
    const std::string & password_,
    const std::string & method_,
    const std::string & db_,
    const std::string & collection_,
    const std::string & options_,
    const Block & sample_block_)
    : dict_struct{dict_struct_}
    , uri{uri_}
    , host{host_}
    , port{port_}
    , user{user_}
    , password{password_}
    , method{method_}
    , db{db_}
    , collection{collection_}
    , options(options_)
    , sample_block{sample_block_}
    , connection{std::make_shared<DBPoco::MongoDB::Connection>()}
{

    StorageMongoDBSocketFactory socket_factory;
    if (!uri.empty())
    {
        // Connect with URI.
        connection->connect(uri, socket_factory);

        DBPoco::URI poco_uri(connection->uri());

        // Parse database from URI. This is required for correctness -- the
        // cursor is created using database name and collection name, so we have
        // to specify them properly.
        db = poco_uri.getPath();
        // getPath() may return a leading slash, remove it.
        if (!db.empty() && db[0] == '/')
        {
            db.erase(0, 1);
        }

        // Parse some other parts from URI, for logging and display purposes.
        host = poco_uri.getHost();
        port = poco_uri.getPort();
        user = poco_uri.getUserInfo();
        if (size_t separator = user.find(':'); separator != std::string::npos)
        {
            user.resize(separator);
        }
    }
    else
    {
        // Connect with host/port/user/etc through constructing the uri
        std::string uri_constructed("mongodb://" + host + ":" + std::to_string(port) + "/" + db + (options.empty() ? "" : "?" + options));
        connection->connect(uri_constructed, socket_factory);

        if (!user.empty())
        {
            DBPoco::MongoDB::Database poco_db(db);
            if (!poco_db.authenticate(*connection, user, password, method.empty() ? DBPoco::MongoDB::Database::AUTH_SCRAM_SHA1 : method))
                throw Exception(ErrorCodes::MONGODB_CANNOT_AUTHENTICATE, "Cannot authenticate in MongoDB, incorrect user or password");
        }
    }
}


MongoDBDictionarySource::MongoDBDictionarySource(const MongoDBDictionarySource & other)
    : MongoDBDictionarySource{
        other.dict_struct, other.uri, other.host, other.port, other.user, other.password, other.method, other.db,
        other.collection, other.options, other.sample_block
    }
{
}

MongoDBDictionarySource::~MongoDBDictionarySource() = default;

QueryPipeline MongoDBDictionarySource::loadAll()
{
    return QueryPipeline(std::make_shared<MongoDBSource>(connection, db, collection, DBPoco::MongoDB::Document{}, sample_block, max_block_size));
}

QueryPipeline MongoDBDictionarySource::loadIds(const std::vector<UInt64> & ids)
{
    if (!dict_struct.id)
        throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "'id' is required for selective loading");

    DBPoco::MongoDB::Document query;

    /** NOTE: While building array, DBPoco::MongoDB requires passing of different unused element names, along with values.
      * In general, DBPoco::MongoDB is quite inefficient and bulky.
      */

    DBPoco::MongoDB::Array::Ptr ids_array(new DBPoco::MongoDB::Array);
    for (const UInt64 id : ids)
        ids_array->add(DB::toString(id), static_cast<Int32>(id));

    query.addNewDocument(dict_struct.id->name).add("$in", ids_array);

    return QueryPipeline(std::make_shared<MongoDBSource>(connection, db, collection, query, sample_block, max_block_size));
}


QueryPipeline MongoDBDictionarySource::loadKeys(const Columns & key_columns, const std::vector<size_t> & requested_rows)
{
    if (!dict_struct.key)
        throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "'key' is required for selective loading");

    DBPoco::MongoDB::Document query;
    DBPoco::MongoDB::Array::Ptr keys_array(new DBPoco::MongoDB::Array);

    for (const auto row_idx : requested_rows)
    {
        auto & key = keys_array->addNewDocument(DB::toString(row_idx));

        const auto & key_attributes = *dict_struct.key;
        for (size_t attribute_index = 0; attribute_index < key_attributes.size(); ++attribute_index)
        {
            const auto & key_attribute = key_attributes[attribute_index];

            switch (key_attribute.underlying_type)
            {
                case AttributeUnderlyingType::UInt8:
                case AttributeUnderlyingType::UInt16:
                case AttributeUnderlyingType::UInt32:
                case AttributeUnderlyingType::UInt64:
                case AttributeUnderlyingType::Int8:
                case AttributeUnderlyingType::Int16:
                case AttributeUnderlyingType::Int32:
                case AttributeUnderlyingType::Int64:
                {
                    key.add(key_attribute.name, static_cast<Int32>(key_columns[attribute_index]->get64(row_idx)));
                    break;
                }
                case AttributeUnderlyingType::Float32:
                case AttributeUnderlyingType::Float64:
                {
                    key.add(key_attribute.name, key_columns[attribute_index]->getFloat64(row_idx));
                    break;
                }
                case AttributeUnderlyingType::String:
                {
                    String loaded_str((*key_columns[attribute_index])[row_idx].safeGet<String>());
                    /// Convert string to ObjectID
                    if (key_attribute.is_object_id)
                    {
                        DBPoco::MongoDB::ObjectId::Ptr loaded_id(new DBPoco::MongoDB::ObjectId(loaded_str));
                        key.add(key_attribute.name, loaded_id);
                    }
                    else
                    {
                        key.add(key_attribute.name, loaded_str);
                    }
                    break;
                }
                default:
                    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported dictionary attribute type for MongoDB dictionary source");
            }
        }
    }

    /// If more than one key we should use $or
    query.add("$or", keys_array);

    return QueryPipeline(std::make_shared<MongoDBSource>(connection, db, collection, query, sample_block, max_block_size));
}

std::string MongoDBDictionarySource::toString() const
{
    return fmt::format("MongoDB: {}.{},{}{}:{}", db, collection, (user.empty() ? " " : " " + user + '@'), host, port);
}

}
#endif
