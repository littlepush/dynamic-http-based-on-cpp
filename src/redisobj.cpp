/*
    redisobj.cpp
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-01-29
    Push Chen

    Copyright 2015-2016 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include "redisobj.h"
#include <unordered_map>

// Utils
namespace dhboc { namespace redis {
    std::map< std::string, std::map< std::string, properity_t > >   g_object_infos;
    utils::lrucache< std::string, Json::Value >                     g_object_cache;
    const filter_map_t                                              empty_filter_map = {};
    const std::vector< std::string >                                empty_filter_keys = {};
    std::vector< properity_t >                                      empty_fetch_keys = {};

    #define CACHE_NOTIFICATION_KEY                                  "__inner_object_patch__"

    void enable_item_cache( size_t cache_size ) {
        g_object_cache.set_cache_size( cache_size );
        manager::register_notification(CACHE_NOTIFICATION_KEY, [](const std::string& value) {
            g_object_cache.erase(value);
            return true;
        });
    }

    Json::Value __get_item(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::vector< properity_t >& filter_keys = empty_fetch_keys
    ) {
        Json::Value& _item = g_object_cache.get(id, [&](const std::string& key) {
            // Get full object
            net::proto::redis::command _cmd;
            auto _oit = g_object_infos.find(name);
            if ( _oit == g_object_infos.end() ) return Json::Value(Json::nullValue);

            _cmd << "HMGET" << "dhboc.__item__." + key;
            std::vector< properity_t > _prop_keys;
            for ( auto& _pkv : _oit->second ) {
                _cmd << _pkv.first;
                _prop_keys.push_back( _pkv.second );
            }
            _cmd << "id" << "__ctime__" << "__utime__" << "__type__";
            auto _r = rg->query( std::move(_cmd) );
            // Type not match or not existed
            if ( _r.rbegin()->content != name ) {
                return Json::Value(Json::nullValue);
            }
            Json::Value _result(Json::objectValue);
            
            for ( size_t i = 0; i < _prop_keys.size(); ++i ) {
                rtype _t = _prop_keys[i].type;
                std::string _k = _prop_keys[i].key;
                if ( _t == R_NULL ) {
                    if ( _k == "__dummy__" ) continue;
                    _result[_k] = Json::Value(Json::nullValue);
                    continue;
                }
                if ( _r[i].is_nil() || _r[i].is_empty() || _r[i].content.size() == 0 ) {
                    if ( _t == R_STRING ) {
                        _result[_k] = _prop_keys[i].dvalue;
                        continue;
                    }
                    if ( _t == R_NUMBER && _prop_keys[i].dvalue.size() > 0 ) {
                        _result[_k] = std::stof(_prop_keys[i].dvalue);
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
            size_t _ts = _prop_keys.size();
            _result["id"] = _r[_ts].content;
            _result["create_time"] = _r[_ts + 1].content;
            _result["update_time"] = _r[_ts + 2].content;
            _result["__type__"] = _r[_ts + 3].content;

            return _result;
        });

        // Invalidate item
        if ( _item.isNull() ) {
            g_object_cache.erase(id);
            return Json::Value(Json::nullValue);
        }

        // If not need to filter, return the item itself(copyed)
        if ( filter_keys.size() == 0 ) return _item;

        // Copy the value
        Json::Value _result(Json::objectValue);
        for ( auto& p : filter_keys ) {
            _result[p.key] = _item[p.key];
        }
        _result["id"] = _item["id"];
        _result["create_time"] = _item["create_time"];
        _result["update_time"] = _item["update_time"];
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
        for ( auto& p : _oit->second ) {
            _keys.push_back(p.second);
        }
        return _keys;
    }

    std::vector< std::string > __list_ids(
        redis_connector_t rg, 
        const std::string& name, 
        bool pin
    ) {
        std::vector< std::string > _ids;
        auto _r = rg->query("LRANGE", "dhboc." + name + (pin ? ".pin_ids" : ".ids"), 0, -1);
        for ( auto& id : _r ) {
            _ids.push_back(id.content);
        }
        return _ids;
    }

    Json::Value __list_object(
        redis_connector_t rg,
        const std::string& name,
        int offset,
        int page_size,
        const std::vector< properity_t >& keys,
        const filter_map_t& filters
    ) {
        auto _pin_ids = __list_ids(rg, name, true);
        auto _ids = __list_ids(rg, name, false);

        if ( page_size == -1 ) {
            page_size = (int)_ids.size() - offset;
        }

        auto _filterp = [&]( const std::string& id ) -> bool {
            auto _item = __get_item(rg, name, id);
            if ( _item.isNull() ) return false;
            for ( auto& fkv : filters ) {
                if ( !_item.isMember(fkv.first) ) return false;
                if ( !fkv.second(_item[fkv.first].asString()) ) return false;
            }
            return true;
        };

        std::vector< std::string > _all_ids;
        for ( auto& _id : _pin_ids ) {
            if ( !_filterp(_id) ) continue;
            _all_ids.push_back(_id);
        }

        for ( auto& _id : _ids ) {
            auto _pit = std::find(_pin_ids.begin(), _pin_ids.end(), _id);
            if ( _pit != _pin_ids.end() ) continue;
            if ( !_filterp(_id) ) continue;
            _all_ids.push_back(_id);
        }

        Json::Value _rlist(Json::arrayValue);
        for ( size_t i = (size_t)offset; i < (size_t)(offset + page_size) && i < _all_ids.size(); ++i ) {
            Json::Value _item = __get_item(rg, name, _all_ids[i], keys);
            auto _pit = std::find(_pin_ids.begin(), _pin_ids.end(), _all_ids[i]);
            _item["is_pin"] = (_pit != _pin_ids.end());
            _rlist.append(_item);
        }

        Json::Value _result(Json::objectValue);
        _result["all"] = (int)_all_ids.size();
        _result["offset"] = offset;
        _result["limit"] = page_size;
        _result["data"] = _rlist;
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
        manager::send_notification(CACHE_NOTIFICATION_KEY, kvs["id"]);
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
    // Unregister an object type
    int unregister_object(
        const std::string& name
    ) {
        auto _it = g_object_infos.find(name);
        if ( _it == g_object_infos.end() ) return 1;
        g_object_infos.erase(_it);
        return 0;
    }

    // Get all objects' name
    std::list< std::string > all_objects() {
        std::list< std::string > _names;
        for ( auto& kv : g_object_infos ) {
            _names.push_back(kv.first);
        }
        return _names;
    }

    // Get the count of specifial object
    int count_object( redis_connector_t rg, const std::string& name ) {
        auto _r = rg->query("LLEN", "dhboc." + name + ".ids");
        if ( _r.size() == 0 ) return 0;
        if ( _r[0].is_nil() || _r[0].is_empty() ) return 0;
        return std::stoi(_r[0].content);
    }
    int count_object( const std::string& name ) {
        return count_object( manager::shared_group(), name );
    }

    // Get the list of value
    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name,
        int offset,
        int page_size,
        const std::vector< std::string >& filter_keys,
        const filter_map_t& filters
    ) {
        auto _oit = g_object_infos.find(name);
        if ( _oit == g_object_infos.end() ) {
            Json::Value _result(Json::objectValue);
            _result["all"] = 0;
            _result["offset"] = offset;
            _result["limit"] = page_size;
            _result["data"] = Json::Value(Json::arrayValue);
            return _result;
        }
        std::vector< properity_t > _keys;
        for ( const auto& k : filter_keys ) {
            auto _kit = _oit->second.find(k);
            if ( _kit == _oit->second.end() ) continue;
            _keys.push_back(_kit->second);
        }
        return __list_object(rg, name, offset, page_size, _keys, filters);
    }
    Json::Value list_object(
        const std::string& name,
        int offset,
        int page_size,
        const std::vector< std::string >& filter_keys,
        const filter_map_t& filters
    ) {
        return list_object(manager::shared_group(), name, offset, page_size, filter_keys, filters);
    }

    // List only ids
    std::vector< std::string > list_object_ids( 
        redis_connector_t rg, const std::string& name,
        const filter_map_t& filters
    ) {
        if ( filters.size() == 0 ) return __list_ids(rg, name, false);
        std::vector< std::string > _result_ids;
        auto _ids = __list_ids(rg, name, false);
        for ( auto& _id : _ids ) {
            bool _match = true;
            auto _item = __get_item(rg, name, _id);
            if ( _item.isNull() ) continue;
            for ( auto& fkv : filters ) {
                if ( !_item.isMember(fkv.first) ) {
                    _match = false; break;
                }
                if ( !fkv.second(_item[fkv.first].asString()) ) {
                    _match = false; break;
                }
            }
            if ( !_match ) continue;
            _result_ids.push_back(_id);
        }
        return _result_ids;
    }
    std::vector< std::string > list_object_ids( 
        const std::string& name,
        const filter_map_t& filters
    ) {
        return list_object_ids( manager::shared_group(), name, filters );
    }

    // Get the list of value
    Json::Value list_object( 
        redis_connector_t rg, 
        const std::string& name, 
        int offset, 
        int page_size,
        const filter_map_t& filters
    ) {
        return __list_object(rg, name, offset, page_size, empty_fetch_keys, filters);
    }
    Json::Value list_object( 
        const std::string& name, 
        int offset, 
        int page_size,
        const filter_map_t& filters
    ) {
        return list_object( manager::shared_group(), name, offset, page_size, filters );
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
        return __get_item(rg, name, id);
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
        return __get_item(rg, name, _iid[0].content);
    }
    Json::Value get_object(
        const std::string& name,
        const std::string& unique_key,
        const std::string& value
    ) {
        return get_object( manager::shared_group(), name, unique_key, value );
    }

    // Delete object
    int delete_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    ) {
        // No Such type
        auto _keys = __get_keys(name);
        if ( _keys.size() == 0 ) return 1;

        auto _o = __get_item(rg, name, id);
        // No such id
        if ( _o.isNull() ) return 1;

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

        // Remove from the cache
        manager::send_notification(CACHE_NOTIFICATION_KEY, id);

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
        auto _item = __get_item(rg, name, id);
        if ( _item.isNull() ) return 1;

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
