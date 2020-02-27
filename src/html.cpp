/*
    html.cpp
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-02-24
    Push Chen

    Copyright 2015-2020 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include "html.h"

// HTML
namespace dhboc { namespace html {

    // // Simple Element Item, should not be used directly
    // struct inner_element {
    //     std::string             name;
    //     bool                    auto_close;
    //     std::string             classes;
    //     std::map< std::string, std::string >    attributes;
    //     std::map< std::string, bool >           properties;
    // };

    // Create an element
    element element::create( const std::string& name ) {
        return element(name);
    }

    // Create an element
    element::element( const std::string& name ) {
        raw_element_ = std::make_shared<inner_element>();
        raw_element_->name = name;
        if ( name == "input" || name == "img" || name == "br" || name == "hr" ) {
            raw_element_->auto_close = false;
        } else {
            raw_element_->auto_close = true;
        }
    }
    // Copy and move
    element::element( const element& rhs ) :
        raw_element_(rhs.raw_element_), children_(rhs.children_)
    { }
    element::element( element&& rhs ) : 
        raw_element_(std::move(rhs.raw_element_)), 
        children_(std::move(rhs.children_))
    { }

    element& element::operator = ( const element& rhs ) {
        if ( this == &rhs ) return *this;
        raw_element_ = rhs.raw_element_;
        children_ = rhs.children_;
        return *this;
    }
    element& element::operator = ( element&& rhs ) {
        if ( this == &rhs ) return *this;
        raw_element_ = std::move(rhs.raw_element_);
        children_ = std::move(rhs.children_);
        return *this;
    }

    // Events
    element& element::addClass( const std::string& class_name ) {
        auto _now_classes = utils::split(raw_element_->classes, " ");
        auto _new_classes = utils::split(class_name, " ");
        for ( const auto& c : _new_classes ) {
            auto _cit = std::find(_now_classes.begin(), _now_classes.end(), c);
            if ( _cit != _now_classes.end() ) break;
            _now_classes.push_back(c);
        }
        raw_element_->classes = utils::join(_now_classes, " ");
        return *this;
    }
    element& element::removeClass( const std::string& class_name ) {
        auto _now_classes = utils::split(raw_element_->classes, " ");
        auto _new_classes = utils::split(class_name, " ");
        for ( const auto& c : _new_classes ) {
            auto _cit = std::find(_now_classes.begin(), _now_classes.end(), c);
            if ( _cit != _now_classes.end() ) continue;
            _now_classes.erase(_cit);
        }
        raw_element_->classes = utils::join(_now_classes, " ");
        return *this;
    }
    element& element::prop( const std::string& pkey, bool on_off ) {
        raw_element_->properties[pkey] = on_off;
        return *this;
    }
    element& element::attr( const std::string& akey, const std::string& value ) {
        raw_element_->attributes[akey] = value;
        return *this;
    }
    element& element::text( const std::string& value ) {
        raw_element_->text = value;
        return *this;
    }

    element& element::children( const std::vector< element >& children_elements ) {
        children_ = children_elements;
        return *this;
    }
    element& element::add_child( const element& e ) {
        children_.push_back(e);
        return *this;
    }
    element& element::add_child( element&& e ) {
        children_.emplace_back(std::move(e));
        return *this;
    }
    // Internal
    void element::_to_string(std::ostream& ss) {
        ss << "<" << raw_element_->name;
        if ( raw_element_->classes.size() > 0 ) {
            ss << " class=\"" << raw_element_->classes << "\"";
        }
        for ( auto& akv : raw_element_->attributes ) {
            ss << " " << akv.first << "=\"" << akv.second << "\"";
        }
        for ( auto& pkv : raw_element_->properties ) {
            if ( pkv.second == false ) continue;
            ss << " " << pkv.first;
        }
        ss << ">";
        if ( ! raw_element_->auto_close ) return;
        if ( children_.size() > 0 ) {
            ss << std::endl;
        }
        for ( auto& e : children_ ) {
            e._to_string(ss);
        }
        ss << raw_element_->text;
        ss << "</" << raw_element_->name << ">" << std::endl;
    }

    // Write to the resposne
    element& element::output( http_response& resp ) {
        std::stringstream _ss;
        this->_to_string(_ss);
        resp.write( _ss.str() );
        return *this;
    }
    // Write to the stdout
    element& element::output( ) {
        this->_to_string(std::cout);
        return *this;
    }
}}

// Push Chen
