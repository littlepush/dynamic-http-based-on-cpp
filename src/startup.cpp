/*
    startup.cpp
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2019-10-31
    Push Chen
*/

/*
MIT License

Copyright (c) 2020 Push Chen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "startup.h"
#include <dlfcn.h>

typedef void(*startup_initialize)( );

#ifdef __APPLE__
#ifdef DEBUG
const std::string EXT_NAME(".debug.dylib");
#else
const std::string EXT_NAME(".dylib");
#endif
const std::string CC("clang++");
const std::string INC_ROOT(CXX_FLAGS);
const std::string EX_FLAGS(LD_FLAGS);
#else
#ifdef DEBUG
const std::string EXT_NAME(".debug.so");
#else
const std::string EXT_NAME(".so");
#endif
const std::string CC("g++");
const std::string INC_ROOT(CXX_FLAGS);
const std::string EX_FLAGS(LD_FLAGS);
#endif

#ifdef DEBUG
#ifdef __APPLE__
const std::string OBJ_EXT(".darwin.debug.o");
#else
const std::string OBJ_EXT(".debug.o");
#endif
const std::string CC_DEFINES("-DDEBUG=1 -g");
#else
#ifdef __APPLE__
const std::string OBJ_EXT(".darwin.o");
#else
const std::string OBJ_EXT(".o");
#endif
const std::string CC_DEFINES("-DRELEASE=1 -O3");
#endif

// All runtime paths
const std::string& startupmgr::webroot_dir() {
    return startupmgr::_s_().webroot_path_;
}
const std::string& startupmgr::runtime_dir() {
    return startupmgr::_s_().runtime_path_;
}
const std::string& startupmgr::piece_dir() {
    return startupmgr::_s_().piece_path_;
}
const std::string& startupmgr::source_dir() {
    return startupmgr::_s_().src_path_;
}
const std::string& startupmgr::object_dir() {
    return startupmgr::_s_().obj_path_;
}
const std::string& startupmgr::server_dir() {
    return startupmgr::_s_().server_path_;
}
const std::string& startupmgr::lib_path() {
    return startupmgr::_s_().lib_path_;
}

// Compile source code, specified the input source file and the output obj path
bool startupmgr::compile_source(
    const std::string& src, 
    const std::string& obj, 
    const char* ex
) {
    time_t _src_time = utils::file_update_time(src);
    time_t _obj_time = utils::file_update_time(obj);
    // Source file not change, do not need to re-compile
    if ( _src_time < _obj_time ) return true; 
    std::vector< std::string > _cflags{
        CC, 
        "--std=c++11",
        "-pthread",
        "-Werror",
        "-Wall",
        "-fPIC",
        CC_DEFINES,
        INC_ROOT,
        modulemgr::compile_flags(),
        "-include dhboc/dhboc.h"
    };
    // Include all files
    for ( const auto& i : modulemgr::include_files() ) {
        _cflags.push_back(std::string("-include ") + i);
    }
    for ( const auto& i : app.pre_includes ) {
        _cflags.push_back(std::string("-include ") + startupmgr::webroot_dir() + i);
    }
    if ( ex != NULL ) _cflags.push_back(std::string(ex));
    _cflags.push_back("-c");
    _cflags.push_back(src);
    _cflags.push_back("-o");
    _cflags.push_back(obj);
    std::string _compile_cmd = utils::join(_cflags.begin(), _cflags.end(), " ");

#ifdef DEBUG
    std::cout << "compile cmd: " << _compile_cmd << std::endl;
#endif

    return (0 == system(_compile_cmd.c_str()));
}

// Link and create a shared library
bool startupmgr::create_library(
    const std::vector<std::string>& objs, 
    const std::string& libpath, 
    const char* ex
) {
    std::vector< std::string > _linkflags{
        CC,
        "-shared",
        "-o",
        libpath
    };
    for ( const auto& o : objs ) {
        _linkflags.push_back(o);
    }
    _linkflags.push_back(EX_FLAGS);
    _linkflags.push_back(modulemgr::link_flags());
    _linkflags.push_back("-lz -lssl -lcrypto -lpeco -ldhboc");
    if ( ex != NULL ) _linkflags.push_back(std::string(ex));

    std::string _link_cmd = utils::join(_linkflags.begin(), _linkflags.end(), " ");

#ifdef DEBUG
    std::cout << "link cmd: " << _link_cmd << std::endl;
#endif

    return (0 == system(_link_cmd.c_str()));
}


// Load the startup file and initialize everything
bool startupmgr::load_startup_file( 
    const std::string& sf, 
    const std::string& cxxflags, 
    bool force_rebuild ) {
    // Get the singleton object
    startupmgr& _s = startupmgr::_s_();
    // Initialize everything
    // std::string             startup_file_;
    _s.startup_file_ = sf;
    // std::string             webroot_path_;
    _s.webroot_path_ = utils::dirname(sf);
    if ( *(_s.webroot_path_.rbegin()) != '/' ) {
        _s.webroot_path_ += '/';
    }
    // std::string             runtime_path_;
    _s.runtime_path_ = _s.webroot_path_ + ".runtime/";
    if ( force_rebuild ) {
        utils::fs_remove(_s.runtime_path_);
    }
    if ( ! utils::rek_make_dir(_s.runtime_path_) ) {
        std::cerr << "failed to make runtime folder: " 
            << _s.runtime_path_ << std::endl;
        return false;
    }
    // std::string             piece_path_;
    _s.piece_path_ = _s.runtime_path_ + "pieces/";
    utils::make_dir( _s.piece_path_ );
    // std::string             src_path_;
    _s.src_path_ = _s.runtime_path_ + "src/";
    utils::make_dir( _s.src_path_ );
    // std::string             obj_path_;
    _s.obj_path_ = _s.runtime_path_ + "objs/";
    utils::make_dir( _s.obj_path_ );
    // std::string             server_path_;
    std::string _spath = _s.webroot_path_ + "_server/";
    std::vector< std::string > _web_sources;
    _web_sources.push_back(sf);
    time_t _last_update_time = utils::file_update_time(sf);
    bool _header_is_newer = false;
    if ( utils::is_folder_existed(_spath) ) {
        utils::rek_scan_dir(
            _spath, 
            [&_last_update_time, &_header_is_newer, &_web_sources]
            (const std::string& p, bool d) {
                if ( d ) return true;
                std::string _ext = utils::extension(p);
                if ( _ext == "cpp" || _ext == "c" ) {
                    _web_sources.push_back(p);
                    time_t _u = utils::file_update_time(p);
                    if ( _u > _last_update_time ) {
                        _last_update_time = _u;
                        _header_is_newer = false;
                    }
                } else if ( _ext == "h" || _ext == "hpp" ) {
                    time_t _u = utils::file_update_time(p);
                    if ( _u > _last_update_time ) {
                        _last_update_time = _u;
                        _header_is_newer = true;
                    }
                }
                return true;
            }
        );
        _s.server_path_ = _spath;
    }
    // std::string             lib_path_;
    std::string _libname = (
        utils::filename(sf) + '.' + 
        std::to_string((long)_last_update_time) + 
        EXT_NAME
    );
    _s.lib_path_ = _s.runtime_path_ + _libname;

    // Check if need to rebuild the lib
    if ( !utils::is_file_existed( _s.lib_path_ ) ) {
        // Build it
        if ( _header_is_newer ) {
            // Remove all old objects
            utils::fs_remove( _s.obj_path_ );
            utils::make_dir( _s.obj_path_ );
        }

        std::vector< std::string > _objs;
        const char *_flag = ( cxxflags.size() > 0 ? cxxflags.c_str() : NULL );
        for ( const auto& s : _web_sources ) {
            std::string _o = startupmgr::object_dir() + utils::md5(s) + OBJ_EXT;
            if ( ! startupmgr::compile_source(s, _o, _flag) ) {
                return false;
            }
            _objs.emplace_back(std::move(_o));
        }

        if ( ! startupmgr::create_library( _objs, _s.lib_path_ ) ) {
            return false;
        }
    }

    // Load and initialize app
    _s.hstartup_ = dlopen(_s.lib_path_.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if ( !_s.hstartup_ ) {
        std::cerr << "failed to load the startup module" << std::endl;
        return false;
    }
    dlerror();
    startup_initialize _initf = (startup_initialize)dlsym(_s.hstartup_, "dhboc_initialize");
    const char* _sym_error = dlerror();
    if ( _sym_error ) {
        std::cerr << "invalidate startup module: " << _sym_error << std::endl;
        return false;
    }

    app.domain = "";
    app.address = "0.0.0.0:8883";
    app.workers = 2;
    app.router.clear();
    app.code.clear();
    app.webroot = startupmgr::webroot_dir();
    app.runtime = startupmgr::runtime_dir();
    app.exclude_path.clear();
    app.pre_includes.clear();
    app.ctnt_exts.clear();
    _initf();
    dlclose(_s.hstartup_);
    _s.hstartup_ = NULL;

    return true;
}

startupmgr::~startupmgr() {
    // Close the startup module
    if ( hstartup_ ) dlclose(hstartup_);
}

// Initialize the worker process
bool startupmgr::worker_initialization( int index ) {
    // Get the singleton object
    startupmgr& _s = startupmgr::_s_();

    _s.hstartup_ = dlopen(_s.lib_path_.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    _s.hrequest_ = (pre_request_t)dlsym(_s.hstartup_, "dhboc_pre_request");
    _s.hresponse_ = (final_response_t)dlsym(_s.hstartup_, "dhboc_final_response");

    // Try to find startup
    worker_init_t _init = (worker_init_t)dlsym(_s.hstartup_, "dhboc_worker_startup");
    if ( !_init ) return false;
    _init( index );

    return true;
}

// Pre request handler
http::CODE startupmgr::pre_request( http_request& req ) {
    if ( startupmgr::_s_().hrequest_ ) {
        return startupmgr::_s_().hrequest_(req);
    }
    return http::CODE_000;
}

// Final Response handler
void startupmgr::final_response( http_request& req, http_response& resp ) {
    if ( startupmgr::_s_().hresponse_ ) {
        startupmgr::_s_().hresponse_(req, resp);
    }
}

// Search for handler with specified name
http_handler startupmgr::search_handler( const std::string& hname ) {
    // Get the singleton object
    startupmgr& _s = startupmgr::_s_();
    if ( _s.hstartup_ == NULL ) return NULL;
    http_handler _h = (http_handler)dlsym(_s.hstartup_, hname.c_str());
    if ( _h == NULL ) return NULL;
    return _h;
}

// Singleton
startupmgr::startupmgr() :
    hstartup_(NULL), hrequest_(NULL), hresponse_(NULL) 
{ }

startupmgr& startupmgr::_s_() {
    static startupmgr _s; return _s;
}

// Push Chen
