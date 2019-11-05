/*
    modules.cpp
    PECoTask
    2019-10-31
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)

    Copyright 2015-2018 MeetU Infomation and Technology Inc. All rights reserved.
*/
#include "modules.h"
#include <dlfcn.h>

std::string module_get_info( module_t mh, const char* method ) {
    module_info_f _fh = (module_info_f)dlsym(mh, method);
    const char* _sym_error = dlerror();
    if ( _sym_error ) throw _sym_error;
    return _fh();
}

std::vector< std::string > module_get_config( module_t mh, const char* method ) {
    module_config_f _fh = (module_config_f)dlsym(mh, method);
    const char* _sym_error = dlerror();
    if ( _sym_error ) throw _sym_error;
    return _fh();
}

// Global module manager
// Singleton
modulemgr& modulemgr::mgr() {
    static modulemgr _; return _;
}

modulemgr::modulemgr() {
    dhboc_module _dm{
        NULL,
        std::string("redis-group"),
        "pe::co::net::redis::group",
        std::vector<std::string>{},
        std::vector<std::string>{},
        std::vector<std::string>{},
    };
    module_map_[_dm.module_name] = _dm;
}

modulemgr::~modulemgr() {
    for ( auto& kv : module_map_ ) {
        if ( kv.second.module != NULL ) dlclose(kv.second.module);
    }
}
// Load a module and add to the cache
bool modulemgr::load_module( const std::string& module_name ) {
    module_t _hm = dlopen( module_name.c_str(), RTLD_LAZY | RTLD_GLOBAL );
    if ( !_hm ) {
        std::cerr << "failed to load the module `" << module_name << "`" << std::endl;
        return false;
    }

    // Reset the error flag
    dlerror();
    try {
        dhboc_module _dm{
            _hm, 
            module_get_info(_hm, "module_name"),
            module_get_info(_hm, "require_type"),
            module_get_config(_hm, "header_files"),
            module_get_config(_hm, "compile_flags"),
            module_get_config(_hm, "link_flags")
        };
        // Try to connect all strings
        for ( auto& i : _dm.header_files ) {
            mgr().includes_ += ("#include <" + i + ">\n");
        }
        for ( auto& c : _dm.compile_flags ) {
            for ( auto& kv : mgr().module_map_ ) {
                if ( std::find(
                        kv.second.compile_flags.begin(), 
                        kv.second.compile_flags.end(), 
                        c
                    ) == kv.second.compile_flags.end() 
                ) {
                    mgr().compile_flags_ += (c + " ");
                }
            }
        }
        for ( auto& l : _dm.link_flags ) {
            for ( auto& kv : mgr().module_map_ ) {
                if ( std::find(
                        kv.second.link_flags.begin(), 
                        kv.second.link_flags.end(), 
                        l
                    ) == kv.second.link_flags.end() 
                ) {
                    mgr().link_flags_ += (l + " ");
                }
            }
        }
        mgr().module_map_[_dm.module_name] = _dm;
    } catch( const char * msg ) {
        std::cerr << "exception: " << msg << std::endl;
    } catch( std::string& msg ) {
        std::cerr << "exception: " << msg << std::endl;
    } catch( std::stringstream& msg ) {
        std::cerr << "exception: " << msg.str() << std::endl;
    } catch ( std::logic_error& ex ) {
        std::cerr << "logic error: " << ex.what() << std::endl;
    } catch ( std::runtime_error& ex ) {
        std::cerr << "runtime error: " << ex.what() << std::endl;
    } catch ( std::bad_exception& ex ) {
        std::cerr << "bad exception: " << ex.what() << std::endl;
    } catch( ... ) {
        std::cerr << "uncaught exception handled" << std::endl;
    }

    #ifdef DEBUG
    std::cout << "module " << module_name << " loaded" << std::endl;
    #endif
    return true;
}

// Get all include file list as a formated string
std::string modulemgr::all_include() { return mgr().includes_; }

// Get all compile flags as a string
std::string modulemgr::compile_flags() { return mgr().compile_flags_; }

// Get all link flags as a string
std::string modulemgr::link_flags() { return mgr().link_flags_; }

// Get the require type of a module
std::string modulemgr::require_type( const std::string& module_name ) {
    auto _it = mgr().module_map_.find(module_name);
    if ( _it == mgr().module_map_.end() ) return std::string("");
    return _it->second.require_type;
}

// Push Chen
