/*
    redisobj.cpp
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-01-29
    Push Chen

    Copyright 2015-2016 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include "redisobj.h"

// Utils
namespace dhboc { namespace redis {
    // struct properity_t {
    //     std::string             key;
    //     rtype                   type;
    //     bool                    unique;
    //     std::string             dvalue;
    // };

    std::map< std::string, std::map< std::string, properity_t > > g_object_infos;

    Json::Value __get_item(
        redis_connector_t rg,
        const std::string& id,
        const std::vector< properity_t >& filter_keys
    ) {
        net::proto::redis::command _cmd;
        _cmd << "HMGET" << "dhboc.__item__." + id;
        for ( const auto& k : filter_keys ) {
            _cmd << k.key;
        }
        _cmd << "__ctime__" << "__utime__";

        auto _r = rg->query( std::move(_cmd) );
        Json::Value _result(Json::objectValue);
        for ( size_t i = 0; i < filter_keys.size(); ++i ) {
            rtype _t = filter_keys[i].type;
            std::string _k = filter_keys[i].key;
            if ( _t == R_NULL ) {
                if ( _k == "__dummy__" ) continue;
                _result[_k] = Json::Value(Json::nullValue);
                continue;
            }
            if ( _r[i].is_nil() || _r[i].is_empty() ) {
                if ( _t == R_STRING ) {
                    _result[_k] = filter_keys[i].dvalue;
                    continue;
                }
                if ( _t == R_NUMBER && filter_keys[i].dvalue.size() > 0 ) {
                    _result[_k] = std::stof(filter_keys[i].dvalue);
                    continue;
                }
                _result[_k] = Json::Value(Json::nullValue);
                continue;
            }
            if ( _t == R_STRING ) {
                _result[_k] = _r[i].content;
            } else if ( _t == R_NUMBER ) {
                _result[_k] = std::stof(_r[i].content);
            }
        }
        size_t _ts = filter_keys.size();
        _result["create_time"] = _r[_ts].content;
        _result["update_time"] = _r[_ts + 1].content;

        return _result;
    }

    // Get the pin count of a list
    int __pin_count(
        redis_connector_t rg,
        const std::string& name
    ) {
        auto _r_pin_count = rg->query("LLEN", "dhboc." + name + ".pin_ids");
        if ( _r_pin_count.size() == 0 || _r_pin_count[0].is_nil() || _r_pin_count[0].is_empty() ) {
            return 0;
        }
        return std::stoi(_r_pin_count[0].content);
    }

    int __pin_limit(
        redis_connector_t rg,
        const std::string& name
    ) {
        auto _r_pin_limit = rg->query("GET", "dhboc." + name + ".pin_limit");
        if ( _r_pin_limit.size() == 0 || _r_pin_limit[0].is_nil() || _r_pin_limit[0].is_empty() ) {
            return 3;
        }
        return std::stoi(_r_pin_limit[0].content);
    }

    std::vector< properity_t > __get_keys(const std::string& name) {
        std::vector< properity_t > _keys;
        auto _oit = g_object_infos.find(name);
        if ( _oit == g_object_infos.end() ) {
            return _keys;
        }
        // Add Id key
        _keys.emplace_back((properity_t){
            "id", R_STRING, true, std::string("")
        });
        for ( auto& p : _oit->second ) {
            _keys.push_back(p.second);
        }
        return _keys;
    }

    // Register an object in the memory
    int register_object( 
        const std::string& name, 
        const std::vector< properity_t >& properties
    ) {
        if ( g_object_infos.find(name) != g_object_infos.end() ) return 1;
        std::map< std::string, properity_t > _key_map;
        for ( const auto& p : properties ) {
            _key_map[p.key] = p;
        }
        g_object_infos[name] = _key_map;
        return 0;
    }

    // Get the count of specifial object
    int count_object( redis_connector_t rg, const std::string& name ) {
        auto _r = rg->query("LLEN", "dhboc." + name + ".pin_ids");
        if ( _r.size() == 0 ) return 0;
        if ( _r[0].is_nil() || _r[0].is_empty() ) return 0;
        return std::stoi(_r[0].content);
    }

    Json::Value __list_object(
        redis_connector_t rg,
        const std::string& name,
        int offset,
        int page_size,
        const std::vector< properity_t >& keys
    ) {
        int _pin_count = __pin_count(rg, name);
        Json::Value _r_list(Json::arrayValue);
        std::map< std::string, bool > _pin_cache;
        if ( _pin_count > 0 ) {
            auto _pr = rg->query("LRANGE", "dhboc." + name + ".pin_ids", 0, -1);
            for ( auto& id: _pr ) {
                _pin_cache[id.content] = true;
                _r_list.append(__get_item(rg, id.content, keys));
            }
        }

        auto _r = rg->query("LRANGE", "dhboc." + name + ".ids", 
            offset, (offset + page_size + _pin_count) - 1);
        int _lsize = _pin_count;
        int _used = 0;
        for ( auto& id : _r ) {
            ++_used;
            if ( _pin_cache.find(id.content) != _pin_cache.end() ) continue;
            _r_list.append(__get_item(rg, id.content, keys));
            ++_lsize;
            if ( _lsize == page_size ) break;
        }

        Json::Value _result(Json::objectValue);
        _result["all"] = count_object(rg, name);
        _result["offset"] = offset + _used;
        _result["limit"] = page_size;
        _result["data"] = _r_list;
        return _result;
    }

    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name
    ) {
        auto _keys = __get_keys(name);
        if ( _keys.size() == 0 ) {
            Json::Value _empty(Json::nullValue);
            return _empty;
        }
        Json::Value _rlist(Json::arrayValue);
        auto _r = rg->query("LRANGE", "dhboc." + name + ".ids", 0, -1);
        for ( auto& id : _r ) {
            _rlist.append(__get_item(rg, id.content, _keys));
        }
        Json::Value _result(Json::objectValue);
        _result["all"] = count_object(rg, name);
        _result["offset"] = 0;
        _result["limit"] = -1;
        _result["data"] = _rlist;
        return _result;
    }

    // Get the list of value
    Json::Value list_object( 
        redis_connector_t rg, 
        const std::string& name, 
        int offset, 
        int page_size
    ) {
        auto _keys = __get_keys(name);
        if ( _keys.size() == 0 ) {
            Json::Value _empty(Json::nullValue);
            return _empty;
        }
        return __list_object(rg, name, offset, page_size, _keys);
    }

    Json::Value list_object( 
        redis_connector_t rg, 
        const std::string& name, 
        int offset, 
        int page_size, 
        const std::vector< std::string >& filter_keys 
    ) {
        auto _oit = g_object_infos.find(name);
        if ( _oit == g_object_infos.end() ) {
            Json::Value _empty(Json::nullValue);
            return _empty;            
        }

        std::vector< properity_t > _keys;
        // Add Id key
        _keys.emplace_back((properity_t){
            "id", R_STRING, true, std::string("")
        });
        for ( const auto& k : filter_keys ) {
            auto _pit = _oit->second.find(k);
            if ( _pit == _oit->second.end() ) {
                // Must be NULL
                _keys.emplace_back((properity_t){
                    std::string("__dummy__"), R_NULL, false, std::string("")
                });
            } else {
                _keys.push_back(_pit->second);
            }
        }
        return __list_object(rg, name, offset, page_size, _keys);
    }

    void __add_object(
        redis_connector_t rg,
        const std::string& name,
        std::map< std::string, std::string >& kvs
    ) {
        kvs["id"] = utils::md5(
            std::to_string(getpid()) + 
            std::to_string(task_time_now().time_since_epoch().count())
        );
        kvs["__ctime__"] = std::to_string(time(NULL));
        kvs["__utime__"] = std::to_string(time(NULL));
        kvs["__type__"] = name;

        net::proto::redis::command _cmd;
        _cmd << "HMSET" << "dhboc.__item__." + kvs["id"];
        for ( const auto& kv : kvs ) {
            _cmd << kv.first << kv.second;
        }
        ignore_result(rg->query(std::move(_cmd)));
        ignore_result(rg->query("LPUSH", "dhboc." + name + ".ids", kvs["id"]));
    }

    void __update_object(
        redis_connector_t rg,
        const std::string& name,
        std::map< std::string, std::string >& kvs
    ) {
        kvs["__utime__"] = std::to_string(time(NULL));
        net::proto::redis::command _cmd;
        _cmd << "HMSET" << "dhboc.__item__." + kvs["id"];
        for ( const auto& kv : kvs ) {
            _cmd << kv.first << kv.second;
        }
        ignore_result(rg->query(std::move(_cmd)));
    }

    // Recurse add a json object
    // if the object does not existed, add it, and return 0
    // if the object existed, update it and return -1
    // on error, return > 1
    int patch_object( 
        redis_connector_t rg,
        const std::string& name, 
        const Json::Value& jobject,
        const format_map_t& format
    ) {
        auto _keys = __get_keys(name);
        std::map< std::string, std::string > _itemkv;

        if ( jobject.isMember("id") ) {
            // this is an patch op
            for ( auto& k : _keys ) {
                if ( jobject.isMember(k.key) ) {
                    _itemkv[k.key] = jobject[k.key].asString();
                }
                auto _fit = format.find(k.key);
                if ( _fit != format.end() ) {
                    _itemkv[k.key] = _fit->second(_itemkv[k.key]);
                }
            }
            _itemkv["id"] = jobject["id"].asString();
            __update_object(rg, name, _itemkv);
        } else {
            // this is an add op
            for ( auto& k : _keys ) {
                if ( jobject.isMember(k.key) ) {
                    _itemkv[k.key] = jobject[k.key].asString();
                } else {
                    _itemkv[k.key] = k.dvalue;
                }
                auto _fit = format.find(k.key);
                if ( _fit != format.end() ) {
                    _itemkv[k.key] = _fit->second(_itemkv[k.key]);
                }
            }
            __add_object(rg, name, _itemkv);
        }
        return 0;
    }

    int patch_object(
        redis_connector_t rg,
        const std::string& name,
        const Json::Value& jobject
    ) {
        return patch_object(rg, name, jobject, {});
    }

    Json::Value get_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    ) {
        Json::Value _empty(Json::nullValue);
        auto _keys = __get_keys(name);
        if ( _keys.size() == 0 ) {
            return _empty;
        }
        _keys.emplace_back((properity_t){
            "__type__", R_STRING, true, std::string("")
        });
        Json::Value _obj = __get_item(rg, id, _keys);
        if ( !_obj.isMember("__type__") ) {
            return _empty;            
        }
        if ( _obj["__type__"].asString() != name ) {
            return _empty;
        }
        _obj.removeMember("__type__");
        return _obj;
    }

    Json::Value query_object(
        redis_connector_t rg,
        const std::string& name,
        const filter_map_t& filters
    ) {
        Json::Value _result(Json::arrayValue);
        auto _keys = __get_keys(name);
        auto _rids = rg->query("LRANGE", "dhboc." + name + ".ids", 0, -1);
        for ( auto& id : _rids ) {
            Json::Value _item = __get_item(rg, id.content, _keys);
            bool _all_match = true;
            for ( auto& fkv : filters ) {
                if ( !_item.isMember(fkv.first) ) {
                    _all_match = false; break;
                }
                _all_match = fkv.second(_item[fkv.first].asString());
                if ( !_all_match ) break;
            }
            if ( _all_match ) {
                _result.append(_item);
            }
        }
        return _result;
    }

    // Delete object
    int delete_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    ) {
        auto _r = rg->query("HMGET", "dhboc.__item__." + id, "__type__");
        if ( _r.size() == 0 || _r[0].is_nil() ) return 1;
        if ( _r[0].content != name ) return 2;

        ignore_result(rg->query("LREM", "dhboc." + name + ".pin_ids", 1, id));
        ignore_result(rg->query("LREM", "dhboc." + name + ".ids", 1, id));
        ignore_result(rg->query("DEL", "dhboc.__item__." + id));

        return 0;
    }

    // Pin an object at the top of the list
    int pin_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    ) {
        auto _r = rg->query("HMGET", "dhboc.__item__." + id, "__type__");
        if ( _r.size() == 0 || _r[0].is_nil() || _r[0].is_empty() ) {
            // Not the same type as name
            return 1;
        }
        if ( name != _r[0].content ) {
            return 1;
        }

        ignore_result(rg->query("LREM", "dhboc." + name + ".pin_ids", 0, id));
        ignore_result(rg->query("LPUSH", "dhboc." + name + ".pin_ids", id));
        int _pcount = __pin_count(rg, name);
        int _plimit = __pin_limit(rg, name);
        if ( _pcount > _plimit ) {
            ignore_result(rg->query("LTRIM", "dhboc." + name + ".pin_ids", 0, _plimit - 1));
        }
        return 0;
    }

    int unpin_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    ) {
        ignore_result(rg->query("LREM", "dhboc." + name + ".pin_ids", 1, id));
        return 0;
    }

    // Set the limit of the pin list
    int set_pin_limit(
        redis_connector_t rg,
        const std::string& name,
        int limit
    ) {
        if ( limit < 0 ) limit = 0;
        if ( limit > 20 ) limit = 20;
        ignore_result(rg->query("SET", "dhboc." + name + ".pin_limit", limit));
        return 0;
    }
}};

// Push Chen
