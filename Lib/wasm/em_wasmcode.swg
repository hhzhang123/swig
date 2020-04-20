/*%fragment ("emwasm_module_begin", "templates")
%{
EMSCRIPTEN_BINDINGS($module) {
%}

%fragment ("emwasm_module_end", "templates")
%{
};
%}*/



%fragment ("emwasm_class", "templates")
%{
    class_<$class_type>("$wasm_classname")
%}


%fragment ("emwasm_class_with_base", "templates")
%{
    class_<$class_type, base<$base_class>>("$wasm_classname")
%}

%fragment ("emwasm_function", "templates")
%{
    function("$name", &$func)
%}

%fragment ("emwasm_class_property", "templates")
%{
    .property("$name", &$value)
%}

%fragment ("emwasm_class_static_property", "templates")
%{
    .class_property("$name", &$value)
%}

%fragment ("emwasm_class_constructor", "templates")
%{
    .constructor<$param_types>()
%}

%fragment ("emwasm_vector", "templates")
%{
    register_vector<$type>("$name");
%}

%fragment ("emwasm_map", "templates")
%{
    register_map<$key_type, $value_type>("$name");
%}


