/*
    hcmlex.cpp
    Dynamic HTTP Server by Cpp(DHSbC, Internal as DHBoC)
    2020-03-17
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

#include "hcmlex.h"

// Utils
namespace dhboc { 

    // Shorten the type
    typedef struct hcml_tag_t *         p_ctag;
    typedef struct hcml_prop_t *        p_prop;

    #define __DHBOC_CONCAT__(x, y)      x##y

    // Print a tag
    #define __ptag(t)   (t)->dl, (t)->data_string
    #define __pprop(p)  (p)->vl, (p)->value
    #define __tstr(t)   (t)->data_string, (t)->dl
    #define __pkstr(p)  (p)->key, (p)->kl
    #define __pvstr(p)  (p)->value, (p)->vl

    #define __child_tag(t)  (t)->c_tag
    #define __next_tag(t)   (t)->n_tag
    #define __first_prop(t) (t)->p_root
    #define __next_prop(p)  (p)->n_prop

    // Generate code
    #define __code(args...)     hcml_append_code_format(h, args)
    #define __error(msg...)     hcml_set_error(h, HCML_ERR_ESYNTAX, msg); return

    bool __tag_is( p_ctag t, const char* name ) {
        return (strncmp(t->data_string, name, t->dl) == 0);
    }

    p_prop __get_prop(p_ctag t, const char* name) {
        auto _p = t->p_root;
        while ( _p != NULL ) {
            if ( strncmp( _p->key, name, _p->kl) == 0 ) break;
            _p = _p->n_prop; 
        }
        return _p;
    }

    bool is_html_along( const char* name, int l ) {
        std::string _s(name, l);
        return ( _s == "input" || _s == "img" || _s == "br" || _s == "hr" );
    }

    int __child_tag_count( p_ctag t ) {
        auto _ct = __child_tag(t);
        int _c = 0;
        while ( _ct != NULL ) {
            ++_c;
            _ct = __next_tag(_ct);
        }
        return _c;
    }

    p_ctag __child_tag_at_index( p_ctag t, int i ) {
        int _i = 0;
        auto _ct = __child_tag(t);
        while ( _ct != NULL ) {
            if ( _i == i ) return _ct;
            ++_i;
            _ct = __next_tag(_ct);
        }
        return _ct;
    }
    bool __break_silbing_call( p_ctag t, hcml_node_t*h, hcml_lang_generator fp ) {
        auto _n = __next_tag(t);
        __next_tag(t) = NULL;
        (*fp)(h, t, NULL);
        __next_tag(t) = _n;
        return (h->errcode == HCML_ERR_OK);
    }
    #define __single_parse( t )    __break_silbing_call( (t), h, fp )

    #define __must_have_prop(t, pname)                              \
        auto __DHBOC_CONCAT__(_p, pname) = __get_prop(t, #pname);   \
        if ( __DHBOC_CONCAT__(_p, pname) == NULL ) {                \
            __error("Syntax Error: Missing property "#pname);}
    #define _PROP(n)    __DHBOC_CONCAT__(_p, n)

    #define __parse_child(t)                                    \
        if ( HCML_ERR_OK != (*fp)(h, __child_tag(t), NULL) ) return

    #define __TAG_PARSER__(tag_name)            \
    void __parse_tag_##tag_name(hcml_node_t *h, p_ctag t, hcml_lang_generator fp)

    __TAG_PARSER__(placeholder) {
        static int __place_holder_index = 0;
        __must_have_prop(t, name);

        __code("auto _ph%d = ph.find(\"%.*s\");\n", __place_holder_index, __pprop(_PROP(name)));
        __code("if ( _ph%d != ph.end() ) _ph%d->second();\n", 
            __place_holder_index, __place_holder_index);
        ++__place_holder_index;
    }

    __TAG_PARSER__(tag) {
        __must_have_prop(t, name);
        __code("resp.write(\"<%.*s\", %d);\n", __pprop(_PROP(name)), _PROP(name)->vl + 1);
        bool _has_inner_html = false;
        bool _has_prop_tag = false;
        if ( __child_tag(t) != NULL ) {
            auto _ct = __child_tag(t);
            while ( _ct != NULL ) {
                if ( __tag_is(_ct, "prop") || __tag_is(_ct, "prop_optional") ) {
                    _has_prop_tag = true;
                    if ( _has_inner_html ) {
                        __error("Syntax Error: cxx:prop cannot be placed after non-prop tag");
                    }
                } else {
                    _has_inner_html = true;
                }
                _ct = __next_tag(_ct);
            }
            if ( !_has_prop_tag ) {
                // CLose the tag if no prop
                __code("resp.write(\">\", 1);\n");
            }

            // Before we close the tag, process all children
            __parse_child(t);

            // If no child tag, this is just an empty tag
            // means no prop and inner html
            if ( is_html_along(__pvstr(_PROP(name))) ) {
                __code("resp.write(\"</%.*s>\", %d);\n", 
                    __pprop(_PROP(name)), _PROP(name)->vl + 3);
            } else {
                if ( is_html_along(__pvstr(_PROP(name))) ) {
                    __code("resp.write(\">\", 1);\n");
                } else {
                    __code("resp.write(\"</%.*s>\", %d);\n", 
                        __pprop(_PROP(name)), _PROP(name)->vl + 3);
                }
            }
        }
    }

    __TAG_PARSER__(prop) {
        __must_have_prop(t, name);
        __code("resp.write(\" %.*s\", %d);\n", __pprop(_PROP(name)), _PROP(name)->vl + 1);
        if ( __child_tag(t) == NULL ) {
            __error("Syntax Error: Invalidate cxx:prop, missing content");
        }

        int _cc = __child_tag_count(t);
        if ( _cc > 0 ) {
            __code("resp.write(\"=\\\"\");");
            __code("resp.write(");
        }
        for ( int i = 0; i < _cc; ++i ) {
            auto _ct = __child_tag_at_index(t, i);
            if ( _ct->is_tag == 0 ) {
                // Pure String tag should not use default parser in prop
                __code("\"%.*s\"", __ptag(_ct));
            } else {
                __single_parse(_ct);
            }
            if ( i < (_cc - 1) ) {
                __code(" + ");
            }
        }
        if ( _cc > 0 ) {
            __code(");");
        }

        // Close the property value part
        if ( _cc > 0 ) {
            __code("resp.write(\"\\\"\", 1);\n");
        }
        if ( 
            __next_tag(t) == NULL || 
            ( 
                !__tag_is(__next_tag(t), "prop") && 
                !__tag_is(__next_tag(t), "prop_optional") 
            )
        ) {
            // Close parent tag
            __code("resp.write(\">\", 1);\n");
        }
    }
    __TAG_PARSER__(prop_optional) {
        __must_have_prop(t, name);
        int _cc = __child_tag_count(t);
        if ( _cc == 0 ) {
            __error("Syntax Error: cxx:prop_optional must contains at least 1 child");
        }
        // We use the first child as the condition expresion
        __code("if ( ");
        __single_parse(__child_tag(t));
        __code(") {\n");

        __code("resp.write(\" %.*s\", %d);\n", __pprop(_PROP(name)), _PROP(name)->vl + 1);
        if ( __child_tag(t) == NULL ) {
            __error("Syntax Error: Invalidate cxx:prop, missing content");
        }

        if ( _cc > 1 ) {
            __code("resp.write(\"=\\\"\");");
            __code("resp.write(");
        }
        for ( int i = 1; i < _cc; ++i ) {
            auto _ct = __child_tag_at_index(t, i);
            if ( _ct->is_tag == 0 ) {
                // Pure String tag should not use default parser in prop
                __code("\"%.*s\"", __ptag(_ct));
            } else {
                __single_parse(_ct);
            }
            if ( i < (_cc - 1) ) {
                __code(" + ");
            }
        }
        if ( _cc > 1 ) {
            __code(");");
        }

        // Close the property value part
        if ( _cc > 1 ) {
            __code("resp.write(\"\\\"\", 1);\n");
        }
        __code("}\n");

        if ( 
            __next_tag(t) == NULL || 
            ( 
                !__tag_is(__next_tag(t), "prop") && 
                !__tag_is(__next_tag(t), "prop_optional") 
            )
        ) {
            // Close parent tag
            __code("resp.write(\">\", 1);\n");
        }
    }

    __TAG_PARSER__(template) {
        __must_have_prop(t, name);
        auto _ct = __child_tag(t);
        while ( _ct != NULL ) {
            if ( !__tag_is(_ct, "content") ) {
                __error("Syntax Error: cxx:template can only contains cxx:content");
            }
            _ct = __next_tag(_ct);
        }
        __code("apply_template(req, resp, \"%.*s\", {\n", __pprop(_PROP(name)));
        __parse_child(t);
        __code("});");
    }

    __TAG_PARSER__(content) {
        __must_have_prop(t, name);
        __code("{\"%.*s\", [&](){\n", __pprop(_PROP(name)));
        if ( __child_tag(t) != NULL ) {
            __parse_child(t);
        }
        if ( __next_tag(t) == NULL ) {
            __code("}}\n");
        } else {
            __code("}},\n");
        }
    }

    __TAG_PARSER__(params_check) {
        __must_have_prop(t, key);
        __must_have_prop(t, code);
        __code("if ( !url_params::contains(req.params, \"%.*s\") ) {\n", __pprop(_PROP(key)));
        __code("resp.status_code = %.*s;\n", __pprop(_PROP(code)));
        auto _loc = __get_prop(t, "location");
        if ( _loc == NULL ) {
            __code("resp.header[\"Location\"] = \"/index.html\";\n");
        } else {
            __code("resp.header[\"Location\"] = \"%.*s\";\n", __pprop(_loc));
        }
        __code("return;\n}");
    }

    __TAG_PARSER__(redis_getobj) {
        auto _pvar = __get_prop(t, "var");
        if ( _pvar != NULL ) {
            __code("auto %.*s = ", __pprop(_pvar));
        }
        __must_have_prop(t, type);
        __code("dhboc::redis::get_object(\"%.*s\", ", __pprop(_PROP(type)));
        if ( __child_tag(t) == NULL ) {
            __error("Syntax Error: cxx:redis_getobj must have content as the object id express");
        }
        __parse_child(t);
        if ( _pvar != NULL ) {
            __code(");");
        }
    }

    __TAG_PARSER__(json_null_check) {
        __must_have_prop(t, var);
        __must_have_prop(t, code);
        __code("if ( %.*s.isNull() ) {\n", __pprop(_PROP(var)));
        __code("resp.status_code = %.*s;\n", __pprop(_PROP(code)));
        auto _loc = __get_prop(t, "location");
        if ( _loc == NULL ) {
            __code("resp.header[\"Location\"] = \"/index.html\";\n");
        } else {
            __code("resp.header[\"Location\"] = \"%.*s\";\n", __pprop(_loc));
        }
        __code("return;\n}");
    }
    __TAG_PARSER__(params_get) {
        /*
            [auto _var = ](url_params::contains(req.params, key) ? req.params[key] : default);
        */
        auto _pvar = __get_prop(t, "var");
        __must_have_prop(t, key);
        auto _pdefault = __get_prop(t, "default");
        auto _ptoint = __get_prop(t, "toint");
        if ( _ptoint != NULL && _pdefault == NULL ) {
            __error("Syntax Error: cxx:params_get toint must used with default");
        }

        if ( _pvar != NULL ) {
            __code("auto %.*s = ", __pprop(_pvar));
        }
        if ( _ptoint != NULL ) {
            __code("std::stoi(");
        }
        __code("(url_params::contains(req.params, \"%.*s\") ? req.params[\"%.*s\"] : ", 
            __pprop(_PROP(key)), __pprop(_PROP(key)));
        if ( _pdefault != NULL ) {
            __code("\"%.*s\")", __pprop(_pdefault));
        } else {
            // Empty string
            __code("\"\")");
        }
        if ( _ptoint != NULL ) {
            __code(")");
        }
        if ( _pvar != NULL ) {
            __code(";");
        }
    }
    __TAG_PARSER__(session_get) {
        /*
            [auto _var = ][](){ auto _s = dhboc::session::get(key); return _s.size() > 0 ? _s : defulat; }();
        */
        auto _pvar = __get_prop(t, "var");
        __must_have_prop(t, key);
        auto _pdefault = __get_prop(t, "default");
        auto _ptoint = __get_prop(t, "toint");
        if ( _ptoint != NULL && _pdefault == NULL ) {
            __error("Syntax Error: cxx:session_get toint must used with default");
        }

        if ( _pvar != NULL ) {
            __code("auto %.*s = ", __pprop(_pvar));
        }
        if ( _ptoint != NULL ) {
            __code("std::stoi(");
        }
        __code("[](){ auto _s = dhboc::session::get(\"%.*s\"); return _s.size() > 0 ? _s : ", 
            __pprop(_PROP(key)));
        if ( _pdefault != NULL ) {
            __code("\"%.*s\"; }()", __pprop(_pdefault));
        } else {
            __code("\"\"; }()");
        }
        if ( _ptoint != NULL ) {
            __code(")");
        }
        if ( _pvar != NULL ) {
            __code(";");
        }
    }

    __TAG_PARSER__(redis_getindex) {
        auto _pvar = __get_prop(t, "var");
        __must_have_prop(t, type);

        int _cc = __child_tag_count(t);
        if ( _cc > 3 ) {
            __error("Syntax Error: cxx:redis_getindex can have no more than 3 children");
        }
        if ( _cc == 0 ) {
            __error("Syntax Error: cxx:redis_getindex must have at least one child node");
        }
        if ( _cc == 3 ) {
            // last tag must be list
            auto _lt = __child_tag_at_index(t, 2);
            if ( !__tag_is(_lt, "list") ) {
                __error("Syntax Error: cxx:redis_getindex's last tag must be cxx:list");
            }
        }
        if ( _pvar != NULL ) {
            __code("auto %.*s = ", __pprop(_pvar));
        }
        __code("dhboc::redis::index_object(\"%.*s\", ", __pprop(_PROP(type)));
        __single_parse( __child_tag_at_index(t, 0) );
        if ( _cc > 1 ) {
            __code(", ");
            __single_parse( __child_tag_at_index(t, 1) );
        }
        if ( _cc > 2 ) {
            __code(", ");
            __single_parse( __child_tag_at_index(t, 2) );
        }

        __code(")");
        if ( _pvar != NULL ) {
            __code(";");
        }
    }

    __TAG_PARSER__(redis_tag) {
        if ( __child_tag(t) == NULL ) {
            __error("Syntax Error: cxx:redis_tag must have content");
        }
        // ^<tag> : put this tag in front of the list
        auto _phead = __get_prop(t, "head");
        // $<tag> : put this tag in end of the list
        auto _ptail = __get_prop(t, "tail");
        // +<tag> / <tag> : only match the tag
        auto _pmatch = __get_prop(t, "match");
        // -<tag> : only not contains the tag
        auto _pomit = __get_prop(t, "omit");
        // tag order format: 
        // <<tag> : order from small to large
        auto _pasc = __get_prop(t, "asc");
        // ><tag> : order from large to small
        auto _pdesc = __get_prop(t, "desc");

        if ( _pasc != NULL && _pdesc != NULL ) {
            __error("Syntax Error: cxx:redis_tag can only set one order");
        }
        if ( _phead != NULL && _ptail != NULL ) {
            __error("Syntax Error: cxx:redis_tag can not use both head and tail");
        }
        if ( _pmatch != NULL && _pomit != NULL ) {
            __error("Syntax Error: cxx:redis_tag can not use both match and omit");
        }
        std::string _d;
        if ( _phead != NULL ) _d += "^";
        if ( _ptail != NULL ) _d += "$";
        if ( _pmatch != NULL ) _d += "+";
        if ( _pomit != NULL ) _d += "-";
        if ( _pasc != NULL ) _d += "<";
        if ( _pdesc != NULL ) _d += ">";
        if ( _d.size() > 0 ) {
            __code("std::string(\"%s\") + ", _d.c_str());
        }

        int _cc = __child_tag_count(t);
        for ( int i = 0; i < _cc; ++i ) {
            __single_parse(__child_tag_at_index(t, i));
            if ( i < (_cc - 1) ) {
                __code(" + ");
            }
        }
    }

    __TAG_PARSER__(json_string) {
        auto _pvar = __get_prop(t, "var");
        __must_have_prop(t, target);
        __must_have_prop(t, key);
        if ( _pvar != NULL ) {
            __code("auto %.*s = ", __pprop(_pvar));
        }
        __code("%.*s[\"%.*s\"].asString()", __pprop(_PROP(target)), __pprop(_PROP(key)));
        if ( _pvar != NULL ) {
            __code(";");
        }
    }
    __TAG_PARSER__(json_int) {
        auto _pvar = __get_prop(t, "var");
        __must_have_prop(t, target);
        __must_have_prop(t, key);
        if ( _pvar != NULL ) {
            __code("auto %.*s = ", __pprop(_pvar));
        }
        __code("%.*s[\"%.*s\"].asInt()", __pprop(_PROP(target)), __pprop(_PROP(key)));
        if ( _pvar != NULL ) {
            __code(";");
        }
    }
    __TAG_PARSER__(json_number) {
        auto _pvar = __get_prop(t, "var");
        __must_have_prop(t, target);
        __must_have_prop(t, key);
        if ( _pvar != NULL ) {
            __code("auto %.*s = ", __pprop(_pvar));
        }
        __code("%.*s[\"%.*s\"].asDouble()", __pprop(_PROP(target)), __pprop(_PROP(key)));
        if ( _pvar != NULL ) {
            __code(";");
        }
    }
    __TAG_PARSER__(json_bool) {
        auto _pvar = __get_prop(t, "var");
        __must_have_prop(t, target);
        __must_have_prop(t, key);
        if ( _pvar != NULL ) {
            __code("auto %.*s = ", __pprop(_pvar));
        }
        __code("%.*s[\"%.*s\"].asBool()", __pprop(_PROP(target)), __pprop(_PROP(key)));
        if ( _pvar != NULL ) {
            __code(";");
        }
    }
    __TAG_PARSER__(import) {
        if ( __child_tag(t) == NULL ) {
            __error("Syntax Error: Missing file in cxx:import");
        }
        __code("resp.body.load_file(");
        if ( __child_tag(t)->is_tag == 0 ) {
            // Pure String tag should not use default parser in prop
            __code("\"%.*s\"", __ptag(__child_tag(t)));
        } else {
            __single_parse(__child_tag(t));
        }
        __code(");\n");
    }

    #define __REG_TAG__(tag_name)               \
        {#tag_name, __parse_tag_##tag_name}

    const static struct {
        const char *            fname;
        void (*func)(hcml_node_t *, p_ctag, hcml_lang_generator);
    } __tag_parser_map [] = {
        __REG_TAG__(placeholder),
        __REG_TAG__(tag),
        __REG_TAG__(prop),
        __REG_TAG__(prop_optional),
        __REG_TAG__(template),
        __REG_TAG__(content),
        __REG_TAG__(params_check),
        __REG_TAG__(redis_getobj),
        __REG_TAG__(json_null_check),
        __REG_TAG__(params_get),
        __REG_TAG__(session_get),
        __REG_TAG__(redis_getindex),
        __REG_TAG__(redis_tag),
        __REG_TAG__(json_string),
        __REG_TAG__(json_int),
        __REG_TAG__(json_number),
        __REG_TAG__(json_bool),
        __REG_TAG__(import)
    };

    // DHBoC HCML Extended Parser
    int hcml_tag_parser(
        hcml_node_t *h, struct hcml_tag_t *root_tag, const char* suf
    ) {
        auto _fp = hcml_set_lang_generator(h, NULL);
        std::string _tagname(__tstr(root_tag));
        bool _match_tag = false;
        for ( int i = 0; i < sizeof(__tag_parser_map) / sizeof(__tag_parser_map[0]); ++i ) {
            if ( _tagname == __tag_parser_map[i].fname ) {
                __tag_parser_map[i].func(h, root_tag, _fp);
                _match_tag = true;
                break;
            }
        }
        if ( !_match_tag ) {
            __error("Syntax Error: Unknow tag: %.*s", __ptag(root_tag)) h->errcode;
        }
        return h->errcode;
    }

};

// Push Chen
