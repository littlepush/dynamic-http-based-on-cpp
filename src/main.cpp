/*
    main.cpp
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2019-10-27
    Push Chen

    Copyright 2015-2019 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include <peco/peco.h>
using namespace pe;
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

#include "dhboc.h"
#include "modules.h"
#include "startup.h"
#include "handler.h"

void server_worker( net::http_request& req ) {
    if ( utils::is_string_end(req.path(), "/") ) {
        req.path() += "index.html";
    }
    this_task::begin_tick();
    http_response _resp;
    try {
        auto _pcode = startupmgr::pre_request(req);
        if ( _pcode == http::CODE_000 ) {
            // route to find the content
            if ( ! content_handlers::try_find_handler(req, _resp) ) {
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
                _resp.load_file(app.webroot + app.code[_pcode]);
            }
            // force to set the response code
            _resp.status_code = _pcode;
        }

        // Do something before send response
        startupmgr::final_response(req, _resp);
    } catch ( ... ) {
        // Internal server error
        _resp.status_code = CODE_500;
        if ( app.code.find(CODE_500) != app.code.end() ) {
            _resp.load_file(app.webroot + app.code[CODE_500]);
        }
    }

    net::http_server::send_response(_resp);
    auto _time_used = this_task::tick();

    rlog::info << req.path() << " (" << _time_used << "ms)" << std::endl;
}

bool _dhboc_forever( const std::string& startup, bool force_rebuild ) {
    std::map< pid_t, int >      _children_pmap;
    // Reload startup manager
    if ( ! startupmgr::load_startup_file(startup, force_rebuild) ) {
        std::cout << "load startup file failed" << std::endl;
        return false;
    }

    // Rescan webroot 
    if ( ! content_handlers::scan_webroot() ) {
        std::cout << "scan webroot failed" << std::endl;
        return false;
    }

    // Try to build the library for all handlers
    if ( ! content_handlers::build_handler_lib() ) {
        std::cout << "build handlers failed" << std::endl;
        return false;
    }

    if ( app.workers <= 0 || app.workers > MAX_WORKERS ) app.workers = 1;
    // Create the listening port
    net::peer_t _li(app.address);
    net::SOCKET_T _lso = net::rawf::createSocket(ST_TCP_TYPE);
    if ( SOCKET_NOT_VALIDATE(_lso) ) {
        return false;
    }
    struct sockaddr_in _sock_addr = _li;
    if ( ::bind(_lso, (struct sockaddr *)&_sock_addr, sizeof(_sock_addr)) == -1 ) {
        std::cerr << "Failed to bind tcp socket on: " << app.address
            << ", " << ::strerror( errno ) << std::endl;
        ::close(_lso);
        return false;
    }

    // Create workers
    for ( int i = 0; i < app.workers; ++i ) {
        // Create pipe to child
        int _c2p_out[2];
        ignore_result(pipe(_c2p_out));
        pid_t _pid = fork();
        if ( _pid < 0 ) {
            std::cerr << "failed to fork child process." << std::endl;
            ::close(_c2p_out[0]); ::close(_c2p_out[1]);
            return false;
        }
        if ( _pid > 0 ) {   // Success on create new worker
            parent_pipe_for_read(_c2p_out);
            // Store the read pipe
            _children_pmap[_pid] = _c2p_out[PIPE_READ_FD];
            continue;
        }
        // Bind self's out pipe to stdout, so when
        // the child exit, the pipe will also been closed.
        child_dup_for_read(_c2p_out, STDOUT_FILENO);

        loop::main.do_job([&]() {
            startupmgr::worker_initialization(i);
            content_handlers::load_handlers();

            loop::main.do_job(_lso, []() {
                net::http_server::listen( &server_worker );
            });
        });
        loop::main.run();
        return true;  // Im a child process, no need to fork again.
    }

    // Wait for all children to exit
    std::map< pid_t, task * > _pipe_cache;
    for ( auto& cp : _children_pmap ) {
        _pipe_cache[cp.first] = loop::main.do_job(cp.second, []() {
            while ( true ) {
                auto _sig = this_task::wait_for_event(
                    event_read, std::chrono::milliseconds(1000)
                );
                if ( _sig == no_signal ) continue;
                if ( _sig == bad_signal ) break;
                std::string _buf = std::forward< std::string >(pipe_read(this_task::get_id()));
                // Pipe closed
                if ( _buf.size() == 0 ) break;
                std::cout << _buf;
            }
        });
    }

    // Check if content has been changed
    loop::main.do_loop([&]() {
        if ( !content_handlers::content_changed() ) return;
        // Auto quit
        std::cout << "content changed, send kill signal" << std::endl;
        for ( auto& c : _children_pmap ) {
            std::cout << "kill subprocess: " << c.first << std::endl;
            task_exit(_pipe_cache[c.first]);
            kill( c.first, SIGTERM );
            int _sig;
            ignore_result(wait(&_sig));
        }
        this_task::cancel_loop();
    }, std::chrono::seconds(1));

    loop::main.run();

    // Close the listening socket
    ::close(_lso);
    return true;
}

int main( int argc, char* argv[] ) {
    // Load Config file first
    std::string _config_path;
    bool _force_rebuild = false;
    bool _forever = false;
    utils::argparser::set_parser("modules", "m", _config_path);
    utils::argparser::set_parser("rebuild", "r", [&_force_rebuild](std::string&&) {
        _force_rebuild = true;
    });
    utils::argparser::set_parser("forever", "f", [&_forever](std::string&&) {
        _forever = true;
    });
    utils::argparser::set_parser("version", "v", [](std::string&&) {
        std::cout << "DHBoC server, version: " << VERSION << std::endl;
        std::cout << "Copyright 2015-2019 Push Lab. All rights reserved." << std::endl;
        std::cout << "Powered by Push Chen <littlepush@gmail.com>." << std::endl;
        exit(0);
    });
    utils::argparser::set_parser("help", "h", [](std::string&&) {

        exit(0);
    });

    // Check the startup file
    if ( !utils::argparser::parse(argc, argv) ) return 1;
    std::string _startup = "./main.cpp";
    auto _iargs = utils::argparser::individual_args();
    utils::argparser::clear();

    if ( _iargs.size() != 0 ) {
        _startup = _iargs[0];
    }
    if ( !utils::is_file_existed(_startup) ) {
        std::cerr << "startup file not existed: " << _startup << std::endl;
        return 2;
    }

    // Try to format the startup
    _startup = utils::dirname(_startup) + utils::full_filename(_startup);
    content_handlers::ignore_file(_startup);
    if ( _config_path.size() > 0 ) {
        _config_path = utils::dirname(_config_path) + utils::full_filename(_config_path);
        content_handlers::ignore_file(_config_path);
    }

    // load all modules
    if ( _config_path.size() > 0 ) {
        // Try to load the config with all modules
        utils::argparser::set_parser("module", [](std::string&& module_name) {
            modulemgr::load_module(module_name);
        });
        utils::argparser::parse(_config_path);
        utils::argparser::clear();
    }

    bool _last_success = true;
    do {
        if ( !_last_success ) {
            // If last run is not success, wait for 10 seconds
            sleep(10);
        }
        _last_success = _dhboc_forever(_startup, _force_rebuild);
        _force_rebuild = false;
    } while ( _forever );

    return 0;
}