/*
    dhboc.cpp
    Dynamic-Http-Based-On-Cpp
    2020-01-21
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)

    Copyright 2015-2019 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include "dhboc.h"
#include "handler.h"
#include "template.h"
#include <iomanip>

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
    std::string _error;
    Json::CharReaderBuilder _builder;
    std::unique_ptr< Json::CharReader > _reader(_builder.newCharReader());
    _reader->parse(_b.c_str(), _b.c_str() + _b.size(), &_root, &_error);
    return _root;
}

// Return JSON Data
void make_response( http_response& resp, const Json::Value& body ) {
    resp.header["Content-Type"] = "application/json";
    Json::Value _root(Json::objectValue);
    _root["code"] = 0;
    _root["msg"] = "ok";
    _root["data"] = body;
    Json::StreamWriterBuilder _builder;
    std::unique_ptr< Json::StreamWriter > _w(_builder.newStreamWriter());
    std::ostringstream _oss;
    _w->write(_root, &_oss);
    resp.write(_oss.str());
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

    Json::StreamWriterBuilder _builder;
    std::unique_ptr< Json::StreamWriter > _w(_builder.newStreamWriter());
    std::ostringstream _oss;
    _w->write(_root, &_oss);
    resp.write(_oss.str());
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

// Use asctime to format the timestamp
std::string dhboc_time_string( time_t t ) {
    if ( t == 0 ) return std::string("N/A");
    // return std::string(ctime(&t));
    struct tm _tm = *std::localtime(&t);
    std::stringstream _tss;
    _tss << 
        std::setfill('0') << std::setw(4) << _tm.tm_year + 1900 << "/" <<
        std::setfill('0') << std::setw(2) << _tm.tm_mon + 1 << "/" <<
        std::setfill('0') << std::setw(2) << _tm.tm_mday << " " <<
        std::setfill('0') << std::setw(2) << _tm.tm_hour << ":" <<
        std::setfill('0') << std::setw(2) << _tm.tm_min << ":" <<
        std::setfill('0') << std::setw(2) << _tm.tm_sec;
    return _tss.str();
    // return std::string( ::asctime(_tm) );
}
// Use asctime to format the timestamp
std::string dhboc_time_string( const std::string& ts ) {
    return dhboc_time_string( (time_t)std::stoi(ts) );
}

// Read Json Object From Data
bool json_cpp_reader( const std::string& data, Json::Value& root ) {
    Json::CharReaderBuilder _rbuilder;
    std::unique_ptr< Json::CharReader > _jr(_rbuilder.newCharReader());
    std::string _error;
    return _jr->parse(data.c_str(), data.c_str() + data.size(), &root, &_error);
}

std::string json_cpp_write( const Json::Value& value ) {
    Json::StreamWriterBuilder _builder;
    std::unique_ptr< Json::StreamWriter > _w(_builder.newStreamWriter());
    std::ostringstream _oss;
    _w->write(value, &_oss);
    return _oss.str();
}
// Invoke template
void apply_template(
    const http_request& req, http_response& resp, 
    const std::string& template_name, const placeholds_t& ph)
{
    content_template::apply_template(req, resp, template_name, ph);
}

// Push Chen
