/*
    template.cpp
    Dynamic-Http-Based-On-Cpp
    2020-03-12
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)

    Copyright 2015-2020 MeetU Infomation and Technology Inc. All rights reserved.
*/

#include "template.h"
#include <dlfcn.h>
#include <hcml.hpp>

bool __dhboc_hcml_is_tag(struct hcml_tag_t* t, const char* name) {
    return (strncmp(t->data_string, name, t->dl) == 0);
}

struct hcml_prop_t* __dhboc_hcml_get_prop(struct hcml_tag_t *t, const char* name) {
    struct hcml_prop_t * _p = t->p_root;
    while ( _p != NULL ) {
        if ( strncmp( _p->key, name, _p->kl ) == 0 ) break;
        _p = _p->n_prop;
    }
    return _p;
}

bool __dhboc_hcml_single_tag( const char* name, int l ) {
    std::string _s(name, l);
    return ( _s == "input" || _s == "img" || _s == "br" || _s == "hr" );
}

// HCML Extend Tag Parser
int content_template::hcml_tag_parser(
    hcml_node_t *h, struct hcml_tag_t *root_tag, const char* suf
) {
    static int _placehold_index = 0;
    int _err_code = 0;
    auto _fp = hcml_set_lang_generator(h, NULL);
    do {
        auto _np = __dhboc_hcml_get_prop(root_tag, "name");
        if ( _np == NULL ) {
            _err_code = 1; break;
        }
        if ( __dhboc_hcml_is_tag(root_tag, "placeholder") ) {
            if ( !hcml_append_code_format(h, "auto _ph%d = ph.find(\"%.*s\");\n", 
                _placehold_index, _np->vl, _np->value)) {
                _err_code = -1; break;
            }
            if ( !hcml_append_code_format(h, "if ( _ph%d != ph.end() ) _ph%d->second();\n", 
                _placehold_index, _placehold_index)) {
                _err_code = -1; break;
            }
            ++_placehold_index;
        } else if ( __dhboc_hcml_is_tag(root_tag, "tag") ) {
            if ( !hcml_append_code_format(h, "resp.write(\"<%.*s\", %d);\n", 
                    _np->vl, _np->value, _np->vl + 1) ) {
                _err_code = -1; break;
            }
            bool _has_inner_html = false;
            bool _has_prop_tag = false;
            if ( root_tag->c_tag != NULL ) {
                // Check if we have non-prop tag, and prop tag after non-prop one
                auto _ct = root_tag->c_tag;
                while ( _ct != NULL ) {
                    if ( __dhboc_hcml_is_tag(_ct, "prop") ) {
                        _has_prop_tag = true;
                        if ( _has_inner_html ) {
                            // Error Here, we already has inner html
                            _err_code = 2; break;
                        }
                    } else {
                        _has_inner_html = true;
                    }
                    _ct = _ct->n_tag;
                }
                if ( _err_code != 0 ) break;
                if ( !_has_prop_tag ) {
                    hcml_append_code_format(h, "resp.write(\">\", 1);\n");
                }

                if ( HCML_ERR_OK != (*_fp)(h, root_tag->c_tag, NULL) ) {
                    _err_code = -1; break;
                }

                if ( !__dhboc_hcml_single_tag(_np->value, _np->vl) ) {
                    hcml_append_code_format(h, "resp.write(\"</%.*s>\", %d);\n", 
                        _np->vl, _np->value, _np->vl + 3);
                }
            } else {
                if ( __dhboc_hcml_single_tag(_np->value, _np->vl) ) {
                    hcml_append_code_format(h, "resp.write(\">\", 1);\n");
                } else {
                    hcml_append_code_format(h, "resp.write(\"</%.*s>\", %d);\n", 
                        _np->vl, _np->value, _np->vl + 3);
                }
            }
        } else if ( __dhboc_hcml_is_tag(root_tag, "prop") ) {
            hcml_append_code_format(h, "resp.write(\" %.*s=\\\"\", %d);\n", 
                _np->vl, _np->value, _np->vl + 3);
            if ( root_tag->c_tag == NULL ) {
                _err_code = 3; break;
            }
            if ( HCML_ERR_OK != (*_fp)(h, root_tag->c_tag, NULL) ) {
                _err_code = -1; break;
            }
            hcml_append_code_format(h, "resp.write(\"\\\"\", 1);\n");
            if ( root_tag->n_tag == NULL || !__dhboc_hcml_is_tag(root_tag->n_tag, "prop") ) {
                hcml_append_code_format(h, "resp.write(\">\", 1);\n");
            }
        } else if ( __dhboc_hcml_is_tag(root_tag, "template") ) {

        } else if ( __dhboc_hcml_is_tag(root_tag, "content") ) {

        } else {
            hcml_set_error(h, HCML_ERR_ESYNTAX, "Syntax Error: Unknow tag: %.*s", 
                root_tag->dl, root_tag->data_string);
            break;
        }
    } while ( false );
    if ( _err_code == 1 ) {
        hcml_set_error(h, HCML_ERR_ESYNTAX, "Syntax Error: Missing property name");
    } else if ( _err_code == 2 ) {
        hcml_set_error(h, HCML_ERR_ESYNTAX, "Syntax Error: prop must at top");
    } else if ( _err_code == 3 ) {
        hcml_set_error(h, HCML_ERR_ESYNTAX, "Syntax Error: Missing content in prop");
    }
    return h->errcode;
}

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
        _h.set_exlang_generator(&content_template::hcml_tag_parser);
        if ( ! _h.parse(origin_file) ) {
            std::cerr << "Parse error, " << _h.errmsg() << std::endl;
            return false;
        }
        std::cout << "Result Size: " << _h.result_size() << std::endl;
        std::cout << _h << std::endl;
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
