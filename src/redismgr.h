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
    typedef std::function< void ( const std::string& ) >    schedule_t;
    using pe::co::net::proto::redis::result_t;

    // Redis Manager
    class manager {
        redis_connector_t                           rgroup_;
        task *                                      notify_sub_;
        std::map< std::string, notification_t >     notification_map_;
        task *                                      expire_sub_;
        std::map< std::string, expire_t >           expire_map_;
        notification_t                              all_exp_;
        task *                                      schedule_task_;
        schedule_t                                  schedule_callback_;
    protected: 
        // Singleton
        static manager& ins();

        // C'str
        manager();
        void begin_subscribe_();
    public:
        ~manager();

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

        // For any expire, first to invoke this callback
        static void on_key_expire( notification_t cb );

        // Register an schedule task
        static void register_schedule_task( time_t on, const std::string& task_info );

        // Cancel schedule task
        static void cancel_schedule_task( const std::string& task_info );

        // Check schedule task
        static void wait_schedule_task( schedule_t callback );

        template < typename... cmd_t >
        static inline result_t query( const cmd_t&... c ) {
            return shared_group()->query(c...);
        }
    };
}}

#endif

// Push Chen
