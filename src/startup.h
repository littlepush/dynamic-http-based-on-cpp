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

#include "dhboc.h"
#include "modules.h"

extern const std::string EXT_NAME;
extern const std::string CC;
extern const std::string INC_ROOT;
extern const std::string EX_FLAGS;
extern const std::string OBJ_EXT;
extern const std::string CC_DEFINES;

/*
 Startup Manager
 Manager the main entry point lib of the web
 Will maintain the runtime folder
 also provide the method to compile any source code
*/
class startupmgr {
public:
    // Pre Request
    typedef http::CODE(*pre_request_t)( http_request& );
    // Final response
    typedef void(*final_response_t)( http_request&, http_response& );
    // Initial worker
    typedef void(*worker_init_t)(int);

protected: 
    module_t                hstartup_;
    pre_request_t           hrequest_;
    final_response_t        hresponse_;

    std::string             startup_file_;
    std::string             webroot_path_;
    std::string             runtime_path_;
    std::string             piece_path_;
    std::string             src_path_;
    std::string             obj_path_;
    std::string             server_path_;
    std::string             lib_path_;

protected:

    // Singleton
    startupmgr( );
    static startupmgr& _s_();

public: 
    ~startupmgr();

    // All runtime paths
    static const std::string& webroot_dir();
    static const std::string& runtime_dir();
    static const std::string& piece_dir();
    static const std::string& source_dir();
    static const std::string& object_dir();
    static const std::string& server_dir();
    static const std::string& lib_path();

    // Load the startup file and initialize everything
    static bool load_startup_file( const std::string& sf, bool force_rebuild = false );

    // Initialize the worker process
    static bool worker_initialization( int index );

    // Pre request handler
    static http::CODE pre_request( http_request& req );

    // Final Response handler
    static void final_response( http_request& req, http_response& resp );

    // Search for handler with specified name
    static http_handler search_handler( const std::string& hname );

    // Compile source code, specified the input source file and the output obj path
    static bool compile_source(
        const std::string& src, 
        const std::string& obj, 
        const char* ex = NULL
    );

    // Link and create a shared library
    static bool create_library(
        const std::vector<std::string>& objs, 
        const std::string& libpath, 
        const char* ex = NULL
    );
};

#endif 

// Push Chen
