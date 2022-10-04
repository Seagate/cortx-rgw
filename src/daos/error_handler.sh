#!/bin/bash
# parameters: $? SCRIPT_NAME FUNCNAME LINENO

function error_handler()
{
    return_code=$1
    script_name=$2
    declare -n function_names=$3
    declare -n line_number=$4
    if [[ ! $return_code == 0 ]]; then
        echo -e "Failed with return code $return_code at $script_name:$line_number\nStack trace:\n\t${function_names[@]/ /\n\t}"
        exit 1
    fi
}
