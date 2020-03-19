/*
    template.cpp
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-03-12
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

#include "template.h"
#include <dlfcn.h>
#include "hcmlex.h"

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
        _ofs << "    resp.body.is_gzipped = true;" << std::endl;
        std::string _piece_prefix = utils::md5(_path);

        hcml _h;
        // All use default, excpet print method
        _h.set_print_method("resp.write");
        _h.set_exlang_generator(&dhboc::hcml_tag_parser);
        if ( ! _h.parse(origin_file) ) {
            std::cerr << _h.errmsg() << std::endl;
            return false;
        }
        _ofs << _h << std::endl;
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
    } else {
        // Remove Output file because it's not correct
        utils::fs_remove(_output);
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
