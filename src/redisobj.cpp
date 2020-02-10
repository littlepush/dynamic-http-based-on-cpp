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

    void __add_object(
        redis_connector_t rg,
        const std::string& name,
        std::map< std::string, std::string >& kvs
    ) {
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
    int count_object( const std::string& name ) {
        return count_object( manager::shared_group(), name );
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
    Json::Value list_object(
        const std::string& name
    ) {
        return list_object( manager::shared_group(), name );
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
        const std::string& name, 
        int offset, 
        int page_size
    ) {
        return list_object( manager::shared_group(), name, offset, page_size );
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
    Json::Value list_object( 
        const std::string& name, 
        int offset, 
        int page_size, 
        const std::vector< std::string >& filter_keys 
    ) {
        return list_object( manager::shared_group(), 
            name, offset, page_size, filter_keys);
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
            std::string _id = jobject["id"].asString();
            Json::Value _old = get_object(rg, name, _id);
            std::map< std::string, std::string > _new_uk;
            std::vector< std::string > _old_uk;
            // this is an patch op
            for ( auto& k : _keys ) {
                if ( !jobject.isMember(k.key) ) continue;

                std::string _v = jobject[k.key].asString();
                auto _fit = format.find(k.key);
                if ( _fit != format.end() ) {
                    _v = _fit->second(_v);
                }
                std::string _ov = _old[k.key].asString();
                if ( _ov == _v ) continue;  // do not need to update
                if ( k.unique && k.key != "id" ) {
                    std::string _uk = "dhboc.unique." + name + "." + k.key + "." + _v;
                    auto _r = rg->query("GET", _uk);
                    if ( !_r[0].is_nil() && _r[0].content != _id ) {
                        // New value is not unique
                        return 1;
                    }
                    _new_uk[_uk] = _id;
                    _old_uk.push_back("dhboc.unique." + name + "." + k.key + "." + _ov);
                }
                _itemkv[k.key] = _v;
            }
            for ( auto& ki : _new_uk ) {
                ignore_result(rg->query("SET", ki.first, ki.second));
            }
            for ( auto& k : _old_uk ) {
                ignore_result(rg->query("DEL", k));
            }
            _itemkv["id"] = _id;
            __update_object(rg, name, _itemkv);
        } else {
            // this is an add op
            std::string _id = utils::md5(
                std::to_string(getpid()) + 
                std::to_string(task_time_now().time_since_epoch().count())
            );
            std::map< std::string, std::string > _new_uk;
            for ( auto& k : _keys ) {
                std::string _v = k.dvalue;
                if ( jobject.isMember(k.key) ) {
                    _v = jobject[k.key].asString();
                }
                auto _fit = format.find(k.key);
                if ( _fit != format.end() ) {
                    _v = _fit->second(_v);
                }

                if ( k.unique && k.key != "id" ) {
                    std::string _uk = "dhboc.unique." + name + "." + k.key + "." + _v;
                    auto _r = rg->query("GET", _uk);
                    if ( !_r[0].is_nil() ) {
                        // New value is not unique
                        return 1;
                    }
                    _new_uk[_uk] = _id;
                }
                for ( auto& ki : _new_uk ) {
                    ignore_result(rg->query("SET", ki.first, ki.second));
                }

                _itemkv[k.key] = _v;
            }
            _itemkv["id"] = _id;
            __add_object(rg, name, _itemkv);
        }
        return 0;
    }
    int patch_object( 
        const std::string& name, 
        const Json::Value& jobject,
        const format_map_t& format
    ) {
        return patch_object(manager::shared_group(), name, jobject, format);
    }

    int patch_object(
        redis_connector_t rg,
        const std::string& name,
        const Json::Value& jobject
    ) {
        return patch_object(rg, name, jobject, {});
    }
    int patch_object(
        const std::string& name,
        const Json::Value& jobject
    ) {
        return patch_object(manager::shared_group(), 
            name, jobject, {});
    }

    int patch_object(
        redis_connector_t rg,
        const std::string& name,
        const std::map< std::string, std::string > kv
    ) {
        Json::Value _jobj(Json::objectValue);
        for ( auto& _kv : kv ) {
            _jobj[_kv.first] = _kv.second;
        }
        return patch_object(rg, name, _jobj, {});
    }
    int patch_object(
        const std::string& name,
        const std::map< std::string, std::string > kv
    ) {
        return patch_object( manager::shared_group(), name, kv );
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
    Json::Value get_object(
        const std::string& name,
        const std::string& id
    ) {
        return get_object( manager::shared_group(), name, id );
    }

    Json::Value get_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& unique_key,
        const std::string& value
    ) {
        auto _oit = g_object_infos.find(name);
        if ( _oit == g_object_infos.end() ) {
            return Json::Value(Json::nullValue);
        }
        // Check if the unique key existed
        auto _ukit = _oit->second.find(unique_key);
        if ( _ukit == _oit->second.end() ) {
            return Json::Value(Json::nullValue);
        }
        // The Key is not unique
        if ( !_ukit->second.unique ) {
            return Json::Value(Json::nullValue);
        }
        std::string _item_id_key = (
            "dhboc.unique." + name + "." + 
            unique_key + "." + value);
        auto _iid = rg->query("GET", _item_id_key);
        // No such object
        if ( _iid.size() == 0 || _iid[0].is_nil() ) {
            return Json::Value(Json::nullValue);
        }
        auto _keys = __get_keys(name);
        return __get_item(rg, _iid[0].content, _keys);
    }
    Json::Value get_object(
        const std::string& name,
        const std::string& unique_key,
        const std::string& value
    ) {
        return get_object( manager::shared_group(), name, unique_key, value );
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
    Json::Value query_object(
        const std::string& name,
        const filter_map_t& filters
    ) {
        return query_object( manager::shared_group(), name, filters );
    }

    // Delete object
    int delete_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    ) {
        auto _keys = __get_keys(name);
        _keys.emplace_back((properity_t){
            "__type__", R_STRING, true, std::string("")
        });
        auto _o = __get_item(rg, id, _keys);
        // No such id
        if ( _o.isNull() ) return 1;
        // Invalidate type
        if ( _o["__type__"].asString() != name ) return 1;

        // Remove all unique index key
        for ( auto& k : _keys ) {
            if ( !k.unique ) continue;
            ignore_result(rg->query(
                "DEL", "dhboc.unique." + name + "." + 
                k.key + "." + _o[k.key].asString()));
        }

        // Remove item from all list
        ignore_result(rg->query("LREM", "dhboc." + name + ".pin_ids", 1, id));
        ignore_result(rg->query("LREM", "dhboc." + name + ".ids", 1, id));

        // Remove the object
        ignore_result(rg->query("DEL", "dhboc.__item__." + id));

        return 0;
    }
    int delete_object(
        const std::string& name,
        const std::string& id
    ) {
        return delete_object( manager::shared_group(), name, id );
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
    int pin_object(
        const std::string& name,
        const std::string& id
    ) {
        return pin_object( manager::shared_group(), name, id );
    }

    int unpin_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    ) {
        ignore_result(rg->query("LREM", "dhboc." + name + ".pin_ids", 1, id));
        return 0;
    }
    int unpin_object(
        const std::string& name,
        const std::string& id
    ) {
        return unpin_object( manager::shared_group(), name, id );
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
    int set_pin_limit(
        const std::string& name,
        int limit
    ) {
        return set_pin_limit( manager::shared_group(), name, limit );
    }
}};

// Push Chen
