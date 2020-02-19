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
    utils::lrucache< std::string, std::vector< std::string > >      g_ids_cache;

    const filter_map_t                                              empty_filter_map = {};
    const std::vector< std::string >                                empty_filter_keys = {};
    std::vector< properity_t >                                      empty_fetch_keys = {};
    const std::string                                               empty_default_value = "";

    #define CACHE_NOTIFICATION_KEY                                  "__inner_object_patch__"
    #define IDS_NOTIFICATION_KEY                                    "__inner_ids_patch__"

    properity_t generate_property(
        const std::string& key, 
        rtype type,
        bool unique,
        const std::string& dvalue,
        bool ordered
    ) {
        return (properity_t){
            key, type, unique, dvalue, ordered
        };
    }

    void enable_item_cache( size_t cache_size ) {
        g_object_cache.set_cache_size( cache_size );
        g_ids_cache.set_cache_size( cache_size );
        manager::register_notification(CACHE_NOTIFICATION_KEY, [](const std::string& value) {
            g_object_cache.erase(value);
            return true;
        });
        manager::register_notification(IDS_NOTIFICATION_KEY, [](const std::string& value) {
            g_ids_cache.erase(value);
            return true;
        });
    }

    bool __string_to_bool( const std::string& value ) {
        if ( value.size() == 0 ) return false;
        if ( value == "true" ) return true;
        if ( value == "false" ) return false;
        if ( value == "0" ) return false;
        return true;
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
                    if ( _t == R_BOOLEAN && _prop_keys[i].dvalue.size() > 0 ) {
                        _result[_k] = __string_to_bool(_prop_keys[i].dvalue);
                    }
                    _result[_k] = Json::Value(Json::nullValue);
                    continue;
                }
                if ( _t == R_STRING ) {
                    _result[_k] = _r[i].content;
                } else if ( _t == R_NUMBER ) {
                    _result[_k] = std::stof(_r[i].content);
                } else if ( _t == R_BOOLEAN ) {
                    _result[_k] = __string_to_bool(_r[i].content);
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
        const std::string& tag
    ) {
        char _op = tag[0];
        std::string _tagkey = tag;
        if ( _op == '<' || _op == '>' ) {
            _tagkey = tag.substr(1);
        }
        if ( _op != '<' && _op != '>' ) {
            _op = '<';
        }
        std::vector< std::string > _ids = g_ids_cache.get(
            "dhboc." + name + "." + _tagkey + ".ids", 
            [&](const std::string& lkey) {
                std::vector< std::string > _result;
                auto _r = rg->query("ZRANGE", lkey, 0, -1);
                for ( auto& id : _r ) {
                    _result.push_back(id.content);
                }
                return _result;
            }
        );
        if ( _op == '>' ) {
            std::reverse(_ids.begin(), _ids.end());
        }
        return _ids;
    }

    /*
        tag format:
        ^<tag> : put this tag in front of the list
        $<tag> : put this tag in end of the list
        +<tag> : only match the tag
        -<tag> : only not contains the tag
    */
    std::vector< std::string > __filter_ids(
        redis_connector_t rg,
        const std::string& name,
        const list_arg_t& arg
    ) {
        std::string _order = arg.orderby;

        // Get all ids
        if ( _order.size() == 0 ) _order = "<__ctime__";
        auto _ids = __list_ids(rg, name, _order);

        // Foreach tag, filter the ids
        for ( auto& _tag : arg.tags ) {
            std::string _t = _tag;
            if ( _tag[0] == '^' ) {
                _t = _tag.substr(1);
                auto _tids = __list_ids(rg, name, _t);
                for ( auto _tit = _tids.rbegin(); _tit != _tids.rend(); ++_tit ) {
                    auto _iit = std::find( _ids.begin(), _ids.end(), *_tit );
                    if ( _iit != _ids.end() ) _ids.erase(_iit);
                    _ids.insert(_ids.begin(), *_tit);
                }
            } else if ( _tag[0] == '$' ) {
                _t = _tag.substr(1);
                auto _tids = __list_ids(rg, name, _t);
                for ( auto _tit = _tids.begin(); _tit != _tids.end(); ++_tit ) {
                    auto _iit = std::find( _ids.begin(), _ids.end(), *_tit );
                    if ( _iit != _ids.end() ) _ids.erase(_iit);
                    _ids.push_back(*_tit);
                }                
            } else if ( _tag[0] == '-' ) {
                _t = _tag.substr(1);
                auto _tids = __list_ids(rg, name, _t);
                for ( auto _tit = _tids.begin(); _tit != _tids.end(); ++_tit ) {
                    auto _iit = std::find( _ids.begin(), _ids.end(), *_tit );
                    if ( _iit != _ids.end() ) _ids.erase(_iit);
                }
            } else {
                if ( _tag[0] == '+' ) {
                    _t = _tag.substr(1);
                }
                std::vector< std::string > _match_ids;
                auto _tids = __list_ids(rg, name, _t);
                for ( auto& _id : _ids ) {
                    auto _it = std::find(_tids.begin(), _tids.end(), _id);
                    if ( _it != _tids.end() ) _match_ids.push_back(_id);
                }
                _ids = _match_ids;
            }
        }

        // Filter

        // Check the filter map
        filter_map_t _f;
        for ( auto& kv : arg.filters ) {
            if ( kv.first.size() == 0 || kv.second == nullptr ) continue;
            _f[kv.first] = kv.second;
        }

        auto _filterp = [&]( const std::string& id ) -> bool {
            // Always return true if no filter
            if ( _f.size() == 0 ) return true;
            // Get the item and check the filter
            auto _item = __get_item(rg, name, id);
            if ( _item.isNull() ) return false;
            for ( auto& fkv : _f ) {
                if ( !_item.isMember(fkv.first) ) return false;
                if ( !fkv.second(_item[fkv.first].asString()) ) return false;
            }
            return true;
        };

        std::vector< std::string > _all_ids;
        for ( auto& _id : _ids ) {
            if ( !_filterp(_id) ) continue;
            _all_ids.push_back(_id);
        }

        return _all_ids;
    }
    Json::Value __list_object(
        redis_connector_t rg,
        const std::string& name,
        const list_arg_t& arg
    ) {
        auto _all_ids = __filter_ids(rg, name, arg);

        std::vector< properity_t > _fkeys;
        auto _oit = g_object_infos.find(name);
        for ( auto& k : arg.keys ) {
            auto _kit = _oit->second.find(k);
            if ( _kit == _oit->second.end() ) continue;
            _fkeys.push_back(_kit->second);
        }

        Json::Value _rlist(Json::arrayValue);
        for ( 
			size_t i = (size_t)arg.offset; 
			i < (size_t)(arg.offset + arg.page_size) && i < _all_ids.size(); 
			++i
		) {
            Json::Value _item = __get_item(rg, name, _all_ids[i], _fkeys);
            if ( _item.isNull() ) continue;
            _rlist.append(_item);
        }

        Json::Value _result(Json::objectValue);
        _result["all"] = (int)_all_ids.size();
        _result["offset"] = arg.offset;
        _result["limit"] = arg.page_size;
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
        std::string _idsckey = "dhboc." + name + ".__ctime__.ids";
        ignore_result(rg->query("ZADD", _idsckey, kvs["__ctime__"], kvs["id"]));
        manager::send_notification(IDS_NOTIFICATION_KEY, _idsckey);

        std::string _idsukey = "dhboc." + name + ".__utime__.ids";
        ignore_result(rg->query("ZADD", _idsukey, kvs["__utime__"], kvs["id"]));
        manager::send_notification(IDS_NOTIFICATION_KEY, _idsukey);
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

        std::string _idsukey = "dhboc." + name + ".__utime__.ids";
        ignore_result(rg->query("ZADD", _idsukey, kvs["__utime__"], kvs["id"]));
        manager::send_notification(IDS_NOTIFICATION_KEY, _idsukey);
    }

    // Register an object in the memory
    int register_object( 
        const std::string& name, 
        const std::vector< properity_t >& properties
    ) {
        if ( g_object_infos.find(name) != g_object_infos.end() ) return 1;
        std::map< std::string, properity_t > _key_map;
        for ( const auto& p : properties ) {
            if ( p.type == R_STRING && p.ordered ) {
                rlog::error << "property: " << p.key << 
                    " is not validate, string cannot be ordered" << std::endl;
                return 2;
            }
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
        auto _r = rg->query("ZCARD", "dhboc." + name + ".__ctime__.ids");
        if ( _r.size() == 0 ) return 0;
        if ( _r[0].is_nil() || _r[0].is_empty() ) return 0;
        return std::stoi(_r[0].content);
    }
    int count_object( const std::string& name ) {
        return count_object( manager::shared_group(), name );
    }

    int index_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& orderby,
        const std::vector< std::string > tags
    ) {
        list_arg_t _arg;
        _arg.offset = 0;
        _arg.page_size = -1;
        _arg.orderby = orderby;
        _arg.tags = tags;

        auto _all_ids = __filter_ids(rg, name, _arg);
        for ( size_t i = 0; i < _all_ids.size(); ++i ) {
            if ( _all_ids[i] == id ) return (int)i;
        }
        return -1;
    }
    int index_object(
        const std::string& name,
        const std::string& id,
        const std::string& orderby,
        const std::vector< std::string > tags
    ) {
        return index_object(manager::shared_group(), name, id, orderby, tags);
    }
    // Get the list of value
    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name
    ) {
        list_arg_t _arg;
        _arg.offset = 0;
        _arg.page_size = -1;
        return __list_object(rg, name, _arg);        
    }
    Json::Value list_object(
        const std::string& name
    ) {
        return list_object(manager::shared_group(), name);        
    }

    Json::Value list_object(
        redis_connector_t rg, 
        const std::string& name,
        const std::vector< std::string > tags
    ) {
        list_arg_t _arg;
        _arg.offset = 0;
        _arg.page_size = -1;
        _arg.tags = tags;
        return __list_object(rg, name, _arg);
    }
    Json::Value list_object(
        const std::string& name,
        const std::vector< std::string > tags
    ) {
        return list_object(manager::shared_group(), name, tags);
    }
    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name,
        const list_arg_t& arg
    ) {
        return __list_object(rg, name, arg);
    }
    Json::Value list_object(
        const std::string& name,
        const list_arg_t& arg
    ) {
        return __list_object(manager::shared_group(), name, arg);
    }
    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name,
        int offset,
        int page_size,
        const std::string& orderby
    ) {
        list_arg_t _arg;
        _arg.offset = offset;
        _arg.page_size = page_size;
        _arg.orderby = orderby;
        return __list_object(rg, name, _arg);
    }
    Json::Value list_object(
        const std::string& name,
        int offset,
        int page_size,
        const std::string& orderby
    ) {
        return list_object( manager::shared_group(), name, offset, page_size, orderby);
    }
    Json::Value list_object(
        redis_connector_t rg,
        const std::string& name,
        int offset,
        int page_size,
        const std::string& orderby,
        const std::vector< std::string >& tags
    ) {
        list_arg_t _arg;
        _arg.offset = offset;
        _arg.page_size = page_size;
        _arg.orderby = orderby;
        _arg.tags = tags;
        return __list_object(rg, name, _arg);
    }
    Json::Value list_object(
        const std::string& name,
        int offset,
        int page_size,
        const std::string& orderby,
        const std::vector< std::string >& tags
    ) {
        return list_object( manager::shared_group(), name, offset, page_size, orderby, tags );
    }

    // Get the list of value
    Json::Value list_object( 
        redis_connector_t rg, 
        const std::string& name, 
        int offset, 
        int page_size,
        const filter_map_t& filters
    ) {
        list_arg_t _arg;
        _arg.offset = offset;
        _arg.page_size = page_size;
        _arg.filters = filters;
        return __list_object(rg, name, _arg);
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
    std::string patch_object( 
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
            std::vector< std::string > _order_k;
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
                        return std::string("");
                    }
                    _new_uk[_uk] = _id;
                    _old_uk.push_back("dhboc.unique." + name + "." + k.key + "." + _ov);
                }
                if ( k.ordered ) {
                    _order_k.push_back(k.key);
                }
                _itemkv[k.key] = _v;
            }
            for ( auto& ki : _new_uk ) {
                ignore_result(rg->query("SET", ki.first, ki.second));
            }
            for ( auto& k : _old_uk ) {
                ignore_result(rg->query("DEL", k));
            }
            for ( auto& k : _order_k ) {
                // Update the order's update time
                std::string _lk = "dhboc." + name + "." + k + ".ids";
                ignore_result(rg->query("ZADD", _lk, _itemkv[k], _id));
                manager::send_notification(IDS_NOTIFICATION_KEY, _lk);
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
            std::vector< std::string > _order_k;
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
                        return std::string("");
                    }
                    _new_uk[_uk] = _id;
                }
                for ( auto& ki : _new_uk ) {
                    ignore_result(rg->query("SET", ki.first, ki.second));
                }
                if ( k.ordered ) {
                    _order_k.push_back(k.key);
                }
                _itemkv[k.key] = _v;
            }
            for ( auto& k : _order_k ) {
                // Update the order's update time
                std::string _lk = "dhboc." + name + "." + k + ".ids";
                ignore_result(rg->query("ZADD", _lk, _itemkv[k], _id));
                manager::send_notification(IDS_NOTIFICATION_KEY, _lk);
            }
            _itemkv["id"] = _id;
            __add_object(rg, name, _itemkv);
        }
        return _itemkv["id"];
    }
    std::string patch_object( 
        const std::string& name, 
        const Json::Value& jobject,
        const format_map_t& format
    ) {
        return patch_object(manager::shared_group(), name, jobject, format);
    }

    std::string patch_object(
        redis_connector_t rg,
        const std::string& name,
        const Json::Value& jobject
    ) {
        return patch_object(rg, name, jobject, {});
    }
    std::string patch_object(
        const std::string& name,
        const Json::Value& jobject
    ) {
        return patch_object(manager::shared_group(), 
            name, jobject, {});
    }

    std::string patch_object(
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
    std::string patch_object(
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
            if ( k.unique ) {
                ignore_result(rg->query(
                    "DEL", "dhboc.unique." + name + "." + 
                    k.key + "." + _o[k.key].asString()));
            }
        }

        // Remove from all list
        std::vector< std::string > _idkeys;
        int _offset = 0;
        do {
            auto _r = rg->query("SCAN", _offset, "MATCH", "dhboc." + name + ".*.ids");
            _offset = std::stoi(_r[0].content);
            for ( auto& r : _r[1].subObjects ) {
                _idkeys.push_back(r.content);
            }
        } while ( _offset != 0 );

        for ( auto& lid : _idkeys ) {
            auto _r = rg->query("ZREM", lid, id);
            if ( _r[0].content == "0" ) continue;
            manager::send_notification(IDS_NOTIFICATION_KEY, lid);
        }

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

    int inc_order_value(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value
    ) {
        auto _oit = g_object_infos.find(name);
        if ( _oit == g_object_infos.end() ) return 1;
        auto _kit = _oit->second.find(key);
        if ( _kit == _oit->second.end() ) return 2;
        if ( !_kit->second.ordered ) return 3;

        auto _item = get_object(rg, name, id);
        if ( _item.isNull() ) return 4;

        int64_t _ov = _item["key"].asInt64();
        _ov += value;
        patch_object(rg, name, {
            {"id", id},
            {key, std::to_string(_ov)}
        });
        return 0;
    }
    int inc_order_value(
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value
    ) {
        return inc_order_value( manager::shared_group(), name, id, key, value );
    }
    int dcr_order_value(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value
    ) {
        return inc_order_value( rg, name, id, key, (value * -1) );
    }
    int dcr_order_value(
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value
    ) {
        return dcr_order_value( manager::shared_group(), name, id, key, value );
    }
    int set_order_value(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value
    ) {
        auto _oit = g_object_infos.find(name);
        if ( _oit == g_object_infos.end() ) return 1;
        auto _kit = _oit->second.find(key);
        if ( _kit == _oit->second.end() ) return 2;
        if ( !_kit->second.ordered ) return 3;

        patch_object(rg, name, {
            {"id", id},
            {key, std::to_string(value)}
        });
        return 0;
    }
    int set_order_value(
        const std::string& name,
        const std::string& id,
        const std::string& key,
        int64_t value
    ) {
        return set_order_value( manager::shared_group(), name, id, key, value );
    }

    int tag_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::vector< std::string >& tags
    ) {
        for ( auto& t : tags ) {
            // Item not existed, ignore all rest tags
            if ( 1 == tag_object(rg, name, id, t) ) {
                return 1;
            }
        }
        return 0;
    }
    int tag_object(
        const std::string& name,
        const std::string& id,
        const std::vector< std::string >& tags
    ) {
        return tag_object(manager::shared_group(), name, id, tags);
    }
    int tag_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& tag
    ) {
        auto _item = __get_item(rg, name, id);
        if ( _item.isNull() ) return 1;

        std::string _tlid = "dhboc." + name + "." + tag + ".ids";
        ignore_result(rg->query("ZADD", _tlid, time(NULL), id));
        manager::send_notification(IDS_NOTIFICATION_KEY, _tlid);
        rg->query("SADD", "dhboc." + name + "." + id + ".tags", tag);
        return 0;
    }

    int tag_object(
        const std::string& name,
        const std::string& id,
        const std::string& tag
    ) {
        return tag_object(manager::shared_group(), name, id, tag);
    }
    int untag_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::vector< std::string >& tags
    ) {
        for ( auto& t : tags ) {
            // Item not existed, ignore all rest tags
            untag_object(rg, name, id, t);
        }
        return 0;
    }
    int untag_object(
        const std::string& name,
        const std::string& id,
        const std::vector< std::string >& tags
    ) {
        return untag_object(manager::shared_group(), name, id, tags);
    }
    int untag_object(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id,
        const std::string& tag
    ) {
        std::string _tlid = "dhboc." + name + "." + tag + ".ids";
        auto _r = rg->query("ZREM", _tlid, id);
        if ( _r[0].content == "0" ) return 1;
        manager::send_notification(IDS_NOTIFICATION_KEY, _tlid);
        rg->query("SREM", "dhboc." + name + "." + id + ".tags", tag);
        return 0;
    }
    int untag_object(
        const std::string& name,
        const std::string& id,
        const std::string& tag
    ) {
        return untag_object(manager::shared_group(), name, id, tag);
    }
    std::vector< std::string > get_tags(
        redis_connector_t rg,
        const std::string& name,
        const std::string& id
    ) {
        auto _r = rg->query("SMEMBERS", "dhboc." + name + "." + id + ".tags");
        std::vector< std::string > _tags;
        for ( auto& r : _r ) {
            _tags.push_back(r.content);
        }
        return _tags;
    }
    std::vector< std::string > get_tags(
        const std::string& name,
        const std::string& id
    ) {
        return get_tags(manager::shared_group(), name, id);
    }
    int taglist_count(
        redis_connector_t rg,
        const std::string& name,
        const std::string& tag
    ) {
        auto _ids = __list_ids(rg, name, tag);
        return (int)_ids.size();
    }
    int taglist_count(
        const std::string& name,
        const std::string& tag
    ) {
        return taglist_count( manager::shared_group(), name, tag );
    }
}};

// Push Chen
