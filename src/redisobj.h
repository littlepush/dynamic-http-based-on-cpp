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
    extern const std::vector< std::string >     empty_filter_tags;
    extern const std::string                    empty_default_value;

    enum rtype {
        R_STRING,
        R_NUMBER,
        R_BOOLEAN,
        R_NULL
    };

    struct properity_t {
        std::string                                         key;
        rtype                                               type;
        bool                                                unique;
        std::string                                         dvalue;
        // Create an ordered set only when type is R_NUMBER and set ordered to true
        bool                                                ordered;
    };

    properity_t generate_property(
        const std::string& key, 
        rtype type,
        bool unique = false,
        const std::string& dvalue = empty_default_value,
        bool ordered = false
    );

    /*
        tag filter format:
        ^<tag> : put this tag in front of the list
        $<tag> : put this tag in end of the list
        +<tag> / <tag> : only match the tag
        -<tag> : only not contains the tag
        tag order format: 
        <<tag> : order from small to large
        ><tag> : order from large to small
    */
    struct list_arg_t {
        int offset;
        int page_size;
        std::vector< std::string > keys;
        std::vector< std::string > tags;
        std::string orderby;
        filter_map_t filters;
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

    int index_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& orderby = empty_default_value,
        const std::vector< std::string > tags = empty_filter_tags
    );
    int index_object(
        const std::string& name,
        const std::string& id,
        const std::string& orderby = empty_default_value,
        const std::vector< std::string > tags = empty_filter_tags
    );

    // Get the list of value
    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name
    );
    Json::Value list_object(
        const std::string& name
    );

    Json::Value list_object(
        redis_connector_t rg, 
        const std::string& name,
        const std::vector< std::string > tags
    );
    Json::Value list_object(
        const std::string& name,
        const std::vector< std::string > tags
    );
    
    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name,
        const list_arg_t& arg
    );
    Json::Value list_object(
        const std::string& name,
        const list_arg_t& arg
    );

    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name,
        int offset,
        int page_size,
        const std::string& orderby
    );
    Json::Value list_object(
        const std::string& name,
        int offset,
        int page_size,
        const std::string& orderby
    );
    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name,
        int offset,
        int page_size,
        const std::string& orderby,
        const std::vector< std::string >& tags
    );
    Json::Value list_object(
        const std::string& name,
        int offset,
        int page_size,
        const std::string& orderby,
        const std::vector< std::string >& tags
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
    std::string patch_object( 
        redis_connector_t rg,
        const std::string& name, 
        const Json::Value& jobject,
        const format_map_t& format
    );
    std::string patch_object(
        const std::string& name, 
        const Json::Value& jobject,
        const format_map_t& format
    );

    std::string patch_object(
        redis_connector_t rg,
        const std::string& name,
        const Json::Value& jobject
    );
    std::string patch_object(
        const std::string& name,
        const Json::Value& jobject
    );

    std::string patch_object(
        redis_connector_t rg,
        const std::string& name,
        const std::map< std::string, std::string > kv
    );
    std::string patch_object(
        const std::string& name,
        const std::map< std::string, std::string > kv
    );

    int inc_order_value(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value = 1
    );
    int inc_order_value(
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value = 1
    );
    int dcr_order_value(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value = 1
    );
    int dcr_order_value(
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value = 1
    );
    int set_order_value(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value
    );
    int set_order_value(
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value
    );

    int tag_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::vector< std::string >& tags
    );
    int tag_object(
        const std::string& name,
        const std::string& id,
        const std::vector< std::string >& tags
    );
    int tag_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& tag
    );
    int tag_object(
        const std::string& name,
        const std::string& id,
        const std::string& tag
    );
    int untag_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::vector< std::string >& tags
    );
    int untag_object(
        const std::string& name,
        const std::string& id,
        const std::vector< std::string >& tags
    );
    int untag_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& tag
    );
    int untag_object(
        const std::string& name,
        const std::string& id,
        const std::string& tag
    );

    std::vector< std::string > get_tags(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    );
    std::vector< std::string > get_tags(
        const std::string& name,
        const std::string& id
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

    int taglist_count(
        redis_connector_t rg,
        const std::string& name,
        const std::string& tag
    );
    int taglist_count(
        const std::string& name,
        const std::string& tag
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
}};

#endif

// Push Chen
