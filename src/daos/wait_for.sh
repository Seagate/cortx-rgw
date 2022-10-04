#!/bin/bash

# parameter 1: number of seconds the timeout waits
# parameter 2: function that returns 0 when the condition has been satisfied
# parameter 3: message to be displayed upon timeout
function wait_for()
{
    local timeout_limit=$1
    local counter=0
    $2
    while [[ ! $? == 0 ]]; do
        if [ $counter -ge $timeout_limit ]; then
            echo "Timeout: $3"
            exit 1
        fi
        sleep 1
        $2
        ((counter++))
    done
}

