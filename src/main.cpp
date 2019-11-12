/*
    main.cpp
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2019-10-27
    Push Chen

    Copyright 2015-2019 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include <peutils.h>
using namespace pe;

#include <cotask.h>
#include <conet.h>
using namespace pe::co;

#ifndef VERSION
#define VERSION     "1.0.alpha"
#endif

#ifndef MAX_WORKERS
#define MAX_WORKERS 20
#endif

#if PZC_TARGET_LINUX
#include <sys/wait.h>
#endif

#include "application.h"
#include "modules.h"
#include "startup.h"

bool g_isChild = false;
std::string g_webroot;
std::map< pid_t, bool > g_children_pmap;
startupmgr *g_startup = NULL;
content_handlers *g_ch = NULL;

void server_worker( net::http_request& req ) {
    if ( utils::is_string_end(req.path(), "/") ) {
        req.path() += "index.html";
    }
    #ifdef DEBUG
    std::cout << req.method() << " " << req.path() << std::endl;
    #endif
    http_response _resp;
    auto _pcode = g_startup->pre_request(req);
    if ( _pcode == http::CODE_000 ) {
        // route to find the content
        if ( !g_ch->try_find_handler(req, _resp) ) {
            // else _resp.load_file(req.path());
            _resp.load_file(app.webroot + req.path());
        }
        if ( _resp.status_code != CODE_200 ) {
            _pcode = _resp.status_code;
        }
    }
    if ( _pcode != http::CODE_000 ) {
        // Load specifial file as the resposne body
        if ( app.code.find(_pcode) != app.code.end() ) {
            #ifdef DEBUG
            std::cout << "pre-request return " << _pcode << ", and has return page: "
                << app.webroot + app.code[_pcode] << std::endl;
            #endif
            _resp.load_file(app.webroot + app.code[_pcode]);
        }
        // force to set the response code
        _resp.status_code = _pcode;
    }

    // Do something before send response
    g_startup->final_response(req, _resp);
    net::http_server::send_response(_resp);
}

int main( int argc, char* argv[] ) {
    // Load Config file first
    std::string _config_path;
    bool _force_rebuild = false; bool* _pfrb = &_force_rebuild;
    utils::argparser::set_parser("modules", "m", _config_path);
    utils::argparser::set_parser("rebuild", "r", [_pfrb](std::string&& arg) {
        *_pfrb = true;
    });
    utils::argparser::set_parser("version", "v", [](std::string&& arg) {
        std::cout << "DHBoC server, version: " << VERSION << std::endl;
        std::cout << "Copyright 2015-2019 Push Lab. All rights reserved." << std::endl;
        std::cout << "Powered by Push Chen <littlepush@gmail.com>." << std::endl;
        exit(0);
    });

    if ( !utils::argparser::parse(argc, argv) ) return 1;
    std::string _startup = "./main.cpp";
    auto _iargs = utils::argparser::individual_args();
    if ( _iargs.size() != 0 ) {
        _startup = _iargs[0];
    }
    if ( !utils::is_file_existed(_startup) ) {
        std::cerr << "startup file not existed: " << _startup << std::endl;
        return 2;
    }
    // Try to format the startup
    _startup = utils::dirname(_startup) + utils::full_filename(_startup);
    utils::argparser::clear();

    // load all modules
    if ( _config_path.size() > 0 ) {
        // Try to load the config with all modules
        utils::argparser::set_parser("module", [](std::string&& module_name) {
            modulemgr::load_module(module_name);
        });
        utils::argparser::parse(_config_path);
    }

    startupmgr _smgr(_startup, _force_rebuild);
    if ( !_smgr ) return 4; // failed to start
    g_startup = &_smgr;

    // Scan all files
    std::string _workroot = utils::dirname(_startup);
    // Format exclude path
    for ( auto& _ep : app.exclude_path ) {
        if ( utils::is_string_start(_ep, "./") ) _ep.erase(0, 2);
        while ( _ep[0] == '/' ) _ep.erase(0, 1);
        if ( *_ep.rbegin() == '/' ) _ep.pop_back();
        _ep = (_workroot + _ep);
    }

    content_handlers _ch(&_smgr);
    g_ch = &_ch;
    utils::rek_scan_dir(
        _workroot, 
        [&_ch, _startup](const std::string& path, bool is_dir) -> bool {
            if ( path == _startup ) return false;
            // Ignore object file
            if ( utils::extension(path) == "o" ) return false;
            // Default to omit any file/folder begin with '_'
            if ( utils::filename(path)[0] == '_' ) return false;
            // Check if the path is in ignore list
            for ( const auto& _ep : app.exclude_path ) {
                // Same path, ignore all 
                if ( _ep == path ) return false;
            }
            if ( !is_dir ) {
                // Add to compile list
                if ( !_ch.format_source_code(path) ) { exit(10); }
            }
            return true;
        }
    );
    // Try to build the library for all handlers
    if ( !_ch.build_handler_lib() ) return 99;

    if ( app.workers <= 0 || app.workers > MAX_WORKERS ) app.workers = 1;
    // Create the listening port
    net::peer_t _li(app.address);
    net::SOCKET_T _lso = net::tcp::create( _li );

    for ( int i = 0; i < app.workers; ++i ) {
        pid_t _pid = fork();
        if ( _pid < 0 ) {
            std::cerr << "failed to fork child process." << std::endl;
            return 1;
        }
        if ( _pid > 0 ) {   // Success on create new worker
            g_children_pmap[_pid] = true;
            continue;
        }
        g_isChild = true;
        break;  // Im a child process, no need to fork again.
    }

    if ( !g_isChild ) {
        while ( g_children_pmap.size() > 0 ) {
            int _cestatus;
            pid_t _p = wait(&_cestatus);
            #ifdef DEBUG
            std::cout << "worker " << _p << " exited: " << _cestatus << std::endl;
            #endif
            g_children_pmap.erase(_p);
        }
    } else {
        _smgr.worker_initialization();
        _ch.load_handlers();
        loop::main.do_job(_lso, []() {
            net::http_server::listen( &server_worker );
        });
        loop::main.run();
    }

    return 0;
}