/*
    startup.h
    PECoTask
    2019-10-31
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)

    Copyright 2015-2018 MeetU Infomation and Technology Inc. All rights reserved.
*/

#pragma once

#ifndef DHBOC_STARTUP_H__
#define DHBOC_STARTUP_H__

#include "application.h"
#include "modules.h"

// manager of the startup module
class startupmgr {
    typedef http::CODE(*pre_request_t)( http_request& );
    typedef void(*final_response_t)( http_request&, http_response& );
    typedef void(*worker_init_t)();

protected: 
    bool                    done_;
    module_t                hstartup_;
    pre_request_t           hrequest_;
    final_response_t        hresponse_;

    std::string             startup_;
    std::string             webroot_;
    std::string             runtime_;
    std::string             serverpath_;
    std::string             libpath_;


public: 
    // Init the startup manager with the input file
    startupmgr( const std::string& startup_file, bool force_rebuild = false );
    ~startupmgr();

    const std::string& webroot;
    const std::string& runtime;
    const std::string& serverpath;
    const std::string& libpath;

    // Check if the startup module has been loaded
    operator bool () const;

    // Initialize the worker process
    bool worker_initialization();

    // Pre request handler
    http::CODE pre_request( http_request& req );

    // Final Response handler
    void final_response( http_request& req, http_response& resp );

    // Compile source code, specified the input source file and the output obj path
    bool compile_source(const std::string& src, const std::string& obj);

    // Link and create a shared library
    bool create_library(const std::vector<std::string>& objs, const std::string& libpath);
};

#endif 

// Push Chen
