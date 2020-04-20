%include "em_wasmcode.swg"
%include "em_wasm_js.swg"

%fragment ("emwasm_include_header", "templates")
%{

#include "emscripten.h"
#include <emscripten/bind.h>
using namespace emscripten;

%}