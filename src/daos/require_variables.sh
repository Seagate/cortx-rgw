#!/bin/bash

require_variables()
{
    while (( $# )); do
        declare -n foo=$1
        if [ -z ${foo+x} ]; then
            echo "variable '$1' needs to be set";
            echo "call stack: ${FUNCNAME[@]}"
            exit 1
        fi
        shift
    done
}
