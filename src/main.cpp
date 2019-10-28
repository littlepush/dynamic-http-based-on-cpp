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

bool g_isChild = false;
std::string g_webroot;
std::map< pid_t, bool > g_children_pmap;

void server_worker( net::http_request& req ) {
    #ifdef DEBUG
    std::cout << req.method() << " " << req.path() << std::endl;
    #endif
    net::http_response _resp;
    net::http_server::send_response(_resp);
}

int main( int argc, char* argv[] ) {
    // Load Config file first
    std::string _config_path;
    utils::argparser::set_parser("config", "f", _config_path);
    utils::argparser::set_parser("version", "v", [](std::string&& arg) {
        std::cout << "DHBoC server, version: " << VERSION << std::endl;
        std::cout << "Copyright 2015-2019 Push Lab. All rights reserved." << std::endl;
        std::cout << "Powered by Push Chen <littlepush@gmail.com>." << std::endl;
        exit(0);
    });

    if ( !utils::argparser::parse(argc, argv) ) return 1;
    if ( _config_path.size() == 0 ) {
        std::cerr << "missing config path" << std::endl;
        return 1;
    }
    utils::argparser::clear();

    // Try to parse the configfile
    std::string _bind_info = "0.0.0.0:8883";
    std::string _worker_count = "1";
    utils::argparser::set_parser("listen", _bind_info);
    utils::argparser::set_parser("webroot", g_webroot);
    utils::argparser::set_parser("workers", _worker_count);

    if ( !utils::argparser::parse(_config_path) ) return 1;

    if ( g_webroot.size() == 0 ) {
        std::cerr << "the root folder of the website cannot be empty." << std::endl;
        return 1;
    }

    // At least 1 worker, which is self.
    int _workerCount = std::stoi(_worker_count);
    if ( _workerCount == 0 ) _workerCount = 1;

    // Create the listening port
    net::peer_t _li(_bind_info);
    net::SOCKET_T _lso = net::tcp::create( _li );

    for ( int i = 0; i < _workerCount; ++i ) {
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
        // To do:
        // Scan the web root folder and find all `dhc` file
        // Find a specifial file named `router.dhc`

        while ( g_children_pmap.size() > 0 ) {
            int _cestatus;
            pid_t _p = wait(&_cestatus);
            #ifdef DEBUG
            std::cout << "worker " << _p << " exited: " << _cestatus << std::endl;
            #endif
            g_children_pmap.erase(_p);
        }
    } else {
        loop::main.do_job(_lso, []() {
            net::http_server::listen( &server_worker );
        });
        loop::main.run();
    }

    return 0;
}