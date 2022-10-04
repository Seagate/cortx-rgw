#!/bin/bash

ACCEPTABLE_INTEGER_REGEX='[0-9]+'

function set_integer()
{
    declare -n foo=$1
    local not_used=`grep -E "^${ACCEPTABLE_INTEGER_REGEX}$" <<< $2`
    if [[ $? == 0 ]]; then
        foo=$2
    else
        echo "set_integer received non-digit results: $2"
        exit 1
    fi
}
