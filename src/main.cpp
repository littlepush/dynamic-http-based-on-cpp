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
#define VERSION     "1.1.0"
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
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

bool __static_cache__ = true;

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
                // For any static file, default to add a one day cache
                if ( __static_cache__ ) {
                    _resp.header["Cache-Control"] = "max-age=86400";
                }
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
        void * _stack[64];
        size_t _ssize = backtrace(_stack, 64);
        backtrace_symbols_fd(_stack, _ssize, STDERR_FILENO);
    }

    net::http_server::send_response(_resp);
    auto _time_used = this_task::tick();

    std::string _ip = rawf::socket_peerinfo(this_task::get_id()).ip.str();
    rlog::info << req.method() << " " << req.path() << ", " << _ip 
        << ", " << _time_used << "ms, pid: " << getpid() << std::endl;
}

bool _dhboc_forever( 
    const std::string& startup, 
    const std::string& cxxflags, 
    bool force_rebuild,
    bool monitor_mode
) {
    // Reload startup manager
    if ( ! startupmgr::load_startup_file(startup, cxxflags, force_rebuild) ) {
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

    if ( monitor_mode ) {
        std::map< pid_t, int >      _children_pmap;
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

            this_loop.do_job([&]() {
                startupmgr::worker_initialization(i);
                content_handlers::load_handlers();

                this_loop.do_job(_lso, []() {
                    net::http_server::listen( &server_worker );
                });
            });
            this_loop.run();
            return true;  // Im a child process, no need to fork again.
        }

        // Wait for all children to exit
        std::map< pid_t, task_t > _pipe_cache;
        for ( auto& cp : _children_pmap ) {
            pid_t _cpid = cp.first;
            _pipe_cache[cp.first] = this_loop.do_job(cp.second, [&, _cpid]() {
                while ( true ) {
                    auto _sig = this_task::wait_for_event(
                        event_read, std::chrono::milliseconds(1000)
                    );
                    if ( _sig == no_signal ) continue;
                    if ( _sig == bad_signal ) {
                        _pipe_cache[_cpid] = NULL;
                        break;
                    }
                    std::string _buf = std::forward< std::string >(pipe_read(this_task::get_id()));
                    // Pipe closed
                    if ( _buf.size() == 0 ) {
                        _pipe_cache[_cpid] = NULL;
                        break;
                    }
                    std::cout << _buf;
                }
            });
        }

        // Check if content has been changed
        this_loop.do_loop([&]() {
            bool _all_correct = true;
            if ( content_handlers::content_changed() ) _all_correct = false;
            for ( auto& cp : _pipe_cache ) {
                if ( cp.second == NULL ) {
                    _all_correct = false;
                    break;
                }
            }
            if ( _all_correct == true ) return;
            // Auto quit
            std::cout << "something has been changed, send kill signal" << std::endl;
            for ( auto& c : _children_pmap ) {
                std::cout << "kill subprocess: " << c.first << std::endl;
                task_exit(_pipe_cache[c.first]);
                kill( c.first, SIGTERM );
                int _sig;
                ignore_result(wait(&_sig));
            }
            this_task::cancel_loop();
        }, std::chrono::seconds(1));

        this_loop.run();

        // Close the listening socket
        ::close(_lso);
        return true;
    } else {
        this_loop.do_job([&]() {
            startupmgr::worker_initialization(0);
            content_handlers::load_handlers();

            this_loop.do_job(_lso, []() {
                net::http_server::listen( &server_worker );
            });
        });

        this_loop.run();
        // Don't need to close anything, the loop will automatically close it.
        // We quit, because some error or signal, which is not success
        return false;
    }
}

int main( int argc, char* argv[] ) {
    // Load Config file first
    std::string _config_path;
    bool _force_rebuild = false;
    bool _forever = false;
    std::string _cxxflags;
    utils::argparser::set_parser("modules", "m", _config_path);
    utils::argparser::set_parser("rebuild", "r", [&_force_rebuild](std::string&&) {
        _force_rebuild = true;
    });
    utils::argparser::set_parser("forever", "f", [&_forever](std::string&&) {
        _forever = true;
    });
    utils::argparser::set_parser("cxxflags", _cxxflags);
    utils::argparser::set_parser("enable-static-cache", [](std::string&&) {
        __static_cache__ = true;
    });
    utils::argparser::set_parser("disable-static-cache", [](std::string&&) {
        __static_cache__ = false;
    });
    utils::argparser::set_parser("version", "v", [](std::string&&) {
        std::cout << "DHBoC server, version: " << VERSION << std::endl;
        std::cout << "Copyright Push Chen @littlepush. All rights reserved." << std::endl;
        std::cout << "Powered by Push Chen <littlepush@gmail.com>." << std::endl;
        exit(0);
    });
    utils::argparser::set_parser("help", "h", [](std::string&&) {
        std::cout
            << "Usage:"
            << "  dhboc [startup file]" << std::endl
            << "  dhboc [-f] [-r] [-m=<module file>] [--cxxflags=...]" << std::endl
            << "  dhboc --[enable|disable]-static-cache" << std::endl
            << "  dhboc -v" << std::endl
            << "  dhboc -h" << std::endl
            << "Options: " << std::endl
            << "  --forever,-f        Fork child process and monitor on file change, " << std::endl
            << "                       auto restart. Default is single process mode." << std::endl
            << "  --rebuild,-r        Rebuild all file before startup." << std::endl
            << "  --modules,-m        External modules file path." << std::endl
            << "  --[enable/disable]-static-cache" << std::endl
            << "                      Enable or disable default static file cache." << std::endl
            << "                      Default is enabled." << std::endl
            << "  --help,-h           Display this message." << std::endl
            << "  --version,-v        Display version information." << std::endl
            << "Powered by Push Chen <littlepush@gmail.com>." << std::endl;

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

    // Go to the work path
    std::string _work_path = utils::dirname(_startup);
    ignore_result(chdir(_work_path.c_str()));

    // Try to format the startup
    _startup = "./" + utils::full_filename(_startup);
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

    // Set compile flags
    if ( _cxxflags.size() > 0 ) {
        content_handlers::set_compile_flag(_cxxflags);
    }

    bool _last_success = true;
    do {
        if ( !_last_success ) {
            // If last run is not success, wait for 3 seconds
            sleep(3);
        }
        _last_success = _dhboc_forever(_startup, _cxxflags, _force_rebuild, _forever);
        _force_rebuild = false;
    } while ( _forever );

    return 0;
}