/*
    template.cpp
    Dynamic-Http-Based-On-Cpp
    2020-03-12
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)

    Copyright 2015-2020 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include "template.h"
#include <dlfcn.h>


/*
    Compile the whole web as a single content handler lib
    This class will scan and find all page/css/js/non-binary
    file and format as a cpp source file
*/
// Singleton
content_template::content_template() { }

content_template& content_template::_s_() {
    static content_template _sch; return _sch;
}

content_template::~content_template() { }

// Set default compile extra flags
void content_template::set_compile_flag( const std::string& flags ) {
    content_template::_s_().compile_flag_ = flags;
}

// Scan the webroot to find all files
bool content_template::scan_templates() {
    // Clear everything
    content_template::_s_().objs_.clear();
    content_template::_s_().template_names_.clear();
    content_template::_s_().templates_.clear();
    content_template::_s_().content_uptime_.clear();

    bool _source_code_ok = true;
    utils::rek_scan_dir(
        startupmgr::webroot_dir() + "_template", 
        [&_source_code_ok]( const std::string& p, bool d ) {
            // Omit any file/folder begin weith '_'
            if ( utils::filename(p)[0] == '_' ) return false;
            // Update the time
            content_template::_s_().content_uptime_[p] = utils::file_update_time(p);
            if ( ! d ) {
                if ( ! content_template::format_source_code(p) ) {
                    _source_code_ok = false;
                }
            }
            return true;
        }
    );
    return _source_code_ok;
}

// Format and compile source code
bool content_template::format_source_code( const std::string& origin_file ) {
    rlog::info << "process template: " << origin_file << std::endl;
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
            << "(const http_request& req, http_response& resp, const placeholds_t& ph) {" << std::endl;
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
    const char *_flag = ( content_template::_s_().compile_flag_.size() > 0 ? 
        content_template::_s_().compile_flag_.c_str() : NULL);
    if ( startupmgr::compile_source(_output, _obj, _flag) ) {
        // Register the handler list
        content_template::_s_().template_names_[_path] = "__" + utils::md5(_path);
        content_template::_s_().objs_.emplace_back(std::move(_obj));
        return true;
    }
    return false;
}

// Check if any content has been changed
bool content_template::content_changed( ) {
    for ( const auto& p : content_template::_s_().content_uptime_ ) {
        if ( p.second == utils::file_update_time(p.first) ) continue;
        rlog::info << p.first << " has changed" << std::endl;
        return true;
    }
    return false;
}
// Get all template's object file path
const std::vector< std::string >& content_template::get_template_objs() {
    return content_template::_s_().objs_;
}

// Load the handler in worker
void content_template::init_template(module_t m) {
    if ( !m ) {
        rlog::error << "receive an invalidate module handler, cannot load templates" << std::endl;
        return;
    }
    content_template& _cs = content_template::_s_();

    rlog::debug << "in workers, try to load template lib" << std::endl;

    // Load all handlers
    for ( const auto& pn : _cs.template_names_ ) {
        template_handler_t _h = (template_handler_t)dlsym(m, pn.second.c_str());
        if ( !_h ) {
            const char* _sym_error = dlerror();
            rlog::error << _sym_error << std::endl;
            throw std::string(_sym_error);
        }
        _cs.templates_[pn.first] = _h;
    }
}

// Try to search the template and run
void content_template::apply_template(
    const http_request& req, http_response& resp, 
    const std::string& template_name, const placeholds_t& ph) 
{
    content_template& _cs = content_template::_s_();
    // Search app.router first
    std::string _p = "/_template";
    if ( template_name[0] == '/' ) _p += template_name;
    else _p += ("/" + template_name);

    auto _h = _cs.templates_.find(_p);
    if ( _h == _cs.templates_.end() ) return;
    _h->second(req, resp, ph);
}

// Push Chen
