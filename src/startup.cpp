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
const std::string EXT_NAME(".dylib");
std::string CC("clang++");
const std::string INC_ROOT("/usr/local/include/pe");
const std::string EX_DEFINES("-I/usr/local/opt/openssl/include/");
const std::string EX_FLAGS("-L/usr/local/opt/openssl/lib");
#else
const std::string EXT_NAME(".so");
std::string CC("g++");
const std::string INC_ROOT("/usr/include/pe");
const std::string EX_DEFINES("");
const std::string EX_FLAGS("-L/usr/lib64");
#endif

struct __module_block {
    std::string                 module_name;
    std::string                 variable_name;
    std::string                 arg_list;
    // std::vector< std::string >  arg_list;
    bool                        is_global;

    // Shift size of the original code
    size_t                      begin_pos;
    size_t                      block_length;
};

__module_block __module_block_search( std::string& code, size_t begin_pos = 0 ) {
    __module_block _mb;
    _mb.begin_pos = std::string::npos;
    _mb.block_length = std::string::npos;
    // Search for the `require` keyword
    size_t _req_pos = code.find("dhboc::require(", begin_pos);
    // If no block
    if ( _req_pos == std::string::npos ) return _mb;
    int _lv = 1;
    size_t _eob = _req_pos + 15;
    for ( ; _eob < code.size(); ++_eob ) {
        if ( code[_eob] == '(' ) _lv += 1;
        if ( code[_eob] == ')' ) _lv -= 1;
        if ( _lv == 0 ) break;
    }

    while ( code[_eob] != ';' && _eob < code.size() ) ++_eob;
    size_t _bob = _req_pos - 1;
    while ( _bob >= begin_pos && code[_bob] != '{' && code[_bob] != ';' ) --_bob;
    _bob += 1;
    _mb.begin_pos = _bob;
    _mb.block_length = _eob - _bob + 1;

    // Copy the block code
    std::string _code_block = code.substr(_bob, _mb.block_length);
    utils::trim(_code_block);
    // Remove last ';' & ')'
    if ( *_code_block.rbegin() == ';' ) _code_block.pop_back();
    utils::trim(_code_block);
    if ( *_code_block.rbegin() == ')' ) _code_block.pop_back();
    utils::trim(_code_block);

    if ( utils::is_string_start(_code_block, "global") ) {
        _mb.is_global = true;
        _code_block.erase(0, 6);    // Erase `global`
        utils::trim(_code_block);
    }
    size_t _eq = _code_block.find('=');
    _mb.variable_name = _code_block.substr(0, _eq);
    utils::trim( _mb.variable_name );

    // Skip dhboc::require(
    size_t _l = _code_block.find('(', _eq);
    _code_block.erase(0, _l + 1);
    size_t _c = _code_block.find(',');
    if ( _c == std::string::npos ) {
        // No arg_list
        _mb.module_name = std::move(utils::trim(_code_block));
        if ( _mb.module_name[0] == '"' ) _mb.module_name.erase(0, 1);
        if ( *_mb.module_name.rbegin() == '"') _mb.module_name.pop_back();
        return _mb;
    }
    _mb.module_name = _code_block.substr(0, _c);
    utils::trim(_mb.module_name);
    if ( _mb.module_name[0] == '\"' ) _mb.module_name.erase(0, 1);
    if ( *_mb.module_name.rbegin() == '\"') _mb.module_name.pop_back();
    _code_block.erase(0, _c + 1);
    utils::trim(_code_block);
    _mb.arg_list = std::move(_code_block);
    return _mb;
}

bool __compile_startup( 
    const std::string& buildpath, 
    const std::string& inputfile, 
    const std::string& outputfile 
) {

    std::string _srcpath = buildpath + "src/";
    if ( !utils::rek_make_dir(_srcpath) ) {
        std::cerr << "failed to create temp source folder: " << _srcpath << std::endl;
        return false;
    }
    std::string _objpath = buildpath + "obj/";
    if ( !utils::rek_make_dir(_objpath) ) {
        std::cerr << "failed to create obj output folder: " << _objpath << std::endl;
        return false;
    }

    // Load Startup file
    std::ifstream _fs(inputfile);
    std::string _fcontent(
        (std::istreambuf_iterator<char>(_fs)), 
        (std::istreambuf_iterator<char>())
    );
    if ( _fcontent.find("namespace ") == std::string::npos ) {
        std::cerr << "no any namespace in the startup file." << std::endl;
        return false;
    }
    std::map< std::string, std::string > _code_map;

    auto _fparts = utils::split(_fcontent, std::vector<std::string>{"namespace"});
    for ( std::string& _code_part : _fparts ) {
        utils::code_filter_comment(_code_part);
        utils::trim(_code_part);
        if ( _code_part.size() == 0 ) continue;
        size_t _b = _code_part.find("{");
        if ( _b == std::string::npos ) {
            std::cout << "invalidate namespace code part" << std::endl;
            return false;
        }
        std::string _namespace = _code_part.substr(0, _b);
        utils::trim(_namespace);
        _code_part.erase(0, _b);
        _code_map.emplace( std::make_pair(std::move(_namespace), std::move(_code_part)) );
    }

    std::string _src = _srcpath + utils::filename(inputfile) + ".cpp";
    std::string _inc = _srcpath + utils::filename(inputfile) + ".h";
    std::ofstream _fmain(_src);
    std::ofstream _finc(_inc);

    // dump default header
    _fmain << "#include <application.h>" << std::endl;
    _fmain << "extern \"C\" {" << std::endl;

    // Dump Setting Code
    _fmain << "void __initialize( )" << std::endl;
    auto _it_setting = _code_map.find("setting");
    if ( _it_setting == _code_map.end() ) { _fmain << "{}" << std::endl; }
    else { _fmain << _it_setting->second << std::endl; }

    // Expand startup
    _finc << "#pragma once" << std::endl;
    std::string _k = utils::md5(inputfile);
    _finc << "#ifndef dhboc_startup_include_" << _k << std::endl;
    _finc << "#define dhboc_startup_include_" << _k << std::endl;
    _finc << "#include <application.h>" << std::endl;

    // Search the startup code to find any `global` keywords
    auto _it_startup = _code_map.find("startup");
    if ( _it_startup != _code_map.end() ) {
        while ( true ) {
            auto _mb = __module_block_search(_it_startup->second, 0);
            if ( _mb.begin_pos == std::string::npos ) break;
            // Search for the module name
            std::string _type = modulemgr::require_type(_mb.module_name);
            if ( _mb.is_global == true ) {
                _finc << "extern std::shared_ptr<" << _type 
                    << "> " << _mb.variable_name << ";" << std::endl;
                _fmain << "std::shared_ptr<" << _type
                    << "> " << _mb.variable_name << ";" << std::endl;
            }
            // Replace the code
            _it_startup->second.erase(_mb.begin_pos, _mb.block_length);
            std::stringstream _fcode;
            if ( !_mb.is_global ) { _fcode << "std::shared_ptr<" << _type << "> "; }
            _fcode << _mb.variable_name << " = std::make_shared<" << _type << ">("
                << _mb.arg_list << ");" << std::endl;
            _it_startup->second.insert(_mb.begin_pos, _fcode.str());
        }
    }
    // Go on main startup
    _fmain << "void __startup( )" << std::endl;
    if ( _it_startup == _code_map.end() ) { _fmain << "{}" << std::endl; }
    else { _fmain << _it_startup->second << std::endl; }

    _finc << "#endif" << std::endl;

    // Dump request
    auto _it_req = _code_map.find("request");
    if ( _it_req != _code_map.end() ) {
        _fmain << "http::CODE __pre_request(http_request& req)" << std::endl;
        _fmain << _it_req->second << std::endl;
    }

    // Dump response
    auto _it_resp = _code_map.find("response");
    if ( _it_resp != _code_map.end() ) {
        _fmain << "void __final_response(http_request& req, http_response& resp)" << std::endl;
        _fmain << _it_resp->second << std::endl;
    }

    // Finish
    _fmain << "}" << std::endl;
    _fmain.close();

    // Build compile and link flags
    std::string _obj = _objpath + utils::filename(inputfile) + ".o";
    std::vector< std::string > _compileflags{
        CC, 
        "--std=c++11",
        "-pthread",
        "-Werror", 
        "-Wall",
        "-fPIC",
        "-O3",
        "-DRELEASE=1",
        EX_DEFINES,
        "-I" + INC_ROOT + "/utils",
        "-I" + INC_ROOT + "/cotask",
        "-I" + INC_ROOT + "/conet",
        "-I" + INC_ROOT + "/dhboc",
        "-c",
        _src,
        "-o",
        _obj
    };
    std::string _compile_cmd = utils::join(_compileflags.begin(), _compileflags.end(), " ");
    if ( 0 != system(_compile_cmd.c_str()) ) return false;
    std::vector< std::string > _linkflags{
        CC,
        "-shared",
        "-o",
        outputfile,
        _obj,
        EX_FLAGS,
        "-lcotask",
        "-lssl",
        "-lresolv",
        "-lpeutils",
        "-lconet",
        "-ldhboc"
    };
    std::string _link_cmd = utils::join(_linkflags.begin(), _linkflags.end(), " ");
    return ( 0 == system(_link_cmd.c_str()));
}

// // manager of the startup module
startupmgr::startupmgr( const std::string& startup_file )
    : done_(false), hstartup_(NULL), hrequest_(NULL), hresponse_(NULL), startup_(startup_file), 
    webroot(webroot_), runtime(runtime_), buildpath(buildpath_), libpath(libpath_)
{
    webroot_ = utils::dirname(startup_file);
    if ( *(webroot_.rbegin()) != '/' ) webroot_ += '/';
    runtime_ = webroot_ + ".runtime/";
    if ( !utils::rek_make_dir(runtime_) ) {
        std::cerr << "failed to make runtime folder: " << runtime_ << std::endl;
        return;
    }
    buildpath_ = webroot_ + ".build/";
    if ( !utils::rek_make_dir(buildpath_) ) {
        std::cerr << "failed to make build folder: " << buildpath_ << std::endl;
        return;
    }
    std::string _libname = (
        utils::filename(startup_file) + '.' + 
        std::to_string((long)utils::file_update_time(startup_file)) + 
        EXT_NAME
        );

    libpath_ = runtime_ + _libname;
    if ( !utils::is_file_existed(libpath_) ) {
        // Compile it
        if ( !__compile_startup( buildpath_, startup_file, libpath_ ) ) {
            std::cerr << "failed to compile the startup module" << std::endl;
            return;
        }
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
        std::cerr << "invalidate startup module" << std::endl;
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
    if ( !hstartup_ ) dlclose(hstartup_);
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

// Push Chen
