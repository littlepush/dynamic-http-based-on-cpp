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

// Utils
namespace dhboc { namespace redis {
    // The redis connector group type
    typedef std::shared_ptr< net::redis::group >    redis_connector_t;
    typedef std::map< std::string, std::function< std::string( const std::string&) > >      format_map_t;
    typedef std::map< std::string, std::function< bool( const std::string&) > >             filter_map_t;

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

    // Register an object in the memory
    int register_object( 
        const std::string& name, 
        const std::vector< properity_t >& properties
    );

    // Get the count of specifial object
    int count_object( redis_connector_t rg, const std::string& name );

    // Get the list of value
    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name
    );

    Json::Value list_object( 
        redis_connector_t rg, 
        const std::string& name, 
        int offset, 
        int page_size
    );

    Json::Value list_object( 
        redis_connector_t rg, 
        const std::string& name, 
        int offset, 
        int page_size, 
        const std::vector< std::string >& filter_keys 
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
        redis_connector_t rg,
        const std::string& name,
        const Json::Value& jobject
    );

    Json::Value get_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    );

    Json::Value query_object(
        redis_connector_t rg,
        const std::string& name,
        const filter_map_t& filters
    );

    // Delete object
    int delete_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    );

    // Pin an object at the top of the list
    int pin_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    );

    int unpin_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    );

    // Set the limit of the pin list
    int set_pin_limit(
        redis_connector_t rg,
        const std::string& name,
        int limit
    );
}};

#endif

// Push Chen
