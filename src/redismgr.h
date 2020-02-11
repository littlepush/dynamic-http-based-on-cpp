/*
    redismgr.h
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-02-04
    Push Chen

    Copyright 2015-2016 MeetU Infomation and Technology Inc. All rights reserved.
*/

#pragma once

#ifndef DHBOC_REDISMGR_H__
#define DHBOC_REDISMGR_H__

#include <peco/peco.h>
using namespace pe;
using namespace pe::co;
using namespace pe::co::net;

namespace dhboc { namespace redis {

    typedef std::shared_ptr< net::redis::group >            redis_connector_t;
    typedef std::function< bool ( const std::string& ) >    notification_t;
    typedef std::function< void ( const std::string& ) >    expire_t;
    using pe::co::net::proto::redis::result_t;

    // Redis Manager
    class manager {
        redis_connector_t                           rgroup_;
        std::map< std::string, task * >             notification_map_;
        bool                                        enabled_expire_;
        task *                                      expire_sub_;
        std::map< std::string, expire_t >           expire_map_;
    protected: 
        // Singleton
        static manager& ins();

        // C'str
        manager();
    public:
        static bool connect_to_redis_server( const std::string& rinfo, size_t count );

        // Get the shared group
        static redis_connector_t shared_group();

        // Register the notification for pub/sub
        static void register_notification(const std::string& key, notification_t cb);

        // Unregister the notification session
        static void unregister_notification(const std::string& key);

        // Send notification key
        static void send_notification( const std::string& key, const std::string& value );

        // Wait for key expire
        static void wait_expire(const std::string& key, expire_t cb);

        template < typename... cmd_t >
        static inline result_t query( const cmd_t&... c ) {
            return shared_group()->query(c...);
        }
    };
}}

#endif

// Push Chen
