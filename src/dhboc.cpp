/*
    dhboc.cpp
    Dynamic-Http-Based-On-Cpp
    2020-01-21
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)

    Copyright 2015-2019 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include "dhboc.h"
#include "handler.h"

// Global App Object
application_t app;

// Check if the request method is allowed
bool req_method_allow( const http_request& req, http_response& resp, const std::string& method ) {
    if ( req.method() == method ) return true;
    resp.status_code = CODE_405;
    return_error( resp, req.path() + " only allow " + method );
    return false;
}

// Check if current request contains a json body
bool is_json_req( const http_request& req ) {
    if ( !req.header.contains("Content-Type") ) return false;
    std::string _ct = req.header["Content-Type"];
    auto _ctp = utils::split(_ct, "; ");
    return ( _ctp[0] == "application/json" );
}
// Get form-format req params
url_params::param_t req_params( const http_request& req ) {
    std::string _b(std::forward< std::string >(req.body.raw()));
    return url_params::parse(_b.c_str(), _b.size());
}

// Get json body
Json::Value req_json( const http_request& req ) {
    std::string _b(std::forward< std::string >(req.body.raw()));
    Json::Value _root;
    if ( _b.size() == 0 ) return _root;
    Json::Reader _reader;
    _reader.parse(_b, _root, false);
    return _root;
}

// Return JSON Data
void make_response( http_response& resp, const Json::Value& body ) {
    resp.header["Content-Type"] = "application/json";
    Json::Value _root(Json::objectValue);
    _root["code"] = 0;
    _root["msg"] = "ok";
    _root["data"] = body;
    Json::FastWriter _w;
    resp.write(_w.write(_root));
}

// Write Error Message in API
void return_error( http_response& resp, const std::string& message ) {
    return_error(resp, -1, message);
}

// Write Error Message in API
void return_error( http_response& resp, int code, const std::string& message ) {
    resp.header["Content-Type"] = "application/json";
    Json::Value _root(Json::objectValue);
    _root["code"] = code;
    _root["msg"] = message;
    _root["data"] = Json::Value(Json::objectValue);
    Json::FastWriter _w;
    resp.write(_w.write(_root));
}

// Write Success Message
void return_ok( http_response& resp ) {
    return_error(resp, 0, "ok");
}

// Return String Map
void return_data( http_response& resp, const std::map< std::string, std::string >& data ) {
    Json::Value _data(Json::objectValue);
    for ( const auto& kv : data ) {
        _data[kv.first] = kv.second;
    }
    make_response(resp, _data);
}

// Push Chen
