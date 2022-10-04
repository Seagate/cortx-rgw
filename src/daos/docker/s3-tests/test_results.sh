#!/bin/bash

#available status codes
test_results_enum="ok|FAIL|ERROR|SKIP|MISSING|NOT_RUNNING|CRASHED"
TEST_RESULT_VALUES="$test_results_enum (multiple comma separated values)"

function set_test_results()
{
    declare -n foo=$1
    local input="|$test_results_enum|"
    local OLD_IFS=$IFS
    IFS=,
    for el in $2; do
        local found=`echo $input | grep "|$el|"`
        if [[ ! $found == $input ]];
        then
            echo "Status $el not recognized, use one or more of $test_results_enum separated by commas"
            exit 1
        fi
        foo[$el]=$el
    done
    IFS=$OLD_IFS
}
