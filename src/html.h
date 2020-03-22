/*
    html.h
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-02-24
    Push Chen

    Copyright 2015-2020 MeetU Infomation and Technology Inc. All rights reserved.
*/

#pragma once

#ifndef DHBOC_HTML_H__
#define DHBOC_HTML_H__

#include <peco/peco.h>
using namespace pe;
using namespace pe::co;
using namespace pe::co::net;


// HTML
namespace dhboc { namespace html {

    // Simple Element Item, should not be used directly
    struct inner_element {
        std::string             name;
        bool                    auto_close;
        std::string             classes;
        std::string             text;
        std::map< std::string, std::string >    attributes;
        std::map< std::string, bool >           properties;
    };

    class element {

    private: 
        std::shared_ptr< inner_element >        raw_element_;
        std::vector< element >                  children_;

        // Internal
        void _to_string(std::ostream& ss);
    public:

        // Create an element
        static element create( const std::string& name );

        // Create an element
        element( const std::string& name );
        // Copy and move
        element( const element& rhs );
        element( element&& rhs );

        element& operator = ( const element& rhs );
        element& operator = ( element&& rhs );

        // Events
        element& addClass( const std::string& class_name );
        element& removeClass( const std::string& class_name );
        element& prop( const std::string& pkey, bool on_off );
        element& attr( const std::string& akey, const std::string& value );
        element& text( const std::string& value );

        element& id(const std::string& id_value);
        element& name(const std::string& name_value);
        element& data(const std::string& key, const std::string& value);

        element& children( const std::vector< element >& children_elements );
        element& add_child( const element& e );
        element& add_child( element&& e );

        // Write to the resposne
        element& output( http_response& resp );
        // Write to the stdout
        element& output( );
    };
}}

#endif

// Push Chen
