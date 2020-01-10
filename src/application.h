/*
    application.h
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2019-10-31
    Push Chen

    Copyright 2015-2016 MeetU Infomation and Technology Inc. All rights reserved.
*/

#pragma once

#ifndef DHBOC_APPLICATION_H__
#define DHBOC_APPLICATION_H__

#include <peco/peco.h>
using namespace pe;
using namespace pe::co;
using namespace pe::co::net;
using namespace pe::co::net::proto;

typedef void (*http_handler)(const net::http_request&, net::http_response&);
typedef bool(*router_parser)(const std::vector<std::string>&);

struct router_t {
    std::string         method;
    router_parser       parser;
    std::string         handler;
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
};
typedef void(*startup_initialize)( );

// Global Application Object
extern application_t app;

#endif

// Push Chen
