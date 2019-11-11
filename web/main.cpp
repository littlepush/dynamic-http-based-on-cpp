/*
Keywords:
app : current application object, can set everything in this object, and it will be sent to all workers
req : current request
resp : current response
cookie : current request's cookie object, and will be write back to response
session : this session, based on cookie's session id
set : make a global variable, all code will share this variable
*/

/*
The application object contains the following keys
domain : empty domain means any host name can be accpeted
address : the binding listen address, default is 0.0.0.0:8883
workers : count of worker child process
index : index path of current site
router : structure, { METHOD, Path Parser, Handler }
code : code[x], a map with status_code and handler
handlers : 
*/

/*
    require( MODULE_NAME, Args... );
    any module: 
        module_name
        header_files
        type

    => translate to
    std::shared_ptr<type> gredis = std::make_shared<type>( new type(...) );
    then the `gredis` can be used as a normal object
*/

/*
    a global variable will be put in the global code space.
    and will also create a header file with `extern` keyword to tell all other
    modules where this variable is.
*/
extern "C" {

#include "_server/main.h"

// Global redis group handler
std::shared_ptr< net::redis::group > gredis;

void __initialize() {
    app.code[404] = "404.html";
    app.code[500] = "500.html";
    std::cout << "this is in the main.dhc file and output by it self" << std::endl;
    app.address = "127.0.0.1:8884";
    std::cout << "lalalalala" << std::endl;
    app.workers = 3;
    app.pre_includes.push_back("_server/main.h");
}

void __startup() {
    gredis = std::make_shared<pe::co::net::redis::group>(
        net::peer_t("192.168.71.130:6379"), 
        "locald1@redis", 
        2
    );
    std::cout << "this is module startup, should be run in child process: " << getpid() << std::endl;
}

http::CODE __pre_request(http_request& req) {
    std::string _path = req.path();
    if ( _path == "a" ) return CODE_000;
    if ( _path == "b" ) return CODE_000;
    if ( !req.header.contains("Access-Token") ) return CODE_401;
    std::string _token = req.header["Access-Token"];
    auto _sessionId = gredis->query("GET", _token);
    if ( ! net::redis::result_check(_sessionId, {0, 1, {}, {}}) ) {
        std::cerr << "No session ID bind to the token, expired" << std::endl;
        return CODE_401;
    }
    // Todo: add session or something
    return CODE_000;
}

void __final_response( http_request& req, http_response& resp ) {
    // Do anything before the response has been sent to the client
}

}