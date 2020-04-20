#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "cparse.h"
#include "pydoc.h"
#include "swigmod.h"

#include <stdint.h>
#include <iostream>

namespace wasm {

class Template {
   public:
    Template(const String *code);
    Template(const String *code, const String *templateName);
    Template(const Template &other);
    ~Template();
    String *str();
    Template &replace(const String *pattern, const String *repl);
    Template &print(DOH *doh);
    Template &pretty_print(DOH *doh);
    void operator=(const Template &t);
    Template &trim();

   private:
    String *code;
    String *templateName;
};

class EmscriptenWasm : public Language {
   public:
    EmscriptenWasm() {}
    ~EmscriptenWasm();

    virtual void main(int argc, char *argv[]) override;
    virtual int top(Node *n) override;

    bool isOverloaded(Node *n) {
        Node *h = Getattr(n, "sym:overloaded");
        return !!h;
    }

    /* Function handlers */

    virtual int functionHandler(Node *n) override;
    virtual int globalfunctionHandler(Node *n) override;
    virtual int memberfunctionHandler(Node *n) override;
    virtual int staticmemberfunctionHandler(Node *n) override;
    virtual int callbackfunctionHandler(Node *n) override;

    /* Variable handlers */

    virtual int variableHandler(Node *n) override;
    virtual int globalvariableHandler(Node *n) override;
    virtual int membervariableHandler(Node *n) override;
    virtual int staticmembervariableHandler(Node *n) override;

    /* C++ handlers */

    virtual int memberconstantHandler(Node *n) override;
    virtual int constructorHandler(Node *n) override;
    virtual int copyconstructorHandler(Node *n) override;
    virtual int destructorHandler(Node *n) override;
    virtual int classHandler(Node *n) override;

    virtual int functionWrapper(Node *n) override;
    virtual int constantWrapper(Node *n) override;
    virtual int nativeWrapper(Node *n) override;

    virtual int enumDeclaration(Node *n) override;
    virtual int enumvalueDeclaration(Node *n) override;

   protected:
    virtual int fragmentDirective(Node *n) override;

    int _classConstructorWrapper(Node *n);
    String *getClassFullMemberName(Node *n);

    String *wrapFunction(Node *n);
    String *wrapWrapperFuntion(Node *n);
    String *generateSetterFunction(Node *n);
    String *generateGetterFunction(Node *n);

    String *generateOverloadedFunctionShadowCode(Node *n);
    String *selectOverloadedFuntionShadowCode(String *sym_funcname,
                                              ParmList *plist);
    bool checkHasRawPointer(SwigType *return_type, ParmList *plist);
    String *extendFuntionCode(Node *);

    int isMember(Node *n) {
        return GetFlag(n, "ismember") != 0 || GetFlag(n, "feature:extend") != 0;
    }

    String *wasmFunctionName(Node *n);

    Template getTemplate(const String *name) {
        String *templ = Getattr(templates, name);

        if (!templ) {
            Printf(stderr, "Could not find template %s\n.", name);
            SWIG_exit(EXIT_FAILURE);
        }
        return Template(templ, name);
    }

    String *paramsToStr(ParmList *p, int t);
    String *parmsToJsTypeStrArray(ParmList *p);

   private:
    File *f_begin = 0;
    File *f_header = 0;
    File *f_wrappers = 0;
    File *f_directors = 0;
    File *f_init = 0;
    File *f_runtime = 0;
    File *f_overloaded_funcs = 0;
    File *f_extends = 0;
    File *f_overload_js = 0;
    // String *s_shadow = 0;
    File *f_shadow_js = 0;
    String* s_constant_wrapper = 0;
    // String* s_last_overloaed_fun = 0;

    Hash *class_members = 0;

    String *methods = 0;

    Hash *templates;
    
    bool in_enum = false;
};

EmscriptenWasm::~EmscriptenWasm() {
#define _Free(v) \
    if (v) Delete(v);

    _Free(f_header);
    _Free(f_wrappers);
    _Free(f_directors);
    _Free(f_init);

    // String *s_shadow = 0;
    _Free(f_shadow_js);

    _Free(class_members);

    _Free(methods);
    _Free(f_begin);
    _Free(f_runtime);
    _Free(f_overloaded_funcs);
    _Free(f_extends);
    _Free(templates);
    _Free(f_overload_js);
    _Free(s_constant_wrapper);
    // _Free(s_last_overloaed_fun);
}

void EmscriptenWasm::main(int argc, char *argv[]) {
    SWIG_library_directory("wasm");
    // SWIG_library_directory("javascript");
    // SWIG_library_directory("javascript/v8");
    SWIG_typemap_lang("wasm");
    SWIG_config_file("em_wasm.swg");
    Preprocessor_define("SWIGWASM", 1);
    allow_overloading();

    f_header = NewString("");
    f_wrappers = NewString("");
    f_directors = NewString("");
    f_init = NewString("");
    f_runtime = NewString("");
    f_overloaded_funcs = NewString("");
    f_extends = NewString("");
    f_overload_js = NewString("");
    s_constant_wrapper = NewString("");
    // s_last_overloaed_fun = NewString("_");

    // s_shadow = NewString("");
    class_members = NewHash();
    methods = NewString("");

    templates = NewHash();
}

int EmscriptenWasm::top(Node *n) {
    String *outfile = Getattr(n, "outfile");
    std::cout << "out file: " << Char(outfile) << std::endl;

    f_begin = NewFile(outfile, "w", SWIG_output_files());
    if (!f_begin) {
        FileErrorDisplay(outfile);
        SWIG_exit(EXIT_FAILURE);
    }

    {
        String *module = Copy(Getattr(n, "name"));
        String *filen =
            NewStringf("%s%s_post.js", SWIG_output_directory(), Char(module));

        if ((f_shadow_js = NewFile(filen, "w", SWIG_output_files())) == 0) {
            FileErrorDisplay(filen);
            SWIG_exit(EXIT_FAILURE);
        }

        // Template tmpl = getTemplate("emwasm_module_begin");
        // tmpl.replace("$module", module);
        // tmpl.pretty_print(f_wrappers);
        Printf(f_wrappers, "EMSCRIPTEN_BINDINGS(%s) {\n", module);

        Delete(filen);
        filen = NULL;
    }

    Swig_register_filebyname("runtime", f_runtime);
    Swig_register_filebyname("begin", f_begin);
    Swig_register_filebyname("header", f_header);
    Swig_register_filebyname("wrapper", f_wrappers);
    Swig_register_filebyname("init", f_init);
    Swig_register_filebyname("director", f_directors);
    Swig_register_filebyname("shadow", f_shadow_js);

    Swig_banner(f_begin);
    { Swig_banner_target_lang(f_shadow_js, "//"); }

    /* emit code */

    Language::top(n);

    Dump(s_constant_wrapper, f_wrappers);
    Printf(f_wrappers, "};\n");
    // std::cout << Char(f_wrappers) << std::endl;
    // Dump(f_runtime, f_begin);
    String *wasm_header = Getattr(templates, "emwasm_include_header");
    Printf(f_header, "%s", wasm_header);
    Dump(f_header, f_begin);
    Dump(f_extends, f_begin);
    Dump(f_overloaded_funcs, f_begin);
    Dump(f_wrappers, f_begin);
    Wrapper_pretty_print(f_init, f_begin);

    // shadow js
    String *emwasm_js_checktype = Getattr(templates, "emwasm_js_checktype");
    Printf(f_shadow_js, "%s\n\n", emwasm_js_checktype);
    Printf(f_shadow_js, "__ATMAIN__.push( () => {\n");
    Dump(f_overload_js, f_shadow_js);
    Printf(f_shadow_js, "});\n");

    return SWIG_OK;
}

void printNode(Node *n, const char *funcn) {
    // return;
    Iterator k;
    Printf(stdout, "===========\n[%s]\n", funcn);
    for (k = First(n); k.key; k = Next(k)) {
        char *ckey = Char(k.key);
        if ((strcmp(ckey, "nextSibling") == 0) ||
            (strcmp(ckey, "previousSibling") == 0) ||
            (strcmp(ckey, "parentNode") == 0) ||
            (strcmp(ckey, "lastChild") == 0)) {
            continue;
        }
        Printf(stdout, "%s: %s\n", k.key, k.item);
    }
    Printf(stdout, "------------\n\n");
}

/* Function handlers */

int EmscriptenWasm::functionHandler(Node *n) {
    Language::functionHandler(n);
    // printNode(n, __FUNCTION__);
    // Printf(stdout, "[%s] %s\n", Getattr(n, "name"),
    // parmsToJsTypeStrArray(Getattr(n, "parms")));

    Node *overload = Getattr(n, "sym:overloaded");
    // shadow js
    if (overload && GetFlag(n, "__wasm_wraped") == 0) {
        Node *classn = getCurrentClass();
        String *wasm_fname = Getattr(n, "sym:name");
        int isstatic =
            getCurrentClass() && Swig_storage_isstatic(n) &&
            !(SmartPointer && Getattr(n, "allocate:smartpointeraccess"));

        if (classn) {
            String *wasm_cname = Getattr(classn, "sym:name");
            if (isstatic) {
                Printf(f_overload_js, "    Module['%s'].%s = function () {\n",
                       wasm_cname, wasm_fname);
            } else {
                Printf(f_overload_js,
                       "    Module['%s'].prototype['%s'] = function () {\n",
                       wasm_cname, wasm_fname);
            }
        } else {
            Printf(f_overload_js, "    Module.%s = function () {\n",
                   wasm_fname);
        }
        Node *nod = overload;

        while (nod) {
            generateOverloadedFunctionShadowCode(nod);
            nod = Getattr(nod, "sym:nextSibling");
        }
        Printf(f_overload_js,
               "        throw('Arguments are illegal! No overloaded function "
               "was matched!');\n");
        Printf(f_overload_js, "    };\n\n");
    }

    wrapFunction(n);
    return SWIG_OK;
}

int EmscriptenWasm::globalfunctionHandler(Node *n) {
    Language::globalfunctionHandler(n);

    // printNode(n, __FUNCTION__);
    // if (isMember(n)) return SWIG_OK;
    // String* wrap_str = wrapFunction(n);
    // Printf(f_wrappers, "    %s;\n", wrap_str);
    return SWIG_OK;
}


int EmscriptenWasm::memberfunctionHandler(Node *n) {
    Language::memberfunctionHandler(n);
    // printNode(n, __FUNCTION__);

    // String* wrap_str = wrapFunction(n);
    // Printf(f_wrappers, "        .%s\n", wrap_str);
    return SWIG_OK;
}

int EmscriptenWasm::staticmemberfunctionHandler(Node *n) {
    Language::staticmemberfunctionHandler(n);
    // printNode(n, __FUNCTION__);
    // String* wrap_str = wrapFunction(n);
    // Printf(f_wrappers, "        .class_%s\n", wrap_str);

    return SWIG_OK;
}
int EmscriptenWasm::callbackfunctionHandler(Node *n) {
    String *name = Getattr(n, "sym:name");
    Printf(stdout, "[%s]->[%s]\n\n", __FUNCTION__, name);

    Language::callbackfunctionHandler(n);
    printNode(n, __FUNCTION__);
    return SWIG_OK;
}

/* Variable handlers */

int EmscriptenWasm::variableHandler(Node *n) {
    Language::variableHandler(n);
    return SWIG_OK;
}
int EmscriptenWasm::globalvariableHandler(Node *n) {
    Language::globalvariableHandler(n);

    if (getCurrentClass()) {
        return SWIG_OK;
    }
    if (checkHasRawPointer(Getattr(n, "type"), 0)) {
        Printf(stdout, "Donot support variable[%s %s] which is pointer type.\n",
               SwigType_str(Getattr(n, "type"), 0), getClassFullMemberName(n));
        return SWIG_OK;
    }
    // printNode(n, __FUNCTION__);
    String *name = Getattr(n, "name");
    String *wasm_name = Getattr(n, "sym:name");

    Printf(f_wrappers, "    constant(\"%s\", %s);\n", wasm_name, name);
    return SWIG_OK;
}
int EmscriptenWasm::membervariableHandler(Node *n) {
    Language::membervariableHandler(n);
    // printNode(n, __FUNCTION__);

    String *name = getClassFullMemberName(n);
    String *wasm_name = Getattr(n, "sym:name");
    if (checkHasRawPointer(Getattr(n, "type"), 0)) {
        // Printf(stdout, "Donot support variable[%s %s] which is pointer type.\n",
            //    SwigType_str(Getattr(n, "type"), 0), getClassFullMemberName(n));
        Delete(name);
        name = generateGetterFunction(n);
    }

    Template tmpl = getTemplate("emwasm_class_property");
    tmpl.replace("$name", wasm_name).replace("$value", name);
    Printf(f_wrappers, "%s    %s\n", tab4, tmpl.trim().str());

    Delete(name);
    return SWIG_OK;
}
int EmscriptenWasm::staticmembervariableHandler(Node *n) {
    Language::staticmembervariableHandler(n);
    if (checkHasRawPointer(Getattr(n, "type"), 0)) {
        Printf(stdout, "Donot support variable[%s %s] which is pointer type.\n",
               SwigType_str(Getattr(n, "type"), 0), getClassFullMemberName(n));
        return SWIG_OK;
    }
    if (1) {  // compile error: 'undefined symbol'
        return SWIG_OK;
    }

    String *name = getClassFullMemberName(n);
    String *wasm_name = Getattr(n, "sym:name");

    Template tmpl = getTemplate("emwasm_class_static_property");
    tmpl.replace("$name", wasm_name).replace("$value", name);
    Printf(f_wrappers, "%s    %s\n", tab4, tmpl.trim().str());

    return SWIG_OK;
}

/* C++ handlers */

int EmscriptenWasm::memberconstantHandler(Node *n) {
    Language::memberconstantHandler(n);
    if (checkHasRawPointer(Getattr(n, "type"), 0)) {
        Printf(stdout, "Donot support variable[%s %s] which is pointer type.\n",
               SwigType_str(Getattr(n, "type"), 0), getClassFullMemberName(n));
        return SWIG_OK;
    }

    // printNode(n, __FUNCTION__);
    int isstatic = Swig_storage_isstatic(n) &&
                   !(SmartPointer && Getattr(n, "allocate:smartpointeraccess"));
    if (isstatic) {
        return SWIG_OK;
    }
    String *name = getClassFullMemberName(n);
    String *wasm_name = Getattr(n, "sym:name");
    SwigType *type = Getattr(n, "type");

    String *new_name = NewStringEmpty();
    Printf(new_name, "__swig__%s_%s", Getattr(getCurrentClass(), "sym:name"),
           wasm_name);
    Printf(f_header, "const %s %s = %s;\n", SwigType_str(type, 0), new_name,
           name);

    Template tmpl = getTemplate("emwasm_class_static_property");
    tmpl.replace("$name", wasm_name).replace("$value", new_name);
    Printf(f_wrappers, "%s    %s\n", tab4, tmpl.trim().str());

    Delete(new_name);

    return SWIG_OK;
}

int EmscriptenWasm::constructorHandler(Node *n) {
    Language::constructorHandler(n);
    return _classConstructorWrapper(n);
}
int EmscriptenWasm::copyconstructorHandler(Node *n) {
    Language::copyconstructorHandler(n);
    return _classConstructorWrapper(n);
}
int EmscriptenWasm::destructorHandler(Node *n) {
    Language::destructorHandler(n);
    return SWIG_OK;
}

int EmscriptenWasm::classHandler(Node *n) {
    String *classtype = Getattr(n, "classtype");
    String *name = Getattr(n, "sym:name");
    List *bases = Getattr(n, "allbases");
    int bases_len = Len(bases);

    // printNode(n, __FUNCTION__);
    Printf(f_header, "const std::string __swig__%s = \"%s\";\n", name,
           classtype);
    Append(f_wrappers, "\n");
    if (bases_len == 0) {
        Template wasm_class = getTemplate("emwasm_class");

        wasm_class.replace("$class_type", classtype)
            .replace("$wasm_classname", name);
        Printf(f_wrappers, "%s%s\n", tab4, wasm_class.trim().str());
    } else if (bases_len > 0) {
        Node *base_n = Getitem(bases, 0);
        String *base_classtype = Getattr(base_n, "classtype");
        Template wasm_class = getTemplate("emwasm_class_with_base");

        wasm_class.replace("$class_type", classtype)
            .replace("$base_class", base_classtype)
            .replace("$wasm_classname", name);
        Printf(f_wrappers, "%s%s\n", tab4, wasm_class.trim().str());

        if (bases_len > 1) {
            Printf(stdout, "Warning: Only surport ONE base class.");
            for (int i = 1; i < bases_len; i++) {
                Node *bn = Getitem(bases, i);
                Printf(stdout, "(%s) ", Getattr(bn, "classtype"));
            }
            Printf(stdout, "will be ignore!");
        }
    }
    // add type to class property
    Printf(f_wrappers, "        .class_property(\"__type\", &__swig__%s)\n",
           name);
    // Printf(f_wrappers, "        .property(\"__type\", &__swig__%s)\n", name);

    Language::classHandler(n);

    Printf(f_wrappers, "        ;\n\n");
    return SWIG_OK;
}

int EmscriptenWasm::functionWrapper(Node *n) {
    if (1) return SWIG_OK;

    String *name = Getattr(n, "name");
    String *iname = Getattr(n, "sym:name");
    // SwigType *d = Getattr(n, "type");
    ParmList *l = Getattr(n, "parms");
    // Node *parent = Swig_methodclass(n);
    // String *nodeType = Getattr(n, "nodeType");
    // String *storage = Getattr(n, "storage");
    // printNode(n, __FUNCTION__);

    Printf(f_wrappers, "%s.%s[%s](", tab4, name, iname);
    ParmList *p = l;

    if (p) {
        String *lname = Getattr(p, "lname");
        Printf(f_wrappers, "%s", lname ? lname : "");
        p = nextSibling(p);
    }
    while (p) {
        String *lname = Getattr(p, "lname");
        Printf(f_wrappers, ", %s", lname ? lname : "");
        p = nextSibling(p);
    }

    Printf(f_wrappers, ")\n");
    Language::functionWrapper(n);
    return SWIG_OK;
}
int EmscriptenWasm::constantWrapper(Node *n) {
    if (getCurrentClass()) {
        return SWIG_OK;
    }
    if (checkHasRawPointer(Getattr(n, "type"), 0)) {
        Printf(stdout, "Donot support variable[%s %s] which is pointer type.\n",
               SwigType_str(Getattr(n, "type"), 0), getClassFullMemberName(n));
        return SWIG_OK;
    }
    // if(Strcmp(nodeType(n), "enumitem") == 0) { // deal in enum enumvalueDeclaration
    //     return SWIG_OK;
    // }

    String *name = Getattr(n, "name");
    String *wasm_name = Getattr(n, "sym:name");

    Printf(s_constant_wrapper, "    constant(\"%s\", %s);\n", wasm_name, name);
    // Language::constantWrapper(n);
    return SWIG_OK;
}

int EmscriptenWasm::nativeWrapper(Node *n) {
    String *name = Getattr(n, "sym:name");
    String *wrapname = Getattr(n, "wrap:name");

    printNode(n, __FUNCTION__);
    // Printf(f_wrappers, "native %s[%s];\n", wrapname, name);
    Language::nativeWrapper(n);
    return SWIG_OK;
}

int EmscriptenWasm::enumDeclaration(Node *n) {
    if (!Getattr(n, "sym:name")){
        return Language::enumDeclaration(n);;
    }
    in_enum = true;
    Printf(f_wrappers, "    enum_<%s>(\"%s\")\n", Getattr(n, "name"), Getattr(n, "sym:name"));
    Language::enumDeclaration(n);
    Printf(f_wrappers, "    ;\n\n");
    in_enum = false;
    // printNode(n, __FUNCTION__);
    return SWIG_OK;
}
int EmscriptenWasm::enumvalueDeclaration(Node *n) {
    Language::enumvalueDeclaration(n);
    if (!in_enum || getCurrentClass()) return SWIG_OK;

    String *value = Getattr(n, "value");
    String *name = Getattr(n, "name");
    String *tmpValue;

    if (value)
        tmpValue = NewString(value);
    else
        tmpValue = NewString(name);
    Printf(f_wrappers, "    .value(\"%s\", %s)\n", Getattr(n, "sym:name"), tmpValue);
    // printNode(n, __FUNCTION__);
    return SWIG_OK;
}

int EmscriptenWasm::fragmentDirective(Node *n) {
    String *section = Getattr(n, "section");
    if (Equal(section, "templates") && !ImportMode) {
        // Printf(stdout, "%s:\n%s\n\n", Getattr(n, "value"), Getattr(n,
        // "code"));
        Setattr(templates, Getattr(n, "value"), Getattr(n, "code"));
        return SWIG_OK;
    } else {
        return Language::fragmentDirective(n);
    }
}

int EmscriptenWasm::_classConstructorWrapper(Node *n) {
    ParmList *plist = Getattr(n, "parms");
    Template tmpl = getTemplate("emwasm_class_constructor");
    String *pstr = paramsToStr(plist, 1);
    tmpl.replace("$param_types", pstr);
    Delete(pstr);
    Printf(f_wrappers, "%s    %s\n", tab4, tmpl.trim().str());
    return SWIG_OK;
}

String *EmscriptenWasm::getClassFullMemberName(Node *n) {
    String *name = Getattr(n, "name");
    Node *classn = getCurrentClass();
    if (!classn) return NewString(name);
    String *class_type = Getattr(classn, "classtype");

    String *full_name = NewString("");
    Printf(full_name, "%s::%s", class_type, name);
    return full_name;
}

String *EmscriptenWasm::wrapFunction(Node *n) {
    // String *storage = Getattr(n, "storage");
    // int isfriend = getCurrentClass() && Cmp(storage, "friend") == 0;
    int isstatic = getCurrentClass() && Swig_storage_isstatic(n) &&
                   !(SmartPointer && Getattr(n, "allocate:smartpointeraccess"));
    Hash *overload = Getattr(n, "sym:overloaded");
    int is_extend = GetFlag(n, "feature:extend");
    bool is_template = !!Getattr(n, "template");

    String *fname, *wasm_name;

    wasm_name = wasmFunctionName(n);
    if (overload || is_extend || is_template) {
        fname = wrapWrapperFuntion(n);
    } else {
        fname = getClassFullMemberName(n);
    }

    if (checkHasRawPointer(Getattr(n, "type"), Getattr(n, "parms"))) {
        Append(fname, ", allow_raw_pointers()");
    }

    Template func = getTemplate("emwasm_function");
    func.replace("$name", wasm_name).replace("$func", fname);

    String *func_str = func.trim().str();
    if (!getCurrentClass()) {
        Printf(f_wrappers, "    %s;\n", func_str);
    } else {
        if (isstatic) {
            Printf(f_wrappers, "        .class_%s\n", func_str);
        } else {
            Printf(f_wrappers, "        .%s\n", func_str);
        }
    }

    Delete(fname);
    Delete(wasm_name);

    return func_str;
}

String *EmscriptenWasm::wrapWrapperFuntion(Node *n) {
    Node *classn = getCurrentClass();
    String *name = Getattr(n, "name");
    String *func_name;

    int is_extend = GetFlag(n, "feature:extend");
    String *call_str = NewStringEmpty();
    ParmList *plist = Getattr(n, "parms");
    String *parms_str = paramsToStr(plist, 0);

    String *return_type = SwigType_str(Getattr(n, "type"), 0);
    String *parm_names_str = paramsToStr(plist, 2);
    bool rtype_is_void = SwigType_type(return_type) == T_VOID;
    Node *tmpl = Getattr(n, "template");

    // printNode(n, __FUNCTION__);
    if (classn) {
        String *self_type = NewStringEmpty();
        String *class_type = Getattr(classn, "classtype");

        if (SwigType_isqualifier(Getattr(n, "qualifier"))) {
            Append(self_type, "const ");
        }
        if (Len(plist) > 0)
            Printf(self_type, "%s& _self, ", class_type);
        else
            Printf(self_type, "%s& _self", class_type);

        Insert(parms_str, 0, self_type);
        func_name = Copy(Getattr(classn, "sym:name"));

        if (is_extend) {
            if (Len(plist) > 0)
                Insert(parm_names_str, 0, "&_self, ");
            else
                Insert(parm_names_str, 0, "&_self");
            String *extend_name = extendFuntionCode(n);
            Append(call_str, extend_name);
            Delete(extend_name);
        } else {
            Printf(call_str, "_self.%s", name);
        }
        Delete(self_type);
    } else {
        func_name = NewStringEmpty();
        call_str = Copy(name);
        if (tmpl) {  // fixme
            Replace(call_str, "(", "", DOH_REPLACE_FIRST);
            Replace(call_str, ")", "", DOH_REPLACE_ID_END);
        }
    }
    Printf(func_name, "_%s_%s", Getattr(n, "sym:name"),
           Getattr(n, "sym:overname"));

    if (!rtype_is_void) {
        Insert(call_str, 0, "return ");
    }

    Printf(f_overloaded_funcs, "%s %s(%s) {\n    %s(%s);\n}\n\n", return_type,
           func_name, parms_str, call_str, parm_names_str);

    Delete(call_str);
    return func_name;
}

String *EmscriptenWasm::generateGetterFunction(Node *n) {
    Node *classn = getCurrentClass();
    String* cls_name = Getattr(classn, "sym:name");
    String *pname = Getattr(n, "sym:name");
    SwigType* type = Copy(Getattr(n, "type"));

    bool is_pointer = checkHasRawPointer(type, 0);
    if (is_pointer) {
        type = SwigType_del_pointer(type);
    }
    String *class_type = Getattr(classn, "classtype");
    String *pcxx_name = Getattr(n, "name");

    String *func_name = NewStringEmpty();
    Printf(func_name, "__%s_get_%s", cls_name, pname);
    if (is_pointer) {
        Printf(f_extends, "val %s(const %s& self) {\n"
            "    if (!self.%s) return val::null();\n"
            "    return val(*(self.%s));\n}\n\n",
            func_name, class_type,
            pcxx_name, pcxx_name
            );
    } else {
        Printf(f_extends, "const %s& %s(const %s& self) {\n"
           "    return (self.%s);\n}\n\n",
           SwigType_str(type, 0), func_name, class_type, pcxx_name
          );
    }

    Delete(type);
    return func_name;
}
String *EmscriptenWasm::generateSetterFunction(Node *n) {
    Node *classn = getCurrentClass();
    String* cls_name = Getattr(classn, "sym:name");
    String *pname = Getattr(n, "sym:name");
    SwigType* type = Copy(Getattr(n, "type"));

    bool is_pointer = checkHasRawPointer(type, 0);
    if (is_pointer) {
        type = SwigType_del_pointer(type);
    }
    String *class_type = Getattr(classn, "classtype");

    String *func_name = NewStringEmpty();
    Printf(func_name, "__%s_set_%s", cls_name, pname);
    // fixme: empty pointer
    Printf(f_extends, "void %s(const %s& self, const %s& val) {\n"
           "    %s(self.%s) = val;\n}\n\n",
           func_name, class_type, SwigType_str(type, 0),
           is_pointer ? "*" :"", Getattr(n, "name")
          );

    Delete(type);
    return func_name;
}

String *EmscriptenWasm::generateOverloadedFunctionShadowCode(Node *n) {
    if (!Getattr(n, "sym:overloaded") || GetFlag(n, "__wasm_wraped")) {
        return 0;
    }

    int isstatic = getCurrentClass() && Swig_storage_isstatic(n) &&
                   !(SmartPointer && Getattr(n, "allocate:smartpointeraccess"));

    String *wasm_name = wasmFunctionName(n);
    if (getCurrentClass()) {
        if (isstatic) {
            String *name_pre = NewStringEmpty();
            Printf(name_pre, "Module.%s.",
                   Getattr(getCurrentClass(), "sym:name"));
            Insert(wasm_name, 0, name_pre);
            Delete(name_pre);
        } else {
            Insert(wasm_name, 0, "this.");
        }
    } else {
        Insert(wasm_name, 0, "Module.");
    }
    selectOverloadedFuntionShadowCode(wasm_name, Getattr(n, "parms"));

    SetFlag(n, "__wasm_wraped");
    return 0;
}

String *EmscriptenWasm::selectOverloadedFuntionShadowCode(String *sym_funcname,
                                                          ParmList *plist) {
    // int plen = Len(plist);
    // String *js_check_code = NewStringEmpty();
    String *type_arr = parmsToJsTypeStrArray(plist);

    Printf(f_overload_js, "        if (__check_arguments(arguments, %s)) {\n",
           type_arr);
    Printf(f_overload_js, "%s%s    return %s(...arguments);\n        }\n", tab4,
           tab4, sym_funcname);
    Delete(type_arr);
    return 0;
}

bool EmscriptenWasm::checkHasRawPointer(SwigType *return_type,
                                        ParmList *plist) {
    if (return_type && (SwigType_type(return_type) == T_POINTER ||
                        SwigType_type(return_type) == T_MPOINTER))
        return true;

    ParmList *p = plist;
    while (p) {
        int type = SwigType_type(Getattr(p, "type"));
        if (type == T_POINTER || type == T_MPOINTER) {
            return true;
        }
        p = nextSibling(p);
    }
    return false;
}

String *EmscriptenWasm::extendFuntionCode(Node *n) {
    Node *classn = getCurrentClass();
    ParmList *plist = Getattr(n, "parms");
    String *parms_str = paramsToStr(plist, 0);

    String *return_type = SwigType_str(Getattr(n, "type"), 0);
    String *self_type = NewStringEmpty();
    String *class_type = Getattr(classn, "classtype");

    if (SwigType_isqualifier(Getattr(n, "qualifier"))) {
        Append(self_type, "const ");
    }
    if (Len(plist) > 0)
        Printf(self_type, "%s* self, ", class_type);
    else
        Printf(self_type, "%s* self", class_type);
    Insert(parms_str, 0, self_type);
    String *func_name = NewStringEmpty();

    Printf(func_name, "extend_%s_%s_%s", Getattr(classn, "sym:name"),
           Getattr(n, "sym:name"), Getattr(n, "sym:overname"));

    Printf(f_extends, "%s %s(%s) %s\n\n", return_type, func_name, parms_str,
           Getattr(n, "code"));

    Delete(self_type);
    return func_name;
}

String *EmscriptenWasm::wasmFunctionName(Node *n) {
    String *wasm_name = Copy(Getattr(n, "sym:name"));
    if (Getattr(n, "sym:overloaded")) {
        Insert(wasm_name, 0, "__");
        Append(wasm_name, Getattr(n, "sym:overname"));
    }
    return wasm_name;
}

String *EmscriptenWasm::paramsToStr(ParmList *p, int t) {
    String *out = NewStringEmpty();
    while (p) {
        String *pstr = NewStringEmpty();
        String *type, *name;

        SwigType *st = Getattr(p, "type");

        // SwigType *st_resolve = 0;
        // if (st) st_resolve = SwigType_typedef_resolve(st);
        // st = st_resolve? st_resolve: st;
        type = SwigType_str(st ? st : NewStringEmpty(), 0);
        name = Getattr(p, "name");
        name = name ? name : NewString("tmp");

        if (t == 1) {  // type
            Append(pstr, type);
        } else if (t == 2) {  // name
            Append(pstr, name);
        } else {
            Printf(pstr, "%s %s", type, name);
        }
        // String *pstr = SwigType_str(type ? type : get_empty_type(),
        // Getattr(p, "name"));
        Append(out, pstr);
        p = nextSibling(p);
        if (p) {
            Append(out, ",");
        }
        Delete(pstr);
    }
    return out;
}

String *EmscriptenWasm::parmsToJsTypeStrArray(ParmList *plist) {
    if (!plist || Len(plist) == 0) {
        return NewString("[]");
    }

    String *arr_str = NewString("[");
    ParmList *p = plist;

    while (p) {
        SwigType *st_resolve = 0;
        SwigType *st = Getattr(p, "type");
        if (st) st_resolve = SwigType_typedef_resolve(st);
        SwigType *type = SwigType_base(st_resolve ? st_resolve : st);

        switch (SwigType_type(type)) {
            case T_STRING:
            case T_WSTRING:
                Append(arr_str, "\"string\"");
                break;
            case T_INT:
            case T_UINT:
            case T_SHORT:
            case T_USHORT:
            case T_LONG:
            case T_ULONG:
            case T_LONGLONG:
            case T_ULONGLONG:
                Append(arr_str, "\"int\"");
                break;
            case T_FLOAT:
            case T_DOUBLE:
            case T_LONGDOUBLE:
            case T_NUMERIC:
                Append(arr_str, "\"float\"");
                break;
            case T_FLTCPLX:
            case T_DBLCPLX:
                Append(arr_str, "\"complex\"");
                break;
            case T_CHAR:
            case T_WCHAR:
                Append(arr_str, "\"char\"");
                break;
            case T_BOOL:
                Append(arr_str, "\"boolean\"");
                break;
            case T_USER:
            default:
                Printf(arr_str, "\"%s\"",
                       SwigType_str(type ? type : NewStringEmpty(), 0));
                break;
        }

        p = nextSibling(p);
        if (p) {
            Append(arr_str, ",");
        }
    }
    Append(arr_str, "]");

    return arr_str;
}

/* -----------------------------------------------------------------------------
 * Template::Template() :  creates a Template class for given template code
 * -----------------------------------------------------------------------------
 */

Template::Template(const String *code_) {
    if (!code_) {
        Printf(stdout, "Template code was null. Illegal input for template.");
        SWIG_exit(EXIT_FAILURE);
    }
    code = NewString(code_);
    templateName = NewString("");
}

Template::Template(const String *code_, const String *templateName_) {
    if (!code_) {
        Printf(stdout, "Template code was null. Illegal input for template.");
        SWIG_exit(EXIT_FAILURE);
    }

    code = NewString(code_);
    templateName = NewString(templateName_);
}

/* -----------------------------------------------------------------------------
 * Template::~Template() :  cleans up of Template.
 * -----------------------------------------------------------------------------
 */

Template::~Template() {
    Delete(code);
    Delete(templateName);
}

/* -----------------------------------------------------------------------------
 * String* Template::str() :  retrieves the current content of the template.
 * -----------------------------------------------------------------------------
 */

String *Template::str() { return code; }

Template &Template::trim() {
    const char *str = Char(code);
    if (str == 0) return *this;

    int length = Len(code);
    if (length == 0) return *this;

    int idx;
    for (idx = 0; idx < length; ++idx) {
        if (str[idx] != ' ' && str[idx] != '\t' && str[idx] != '\r' &&
            str[idx] != '\n')
            break;
    }
    int start_pos = idx;

    for (idx = length - 1; idx >= start_pos; --idx) {
        if (str[idx] != ' ' && str[idx] != '\t' && str[idx] != '\r' &&
            str[idx] != '\n')
            break;
    }
    int end_pos = idx;

    int new_length = end_pos - start_pos + 1;
    char *newstr = new char[new_length + 1];
    memcpy(newstr, str + start_pos, new_length);
    newstr[new_length] = 0;

    Delete(code);
    code = NewString(newstr);
    delete[] newstr;

    return *this;
}

Template &Template::replace(const String *pattern, const String *repl) {
    Replaceall(code, pattern, repl);
    return *this;
}

Template &Template::print(DOH *doh) {
    Printv(doh, str(), 0);
    return *this;
}

Template &Template::pretty_print(DOH *doh) {
    Wrapper_pretty_print(str(), doh);
    return *this;
}

Template::Template(const Template &t) {
    code = NewString(t.code);
    templateName = NewString(t.templateName);
}

void Template::operator=(const Template &t) {
    Delete(code);
    Delete(templateName);
    code = NewString(t.code);
    templateName = NewString(t.templateName);
}
}  // namespace wasm

static Language *new_swig_emwasm() { return new wasm::EmscriptenWasm(); }
extern "C" Language *swig_emwasm(void) { return new_swig_emwasm(); }