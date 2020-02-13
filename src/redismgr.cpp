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
    void manager::begin_subscribe_() {
        notify_sub_ = rgroup_->subscribe([this](const std::string& key, const std::string& value) {
            auto _nit = this->notification_map_.find(key);
            if ( _nit == this->notification_map_.end() ) return;
            if ( !_nit->second( value ) ) this->notification_map_.erase(_nit);
        }, "PSUBSCRIBE", "__dhboc__:*:__notify__");

        // Config to enable expire event
        rgroup_->query("config", "set", "notify-keyspace-events", "Ex");
        expire_sub_ = rgroup_->subscribe([this](const std::string& key, const std::string& value) {
            if ( all_exp_ ) {
                if ( ! all_exp_(value) ) return;
            }
            auto _eit = this->expire_map_.find(key);
            if ( _eit == this->expire_map_.end() ) return;
            _eit->second(value);
            this->expire_map_.erase(_eit);
        }, "PSUBSCRIBE", "__key*__:*");
    }

    // C'str
    manager::manager() : 
        rgroup_(nullptr), 
        notify_sub_(NULL),
        expire_sub_(NULL) 
    { }

    manager::~manager() {
        if ( notify_sub_ ) {
            rgroup_->unsubscribe(notify_sub_);
            notify_sub_ = NULL;
        }
        if ( expire_sub_ ) {
            rgroup_->unsubscribe(expire_sub_);
            expire_sub_ = NULL;
        }
    }
    bool manager::connect_to_redis_server( const std::string& rinfo, size_t count ) {
        ins().rgroup_ = std::make_shared< net::redis::group >( rinfo, count );
        bool _result = ins().rgroup_->lowest_load_connector().is_validate();
        if ( !_result ) return _result;

        // Begin Subscribe
        ins().begin_subscribe_();
        return _result;
    }

    // Get the shared group
    redis_connector_t manager::shared_group() {
        return ins().rgroup_;
    }

    // Register the notification for pub/sub
    void manager::register_notification( const std::string& key, notification_t cb ) {
        if ( cb == nullptr ) return;
        ins().notification_map_["__dhboc__:" + key + ":__notify__"] = cb;
    }

    // Unregister the notification session
    void manager::unregister_notification(const std::string& key) {
        std::string _k = "__dhboc__:" + key + ":__notify__";
        auto _it = ins().notification_map_.find(_k);
        if ( _it == ins().notification_map_.end() ) return;
        ins().notification_map_.erase(_it);
    }

    // Wait for key expire
    void manager::wait_expire(const std::string& key, expire_t cb) {
        ins().expire_map_[key] = cb;
    }

    // For any expire, first to invoke this callback
    void manager::on_key_expire( notification_t cb ) {
        ins().all_exp_ = cb;
    }

    // Send notification key
    void manager::send_notification( const std::string& key, const std::string& value ) {
        manager::query("PUBLISH", "__dhboc__:" + key + ":__notify__", value);
    }
}}

// Push Chen
