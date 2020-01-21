/*
    handler.h
    Dynamic-Http-Based-On-Cpp
    2020-01-21
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)

    Copyright 2015-2020 MeetU Infomation and Technology Inc. All rights reserved.
*/

#pragma once

#ifndef DHBOC_HANDLER_H__
#define DHBOC_HANDLER_H__

#include "dhboc.h"
#include "startup.h"

/*
    Compile the whole web as a single content handler lib
    This class will scan and find all page/css/js/non-binary
    file and format as a cpp source file
*/
class content_handlers {
    // Handler module
    module_t                                mh_;

    // Handler lib path
    std::string                             hlibpath_;

    // All Handler temp objects
    std::vector< std::string >              objs_;

    // Handler names(path/function_name)
    std::map< std::string, std::string >    handler_names_;

    // Handler function points
    std::map< std::string, http_handler >   handlers_;

    // Ignore files
    std::vector< std::string >              ignore_files_;

    // Content update time
    std::map< std::string, time_t >         content_uptime_;

    // Singleton
    content_handlers();
    static content_handlers& _s_();

public: 
    ~content_handlers();

    // Append ignore files
    static void ignore_file( const std::string& ifile );

    // Scan the webroot to find all files
    static bool scan_webroot();

    // Format and compile source code
    static bool format_source_code( const std::string& origin_file );

    // Check if any content has been changed
    static bool content_changed( );

    // Create handler lib
    static bool build_handler_lib();

    // Load the handler in worker
    static void load_handlers();

    // Try to search the handler and run
    static bool try_find_handler(const http_request& req, http_response& resp);
};

#endif 

// Push Chen
