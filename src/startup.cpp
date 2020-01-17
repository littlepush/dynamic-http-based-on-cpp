/*
    startup.cpp
    PECoTask
    2019-10-31
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)

    Copyright 2015-2018 MeetU Infomation and Technology Inc. All rights reserved.
*/
#include "startup.h"
#include <dlfcn.h>

application_t app;

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
const std::string OBJ_EXT(".debug.o");
const std::string CC_DEFINES("-DDEBUG=1 -Og");
#else
const std::string OBJ_EXT(".o");
const std::string CC_DEFINES("-DRELEASE=1 -O3");
#endif

// Compile source code, specified the input source file and the output obj path
bool startupmgr::compile_source(const std::string& src, const std::string& obj, const char* ex) {
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
        "-include dhboc/application.h"
    };
    // Include all files
    for ( const auto& i : modulemgr::include_files() ) {
        _cflags.push_back(std::string("-include ") + i);
    }
    for ( const auto& i : app.pre_includes ) {
        _cflags.push_back(std::string("-include ") + webroot_ + i);
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
    _linkflags.push_back("-lz -lssl -lcrypto -lresolv -lpeco -ldhboc");
    if ( ex != NULL ) _linkflags.push_back(std::string(ex));

    std::string _link_cmd = utils::join(_linkflags.begin(), _linkflags.end(), " ");

#ifdef DEBUG
    std::cout << "link cmd: " << _link_cmd << std::endl;
#endif

    return (0 == system(_link_cmd.c_str()));
}

// // manager of the startup module
startupmgr::startupmgr( const std::string& startup_file, bool force_rebuild )
    : done_(false), hstartup_(NULL), hrequest_(NULL), hresponse_(NULL), startup_(startup_file), 
    webroot(webroot_), runtime(runtime_), serverpath(serverpath_), libpath(libpath_)
{
    webroot_ = utils::dirname(startup_file);
    if ( *(webroot_.rbegin()) != '/' ) webroot_ += '/';
    runtime_ = webroot_ + ".runtime/";
    if ( force_rebuild ) { utils::fs_remove(runtime_); }
    if ( !utils::rek_make_dir(runtime_) ) {
        std::cerr << "failed to make runtime folder: " << runtime_ << std::endl;
        return;
    }
    // Make pieces path
    utils::make_dir(runtime_ + "pieces");

    std::string _libname = (
        utils::filename(startup_file) + '.' + 
        std::to_string((long)utils::file_update_time(startup_file)) + 
        EXT_NAME
        );

    libpath_ = runtime_ + _libname;
    if ( !utils::is_file_existed(libpath_) ) {
        // Compile it
        // Try to find all source code files
        std::vector<std::string> _src_files;
        _src_files.push_back(startup_file);
        std::string _server_path = webroot_ + "_server";
        if ( utils::is_folder_existed(_server_path) ) {
            utils::rek_scan_dir(
                _server_path, 
                [&_src_files](const std::string& path, bool is_dir) {
                    if ( is_dir ) return true;
                    std::string _ext = utils::extension(path);
                    if ( _ext == "cpp" || _ext == "c" ) {
                        _src_files.push_back(path);
                    }
                    return true;
                }
            );
        }
        std::vector< std::string > _objs;
        for ( const auto& sp : _src_files ) {
            std::string _obj = utils::dirname(sp) + utils::filename(sp) + OBJ_EXT;
            if ( ! this->compile_source(sp, _obj) ) return;
            _objs.emplace_back(std::move(_obj));
        }
        if ( ! this->create_library(_objs, libpath_) ) return;
    }
    // Load the startup module
    hstartup_ = dlopen(libpath_.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if ( !hstartup_ ) {
        std::cerr << "failed to load the startup module" << std::endl;
        return;
    }
    dlerror();
    startup_initialize _initf = (startup_initialize)dlsym(hstartup_, "__initialize");
    const char* _sym_error = dlerror();
    if ( _sym_error ) {
        std::cerr << "invalidate startup module: " << _sym_error << std::endl;
        return;
    }

    app.address = "0.0.0.0:8883";
    app.workers = 2;
    app.webroot = webroot_;
    app.runtime = runtime_;
    _initf();
    dlclose(hstartup_);
    hstartup_ = NULL;
    done_ = true;
}
startupmgr::~startupmgr() {
    // Close the startup module
    if ( hstartup_ ) dlclose(hstartup_);
}

// Check if the startup module has been loaded
startupmgr::operator bool () const { return done_; }

// Initialize the worker process
bool startupmgr::worker_initialization() {
    if ( !done_ ) return false;
    hstartup_ = dlopen(libpath_.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    hrequest_ = (pre_request_t)dlsym(hstartup_, "__pre_request");
    hresponse_ = (final_response_t)dlsym(hstartup_, "__final_response");

    // Try to find startup
    worker_init_t _init = (worker_init_t)dlsym(hstartup_, "__startup");
    if ( !_init ) return false;
    _init();

    return true;
}

// Pre request handler
http::CODE startupmgr::pre_request( http_request& req ) {
    if ( hrequest_ ) return hrequest_(req);
    return http::CODE_000;
}

// Final Response handler
void startupmgr::final_response( http_request& req, http_response& resp ) {
    if ( hresponse_ ) hresponse_(req, resp);
}

// Create a content handler with the startup manager
content_handlers::content_handlers(startupmgr * smgr) : smgr_(smgr) { }

// Format and compile source code
bool content_handlers::format_source_code( const std::string& origin_file ) {
    std::cout << "Process File: " << origin_file << std::endl;
    std::string _output = utils::dirname(origin_file) + "_" + utils::filename(origin_file) + ".cpp";
    std::string _path = origin_file;
    if ( utils::is_string_start(_path, app.webroot) ) {
        _path.erase(0, app.webroot.size() - 1);
    }

    time_t _origin_time = utils::file_update_time(origin_file);
    time_t _output_time = utils::file_update_time(_output);
    // Only re-process the output file when we have a newer origin file
    if ( _output_time < _origin_time ) {
        std::ofstream _ofs(_output);
        _ofs << "extern \"C\"{" << std::endl;
        _ofs << "void __" << utils::md5(_path)
            << "(const http_request& req, http_response& resp) {" << std::endl;
        _ofs << "    resp.body.is_chunked = true;" << std::endl;
        std::string _piece_prefix = utils::md5(_path);

        // Load code
        std::ifstream _ifs(origin_file);
        std::string _code(
            (std::istreambuf_iterator<char>(_ifs)), 
            (std::istreambuf_iterator<char>())
        );
        _ifs.close();

        size_t _le = 0;
        size_t _pindex = 0;
        while ( _le != std::string::npos && _le < _code.size() ) {
            size_t _bpos = _code.find("{@", _le);
            size_t _epos = _bpos;
            if ( _bpos != std::string::npos ) {
                _epos = _code.find("@}", _bpos);
                if ( _epos == std::string::npos ) {
                    std::cerr << "error code block, missing `@}`" << std::endl;
                    std::cerr << "failed to format file: " << origin_file << std::endl;
                    return false;
                }
            }
            // We do find some code block after some html code
            if ( _bpos > _le && _bpos != std::string::npos ) {
                std::string _html = _code.substr(_le, _bpos - _le);

                // Dump the html data to piece file
                std::string _piece_name = _piece_prefix + "_" + std::to_string(_pindex);
                _pindex += 1;
                std::string _piece_path = smgr_->runtime + "pieces/" + _piece_name;
                std::ofstream _pfs(_piece_path);
                _pfs << _html;
                _pfs.close();

                _ofs << "    resp.body.load_file(\"" + _piece_path + "\");" << std::endl;
            }
            // Till end of code, no more code block
            if ( _bpos == std::string::npos && _le < _code.size() ) {
                std::string _html = _code.substr(_le);
                // Dump the html data to piece file
                std::string _piece_name = _piece_prefix + "_" + std::to_string(_pindex);
                _pindex += 1;
                std::string _piece_path = smgr_->runtime + "pieces/" + _piece_name;
                std::ofstream _pfs(_piece_path);
                _pfs << _html;
                _pfs.close();

                _ofs << "    resp.body.load_file(\"" + _piece_path + "\");" << std::endl;
                break;
            }
            // Update _le
            _le = _epos;
            if ( _le != std::string::npos ) _le += 2;

            // Now we should copy the code
            _ofs << _code.substr(_bpos + 2, _epos - _bpos - 2) << std::endl;
        }

        _ofs << "}}" << std::endl;        
    }

    std::string _obj = utils::dirname(origin_file) + utils::filename(origin_file) + OBJ_EXT;
    if ( smgr_->compile_source(_output, _obj) ) {
        // Register the handler list
        handler_names_[_path] = "__" + utils::md5(_path);
        objs_.emplace_back(std::move(_obj));
        return true;
    }
    return false;
}

// Create handler lib
bool content_handlers::build_handler_lib( ) {
    hlibpath_ = smgr_->runtime + "handlers" + EXT_NAME;
    if ( objs_.size() > 0 ) {
        // Try to create the final lib
        return smgr_->create_library(objs_, hlibpath_, smgr_->libpath.c_str());
    }
    return true;
}

// Load the handler in worker
void content_handlers::load_handlers() {
    // Load handlers
    if ( utils::is_file_existed(hlibpath_) ) {
        #ifdef DEBUG
        std::cout << "in workers, try to load handler lib: " << hlibpath_ << std::endl;
        #endif
        mh_ = dlopen(hlibpath_.c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if ( !mh_ ) {
            std::cerr << dlerror() << std::endl;
            loop::main.exit(12);
        }
        dlerror();
        // Load all handlers
        for ( const auto& pn : handler_names_ ) {
            http_handler _h = (http_handler)dlsym(mh_, pn.second.c_str());
            if ( !_h ) {
                const char* _sym_error = dlerror();
                std::cerr << _sym_error << std::endl;
                if ( _sym_error ) throw std::string(_sym_error);
            }
            handlers_[pn.first] = _h;
        }
    }
}

// Try to search the handler and run
bool content_handlers::try_find_handler(const http_request& req, http_response& resp) {
    #ifdef DEBUG
    std::cout << "try to find handler for path: " << req.path() << std::endl;
    #endif
    auto _h = handlers_.find(req.path());
    if ( _h == handlers_.end() ) return false;
    #ifdef DEBUG
    std::cout << "do find the handler for path: " << req.path() << std::endl;
    #endif
    if ( _h->second == NULL ) {
        #ifdef DEBUG
        std::cout << "but the handler is NULL for path: " << req.path();
        std::cout << ", handler name should be: " << handler_names_[req.path()] << std::endl;
        #endif
        return false;
    }
    _h->second(req, resp);
    return true;
}

// Push Chen
