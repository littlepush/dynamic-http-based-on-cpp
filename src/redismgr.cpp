/*
    redismgr.cpp
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-02-04
    Push Chen

    Copyright 2015-2016 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include "redismgr.h"

namespace dhboc { namespace redis {

    // Singleton
    manager& manager::ins() {
        static manager _gm;
        return _gm;
    }

    // C'str
    manager::manager() : rgroup_(nullptr) { }
    bool manager::connect_to_redis_server( const std::string& rinfo, size_t count ) {
        ins().rgroup_ = std::make_shared< net::redis::group >( rinfo, count );
        return ins().rgroup_->lowest_load_connector().is_validate();
    }

    // Get the shared group
    redis_connector_t manager::shared_group() {
        return ins().rgroup_;
    }

    // Register the notification for pub/sub
    void manager::register_notification( const std::string& key, notification_t cb ) {
        if ( cb == nullptr ) return;
        task * _stask = ins().rgroup_->subscribe(
            [cb](const std::string& k, const std::string& v) {
                bool _goon = cb(v);
                if ( _goon ) return;
                manager::unregister_notification(k);
            }, "SUBSCRIBE", key
        );
        if ( _stask != NULL ) {
            ins().notification_map_[key] = _stask;
        }
    }

    // Unregister the notification session
    void manager::unregister_notification(const std::string& key) {
        auto _it = ins().notification_map_.find(key);
        if ( _it == ins().notification_map_.end() ) return;
        ins().rgroup_->unsubscribe(_it->second);
        ins().notification_map_.erase(_it);
    }

    // Send notification key
    void manager::send_notification( const std::string& key, const std::string& value ) {
        manager::query("PUBLISH", key, value);
    }
}}

// Push Chen
