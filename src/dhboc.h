/*
    dhboc.h
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2019-10-31
    Push Chen

    Copyright 2015-2016 MeetU Infomation and Technology Inc. All rights reserved.
*/

#pragma once

#ifndef DHBOC_DHBOC_H__
#define DHBOC_DHBOC_H__

#include <peco/peco.h>
using namespace pe;
using namespace pe::co;
using namespace pe::co::net;
using namespace pe::co::net::proto;

#include <regex>

typedef void (*http_handler)(const net::http_request&, net::http_response&);

struct router_t {
    std::string                 method;
    std::string                 match_rule;
    std::string                 handler;
};

struct application_t {
    std::string                                 domain;
    std::string                                 address;
    int                                         workers;
    std::vector< router_t >                     router;
    std::map< int, std::string >                code;
    std::string                                 webroot;
    std::string                                 runtime;
    std::vector< std::string >                  exclude_path;
    std::vector< std::string >                  pre_includes;
    std::vector< std::string >                  ctnt_exts;
};

// Entry Point Function
extern "C" {

    // Initialize the web host, set anything we 
    // need in `app`
    void dhboc_initialize();

    // Start the worker with the index.
    void dhboc_worker_startup( int index );

    // Do pre-process before the worker get the request
    // Usually we check auth in this function.
    http::CODE dhboc_pre_request( http_request& req );

    // Final process before we send the response to the client
    void dhboc_final_response( http_request& req, http_response& resp );
}

// Global Application Object
extern application_t app;

// Check if the request method is allowed
bool req_method_allow( const http_request& req, http_response& resp, const std::string& method );

// Check if current request contains a json body
bool is_json_req( const http_request& req );

// Get form-format req params
url_params::param_t req_params( const http_request& req );

// Get json body
Json::Value req_json( const http_request& req );

// Return JSON Data
void make_response( http_response& resp, const Json::Value& body );

// Write Error Message in API
void return_error( http_response& resp, const std::string& message );

// Write Error Message in API
void return_error( http_response& resp, int code, const std::string& message );

// Write Success Message
void return_ok( http_response& resp );

// Return String Map
void return_data( http_response& resp, const std::map< std::string, std::string >& data );

// Return Array
template < typename T >
void return_array( http_response& resp, const std::vector<T>& array ) {
    Json::Value _node(Json::arrayValue);
    for ( const auto& a : array ) {
        _node.append(a);
    }
    make_response( resp, _node );
}

template < typename p_t >
Json::Value& __check_json_contains( Json::Value& j, bool& r, const p_t& k ) {
    r &= ( j.isMember(k) );
    return j;
}
template < typename p_t, typename... other_p_t >
Json::Value& __check_json_contains( Json::Value& j, bool& r, const p_t& k, const other_p_t&... ok ) {
    return __check_json_contains(__check_json_contains(j, r, k), r, ok...);
}
template < typename... p_t >
bool check_json_contains( Json::Value& j, const p_t&... k ) {
    bool _r = true;
    __check_json_contains(j, _r, k...);
    return _r;
}

// Use asctime to format the timestamp
std::string dhboc_time_string( time_t t );
// Use asctime to format the timestamp
std::string dhboc_time_string( const std::string& ts );

#include "redismgr.h"
#include "redisobj.h"
#include "session.h"
#include "html.h"

#endif

// Push Chen
