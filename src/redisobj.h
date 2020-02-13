/*
    redisobj.h
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-01-29
    Push Chen

    Copyright 2015-2016 MeetU Infomation and Technology Inc. All rights reserved.
*/

#pragma once

#ifndef DHBOC_REDISOBJ_H__
#define DHBOC_REDISOBJ_H__

#include <peco/peco.h>
using namespace pe;
using namespace pe::co;
using namespace pe::co::net;

#include "redismgr.h"

// Utils
namespace dhboc { namespace redis {
    // The redis connector group type
    typedef std::map< std::string, std::function< std::string( const std::string&) > >      format_map_t;
    typedef std::map< std::string, std::function< bool( const std::string&) > >             filter_map_t;

    extern const filter_map_t                   empty_filter_map;
    extern const std::vector< std::string >     empty_filter_keys;

    enum rtype {
        R_STRING,
        R_NUMBER,
        R_NULL
    };

    struct properity_t {
        std::string             key;
        rtype                   type;
        bool                    unique;
        std::string             dvalue;
    };

    // Enable Item Cache
    void enable_item_cache( size_t cache_size = 10000 );

    // Register an object in the memory
    int register_object( 
        const std::string& name, 
        const std::vector< properity_t >& properties
    );

    // Unregister an object type
    int unregister_object(
        const std::string& name
    );

    // Get all objects' name
    std::list< std::string > all_objects();

    // Get the count of specifial object
    int count_object( redis_connector_t rg, const std::string& name );
    int count_object( const std::string& name );

    // Get the list of value
    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name,
        int offset = 0,
        int page_size = -1,
        const std::vector< std::string >& filter_keys = empty_filter_keys,
        const filter_map_t& filters = empty_filter_map
    );
    Json::Value list_object(
        const std::string& name,
        int offset = 0,
        int page_size = -1,
        const std::vector< std::string >& filter_keys = empty_filter_keys,
        const filter_map_t& filters = empty_filter_map
    );

    // List only ids
    std::vector< std::string > list_object_ids( 
        redis_connector_t rg, const std::string& name,
        const filter_map_t& filters = empty_filter_map
    );
    std::vector< std::string > list_object_ids( 
        const std::string& name,
        const filter_map_t& filters = empty_filter_map
    );

    Json::Value list_object( 
        redis_connector_t rg, 
        const std::string& name, 
        int offset, 
        int page_size,
        const filter_map_t& filters = empty_filter_map
    );
    Json::Value list_object( 
        const std::string& name, 
        int offset, 
        int page_size,
        const filter_map_t& filters = empty_filter_map
    );

    // Recurse add a json object
    // if the object does not existed, add it, and return 0
    // if the object existed, update it and return -1
    // on error, return > 1
    int patch_object( 
        redis_connector_t rg,
        const std::string& name, 
        const Json::Value& jobject,
        const format_map_t& format
    );
    int patch_object(
        const std::string& name, 
        const Json::Value& jobject,
        const format_map_t& format
    );

    int patch_object(
        redis_connector_t rg,
        const std::string& name,
        const Json::Value& jobject
    );
    int patch_object(
        const std::string& name,
        const Json::Value& jobject
    );

    int patch_object(
        redis_connector_t rg,
        const std::string& name,
        const std::map< std::string, std::string > kv
    );
    int patch_object(
        const std::string& name,
        const std::map< std::string, std::string > kv
    );

    Json::Value get_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    );
    Json::Value get_object(
        const std::string& name,
        const std::string& id
    );

    Json::Value get_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& unique_key,
        const std::string& value
    );
    Json::Value get_object(
        const std::string& name,
        const std::string& unique_key,
        const std::string& value
    );

    // Delete object
    int delete_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    );
    int delete_object(
        const std::string& name,
        const std::string& id
    );

    // Pin an object at the top of the list
    int pin_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    );
    int pin_object(
        const std::string& name,
        const std::string& id
    );

    int unpin_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    );
    int unpin_object(
        const std::string& name,
        const std::string& id
    );

    // Set the limit of the pin list
    int set_pin_limit(
        redis_connector_t rg,
        const std::string& name,
        int limit
    );
    int set_pin_limit(
        const std::string& name,
        int limit
    );

}};

#endif

// Push Chen
