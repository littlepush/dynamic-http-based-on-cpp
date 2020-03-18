/*
    modules.h
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2019-10-31
    Push Chen
*/

/*
MIT License

Copyright (c) 2020 Push Chen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#ifndef DHBOC_MODULES_H__
#define DHBOC_MODULES_H__

#include "dhboc.h"

typedef void *                  module_t;

// Module Info function
typedef std::string(*module_info_f)( void );

// Module Config function
typedef std::vector<std::string>(*module_config_f)( void );

struct dhboc_module {
    module_t                    module;
    std::string                 module_name;
    std::vector<std::string>    header_files;
    std::vector<std::string>    compile_flags;
    std::vector<std::string>    link_flags;
};

// Get module info
std::string module_get_info( module_t mh, const char* method );

// Get module config
std::vector< std::string > module_get_config( module_t mh, const char* method );

// Global module manager
class modulemgr {
    // Save all loaded modules
    std::map< std::string, dhboc_module >   module_map_;
    std::vector< std::string >              include_files_;
    std::string                             includes_;
    std::string                             compile_flags_;
    std::string                             link_flags_;

protected: 
    // Singleton
    static modulemgr& mgr();
    // Internal c'str
    modulemgr();
public: 
    ~modulemgr();
    // Load a module and add to the cache
    static bool load_module( const std::string& module_name );

    // Get all include files vector
    static const std::vector< std::string >& include_files();

    // Get all include file list as a formated string
    static std::string include_string();

    // Get all compile flags as a string
    static std::string compile_flags();

    // Get all link flags as a string
    static std::string link_flags();
};

#endif 

// Push Chen
