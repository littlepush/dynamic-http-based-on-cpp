/*
    session.h
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-02-11
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

#ifndef DHBOC_SESSION_H__
#define DHBOC_SESSION_H__

#include <peco/peco.h>
using namespace pe;
using namespace pe::co;
using namespace pe::co::net;

#include "redismgr.h"

// Utils
namespace dhboc { 

    // Session Manager according to current request task
    // Global Session Cache, any set event will send a redis notification
    class session {

    public:
        typedef std::map< std::string, std::string >            session_cache_t;
        typedef std::shared_ptr< session_cache_t >              shared_session_t;
        typedef std::map< std::string, shared_session_t >       session_map_t;
        typedef std::map< task_id, std::string >                session_relation_t;

    protected:

        bool                            local_cache_;
        session_relation_t              relation_map_;
        session_map_t                   sessions_;

    protected:
        // Singleton C'Str
        session();

        // Singleton Instance
        static session& shared();
    public:

        // Enable/Disable Local Cache of the session
        static void enable_local_cache( bool enabled = true );

        // Create a new session or load from redis manager with specified id
        // If the session id is already existed, will force to reload all values
        // from redis server
        static void load_session( const std::string& sid );

        // Clear a session, remove all keys
        static void clear_session( const std::string& sid );

        // Bind current task to specified session
        // If the session does not existed, will invoke load_session to create it
        static void bind_session( const std::string& sid );

        // Unbind the session
        static void unbind_session( );

        // Get current bound session id
        static std::string get_session_id( );

        // Query specified session to find the key
        static std::string get( const std::string& sid, const std::string& key );
        // Query current task's binding session to find the key
        static std::string get(const std::string& key);

        // Set key and value and tell the redis manager to send update notification
        static void set( const std::string& sid, const std::string& key, const std::string& value );
        // Set key and value to current task's binding session
        static void set( const std::string& key, const std::string& value );

        // Remove key
        static void remove( const std::string& sid, const std::string& key );
        // Remove key from current task's binding session
        static void remove( const std::string& key );
    };
};

#endif

// Push Chen
