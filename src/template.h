/*
    template.h
    Dynamic-Http-Based-On-Cpp
    2020-03-12
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)

    Copyright 2015-2020 MeetU Infomation and Technology Inc. All rights reserved.
*/

#pragma once

#ifndef DHBOC_TEMPLATE_H__
#define DHBOC_TEMPLATE_H__

#include "dhboc.h"
#include "startup.h"

/*
    Compile the whole web as a single content handler lib
    This class will scan and find all page/css/js/non-binary
    file and format as a cpp source file
*/
class content_template {
protected:
    // All Template temp objects
    std::vector< std::string >              objs_;

    // Template names(path/function_name)
    std::map< std::string, std::string >    template_names_;

    // Template function points
    std::map< std::string, template_handler_t > templates_;

    // Content update time
    std::map< std::string, time_t >         content_uptime_;

    // Compile Flag
    std::string                             compile_flag_;

    static content_template& _s_();
    content_template();
public: 
    ~content_template();

    // Set default compile extra flags
    static void set_compile_flag( const std::string& flags );

    // Scan the webroot to find all files
    static bool scan_templates();

    // Format and compile source code
    static bool format_source_code( const std::string& origin_file );

    // Check if any content has been changed
    static bool content_changed( );

    // Init template handlers, must set a dlopen-ed handler
    static void init_template(module_t m);

    // Get all template's object file path
    static const std::vector< std::string >& get_template_objs();

    // Try to search the handler and run
    static void apply_template(
        const http_request& req, http_response& resp, 
        const std::string& template_name, const placeholds_t& ph);
};

#endif 

// Push Chen
