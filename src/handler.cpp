/*
    handler.cpp
    Dynamic-Http-Based-On-Cpp
    2020-01-21
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)

    Copyright 2015-2020 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include "handler.h"
#include <dlfcn.h>
#include <regex>

static std::map< std::string, bool > g_ctnt_ext = {
    {"cpp", true},
    {"c", true},
    {"h", true},
    {"hpp", true},
    {"html", true},
    {"js", true},
    {"css", true},
    {"json", true},
    {"md", true},
    {"markdown", true},
    {"txt", true}
};

/*
    Compile the whole web as a single content handler lib
    This class will scan and find all page/css/js/non-binary
    file and format as a cpp source file
*/
// Singleton
content_handlers::content_handlers() :
    mh_(NULL)
{ }

content_handlers& content_handlers::_s_() {
    static content_handlers _sch; return _sch;
}

content_handlers::~content_handlers() {
    if ( mh_ != NULL ) {
        dlclose(mh_); mh_ = NULL;
    }
}

// Append ignore files
void content_handlers::ignore_file( const std::string& ifile ) {
    content_handlers::_s_().ignore_files_.push_back(ifile);
}

// Scan the webroot to find all files
bool content_handlers::scan_webroot() {
    // Clear everything
    content_handlers::_s_().objs_.clear();
    content_handlers::_s_().handler_names_.clear();
    content_handlers::_s_().handlers_.clear();
    content_handlers::_s_().content_uptime_.clear();

    // Format exclude path
    for ( auto& _ep : app.exclude_path ) {
        if ( utils::is_string_start(_ep, "./") ) _ep.erase(0, 2);
        while ( _ep[0] == '/' ) _ep.erase(0, 1);
        if ( *_ep.rbegin() == '/' ) _ep.pop_back();
        _ep = (startupmgr::webroot_dir() + _ep);
    }

    // Add ignore file
    for ( auto& igf : _s_().ignore_files_ ) {
        app.exclude_path.push_back(igf);
    }

    bool _source_code_ok = true;
    utils::rek_scan_dir(
        startupmgr::webroot_dir(), 
        [&_source_code_ok]( const std::string& p, bool d ) {
            // Omit any file/folder begin weith '_'
            if ( utils::filename(p)[0] == '_' ) return false;

            std::string _ext = utils::extension(p);
            if ( (!d) && _ext.size() > 0 ) {
                if ( g_ctnt_ext.find(_ext) == g_ctnt_ext.end() ) {
                    if ( std::find(
                        app.ctnt_exts.begin(), 
                        app.ctnt_exts.end(), 
                        _ext) == app.ctnt_exts.end()
                    ) {
                        // The ext is not null
                        // not in default ext map
                        // not in app's ext list
                        // so ignore it
                        return false;
                    }
                }
            }

            if ( std::find(
                app.exclude_path.begin(), 
                app.exclude_path.end(),
                p
            ) != app.exclude_path.end() ) {
                // Do find in exclude list
                return false;
            }
            // Update the time
            content_handlers::_s_().content_uptime_[p] = utils::file_update_time(p);
            if ( ! d ) {
                if ( ! content_handlers::format_source_code(p) ) {
                    _source_code_ok = false;
                }
            }
            return true;
        }
    );
    return _source_code_ok;
}

// Format and compile source code
bool content_handlers::format_source_code( const std::string& origin_file ) {
    rlog::info << "process source: " << origin_file << std::endl;
    std::string _path = origin_file;
    if ( utils::is_string_start(_path, app.webroot) ) {
        _path.erase(0, app.webroot.size() - 1);
    }
    std::string _output = startupmgr::source_dir() + utils::md5(_path) + ".cpp";

    time_t _origin_time = utils::file_update_time(origin_file);
    time_t _output_time = utils::file_update_time(_output);
    // Only re-process the output file when we have a newer origin file
    if ( _output_time < _origin_time ) {
        std::ofstream _ofs(_output);
        _ofs << "extern \"C\"{" << std::endl;
        _ofs << "void __" << utils::md5(_path)
            << "(const http_request& req, http_response& resp) {" << std::endl;
        // _ofs << "    resp.body.is_chunked = true;" << std::endl;
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
                std::string _piece_path = startupmgr::piece_dir() + _piece_name;
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
                std::string _piece_path = startupmgr::piece_dir() + _piece_name;
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

    std::string _obj = startupmgr::object_dir() + utils::md5(_path) + OBJ_EXT;
    if ( startupmgr::compile_source(_output, _obj) ) {
        // Register the handler list
        content_handlers::_s_().handler_names_[_path] = "__" + utils::md5(_path);
        content_handlers::_s_().objs_.emplace_back(std::move(_obj));
        return true;
    }
    return false;
}

// Check if any content has been changed
bool content_handlers::content_changed( ) {
    for ( const auto& p : content_handlers::_s_().content_uptime_ ) {
        if ( p.second == utils::file_update_time(p.first) ) continue;
        return true;
    }
    return false;
}

// Create handler lib
bool content_handlers::build_handler_lib( ) {
    content_handlers::_s_().hlibpath_ = (
        startupmgr::runtime_dir() + "handlers" + EXT_NAME
    );
    if ( content_handlers::_s_().objs_.size() > 0 ) {
        // Try to create the final lib
        return startupmgr::create_library(
            content_handlers::_s_().objs_, 
            content_handlers::_s_().hlibpath_, 
            startupmgr::lib_path().c_str());
    }
    return true;
}

// Load the handler in worker
void content_handlers::load_handlers() {
    // Load handlers
    if ( ! utils::is_file_existed( content_handlers::_s_().hlibpath_ ) ) {
        return;
    }
    content_handlers& _cs = content_handlers::_s_();

    rlog::debug 
        << "in workers, try to load handler lib: " 
        << _cs.hlibpath_ << std::endl;

    _cs.mh_ = dlopen(_cs.hlibpath_.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if ( !_cs.mh_ ) {
        rlog::error << dlerror() << std::endl;
        throw std::string(dlerror());
    }
    dlerror();
    // Load all handlers
    for ( const auto& pn : _cs.handler_names_ ) {
        http_handler _h = (http_handler)dlsym(_cs.mh_, pn.second.c_str());
        if ( !_h ) {
            const char* _sym_error = dlerror();
            rlog::error << _sym_error << std::endl;
            throw std::string(_sym_error);
        }
        _cs.handlers_[pn.first] = _h;
    }

    // Load all router required handler
    for ( const auto& r : app.router ) {
        http_handler _h = startupmgr::search_handler(r.handler);
        if ( _h == NULL ) {
            rlog::error << "router handler: " << r.handler 
                << " cannot be found" << std::endl;
            throw std::string("handler not found");
        }
        std::string _rpath = "r_" + r.method + "_" + r.handler;
        _cs.handlers_[_rpath] = _h;

        _cs.routers_.push_back({
            r.method, r.handler, std::regex(r.match_rule)
        });
    }
}

// Try to search the handler and run
bool content_handlers::try_find_handler(const http_request& req, http_response& resp) {
    content_handlers& _cs = content_handlers::_s_();

    #ifdef DEBUG
    std::cout << "try to find handler for path: " << req.path() << std::endl;
    #endif
    // Search app.router first
    std::string _p = req.path();

    if ( _cs.routers_.size() > 0 ) {
        for ( auto& r : _cs.routers_ ) {
            if ( req.method() != r.method ) {
                continue;
            }
            if ( ! std::regex_match(_p, r.rule) ) {
                continue;
            }
            // Do fine router
            _p = "r_" + r.method + "_" + r.handler;
            break;
        }
    }

    auto _h = _cs.handlers_.find(_p);
    if ( _h == _cs.handlers_.end() ) {
        return false;
    }
    #ifdef DEBUG
    std::cout << "do find the handler for path: " << req.path() << std::endl;
    #endif
    _h->second(req, resp);
    return true;
}

// Push Chen
