/*
    session.cpp
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-02-11
    Push Chen

    Copyright 2015-2020 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include "session.h"

// Utils
namespace dhboc { 
    // session_relation_t              relation_map_;
    // session_map_t                   sessions_;

    // Singleton C'Str
    session::session() : local_cache_(true) {
        redis::manager::register_notification(
            "__dhboc_session__", [this](const std::string& value) -> bool {
                if ( this->local_cache_ == false ) return true;
                auto _np = utils::split(value, ",");
                std::string _spid = std::to_string(getpid());
                // We do not need to process a notification from self
                if ( _spid == _np[0] ) return true;

                bool _validated = false;
                do {
                    // Invalidate notification
                    if ( _np.size() < 3 ) break;
                    if ( _np[1] == "+" ) {
                        if ( _np.size() != 5 ) break;
                        _validated = true;
                        auto _sit = sessions_.find(_np[2]);
                        // Not loaded, ignore
                        if ( _sit == sessions_.end() ) break;
                        (*_sit->second)[_np[3]] = _np[4];
                    } else if ( _np[1] == "-" ) {
                        if ( _np.size() != 4 ) break;
                        _validated = true;
                        auto _sit = sessions_.find(_np[2]);
                        // Not loaded, ignore
                        if ( _sit == sessions_.end() ) break;
                        (*_sit->second).erase(_np[3]);
                    } else if ( _np[1] == "/" ) {
                        // Erase all
                        _validated = true;
                        sessions_.erase(_np[2]);
                    }
                } while ( false );

                if ( !_validated ) {
                    rlog::error 
                        << "invalidate notification for __dhboc_session__: " 
                        << value << std::endl;
                }
                // Wait for next notification
                return true;
            }
        );
    }

    // Singleton Instance
    session& session::shared() {
        static session __gshared;
        return __gshared;
    }

    // Enable/Disable Local Cache of the session
    void session::enable_local_cache( bool enabled ) {
        session::shared().local_cache_ = enabled;
        if ( enabled == true ) return;
        // Remove all cached session
        session::shared().sessions_.clear();
     }

    // Create a new session or load from redis manager with specified id
    // If the session id is already existed, will force to reload all values
    // from redis server
    void session::load_session( const std::string& sid ) {
        if ( session::shared().local_cache_ == false ) return;
        auto _r = redis::manager::shared_group()->query("HGETALL", "__dhboc_session__." + sid);
        shared_session_t _ss = std::make_shared< session_cache_t >();
        for ( size_t i = 0; i < _r.size(); i += 2 ) {
            (*_ss)[_r[i].content] = _r[i + 1].content;
        }
        shared().sessions_[sid] = _ss;
    }
    // Clear a session, remove all keys
    void session::clear_session( const std::string& sid ) {
        if ( session::shared().local_cache_ ) {
            shared().sessions_.erase(sid);
        }
        // Delete the the session hash
        redis::manager::query("DEL", "__dhboc_session__." + sid);
        redis::manager::send_notification("__dhboc_session__", 
            std::to_string(getpid()) + ",/," + sid
        );
    }

    // Bind current task to specified session
    // If the session does not existed, will invoke load_session to create it
    void session::bind_session( const std::string& sid ) {
        if ( session::shared().local_cache_ == true ) {
            auto _sit = shared().sessions_.find(sid);
            if ( _sit == shared().sessions_.end() ) {
                load_session(sid);
            }
        }
        shared().relation_map_[this_task::get_id()] = sid;
    }
    // Unbind the session
    void session::unbind_session( ) {
        shared().relation_map_.erase(this_task::get_id());
    }

    // Get current bound session id
    std::string session::get_session_id( ) {
        auto _rit = session::shared().relation_map_.find(this_task::get_id());
        if ( _rit == session::shared().relation_map_.end() ) return std::string("");
        return _rit->second;
    }

    // Query specified session to find the key
    std::string session::get( const std::string& sid, const std::string& key ) {
        if ( sid.size() == 0 ) return std::string("");
        while ( session::shared().local_cache_ ) {
            auto _sit = session::shared().sessions_.find(sid);
            if ( _sit == session::shared().sessions_.end() ) break;
            auto _kit = _sit->second->find(key);
            if ( _kit == _sit->second->end() ) break;
            return _kit->second;
        }
        auto _r = redis::manager::query("HMGET", "__dhboc_session__." + sid, key);
        if ( _r.size() == 0 ) return std::string("");
        return _r[0].content;
    }
    // Query current task's binding session to find the key
    std::string session::get(const std::string& key) {
        auto _rit = session::shared().relation_map_.find(this_task::get_id());
        std::string _sid;
        if ( _rit != session::shared().relation_map_.end() ) _sid = _rit->second;
        return session::get(_sid, key);
    }

    // Set key and value and tell the redis manager to send update notification
    void session::set( const std::string& sid, const std::string& key, const std::string& value ) {
        if ( sid.size() == 0 ) return;

        if ( session::shared().local_cache_ ) {
            auto _sit = session::shared().sessions_.find(sid);
            if ( _sit == session::shared().sessions_.end() ) {
                session::load_session(sid);
                _sit = session::shared().sessions_.find(sid);
            }
            (*_sit->second)[key] = value;
        }
        redis::manager::query("HMSET", "__dhboc_session__." + sid, key, value);
        // Maybe some workers are use local cache, so we still need to send the notification
        redis::manager::send_notification(
            "__dhboc_session__", std::to_string(getpid()) + ",+," + sid + "," + key + "," + value
        );
    }
    // Set key and value to current task's binding session
    void session::set( const std::string& key, const std::string& value ) {
        auto _rit = session::shared().relation_map_.find(this_task::get_id());
        std::string _sid;
        if ( _rit != session::shared().relation_map_.end() ) _sid = _rit->second;
        session::set( _sid, key, value );
    }

    // Remove key
    void session::remove( const std::string& sid, const std::string& key ) {
        if ( sid.size() == 0 ) return;

        while ( session::shared().local_cache_ ) {
            auto _sit = session::shared().sessions_.find(sid);
            if ( _sit == session::shared().sessions_.end() ) break;
            auto _kit = _sit->second->find(key);
            // No such key, do not need to remove
            if ( _kit == _sit->second->end() ) break;
            _sit->second->erase(_kit);
            break;
        }
        redis::manager::query("HDEL", "__dhboc_session__." + sid, key);
        redis::manager::send_notification(
            "__dhboc_session__", std::to_string(getpid()) + ",-," + sid + "," + key
        );
    }
    // Remove key from current task's binding session
    void session::remove( const std::string& key ) {
        auto _rit = session::shared().relation_map_.find(this_task::get_id());
        std::string _sid;
        if ( _rit != session::shared().relation_map_.end() ) _sid = _rit->second;
        session::remove(_sid, key);
    }
};

// Push Chen
