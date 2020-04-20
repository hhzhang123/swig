%fragment ("emwasm_js_checktype", "templates")
%{
    function __check_number(input, dest) {
        if (Number.isInteger(input)) {
            if (dest != "int" && dest != "float" && dest != "size_t")
                return false;
        } else if ("float" != dest) {
            return false;
        }
        return true;
    }
    
    function __check_arguments(input_args, dest_args) {
        if (input_args.length != dest_args.length) {
            return false;
        }
        for (let i = 0; i < input_args.length; i++) {
            let in_type = typeof input_args[i];
            if (in_type === "object") {
                if (typeof(input_args[i].value) == "number") {
                    if (!__check_number(input_args[i].value, dest_args[i])){
                        return false;
                    }
                } else if (input_args[i].constructor.__type != dest_args[i]) {
                    return false;
                }
            } 
            else if (in_type === "string") {
                if (dest_args[i] != "string" && dest_args[i] != "std::string") {
                    return false;
                }
            }
            else if (in_type === "number") {
                if(!__check_number(input_args[i], dest_args[i])){
                    return false;
                }
            } else if (in_type == "boolean") {
                if ("boolean" != dest_args[i]) {
                    return false;
                }
            } else {
                return false;
            }
        }
        return true;
    }
%}