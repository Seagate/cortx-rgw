#!/bin/bash
# 

source $CEPH_PATH/src/daos/color_output.sh
source $CEPH_PATH/src/daos/set_boolean.sh
source $CEPH_PATH/src/daos/set_integer.sh
source $CEPH_PATH/src/daos/daos/restart_daos.sh
source $CEPH_PATH/src/daos/radosgw/radosgw_run_state.sh
source $CEPH_PATH/src/daos/silent_pushd_popd.sh
source $CEPH_PATH/src/daos/docker/s3-tests/test_results.sh
source $CEPH_PATH/src/daos/radosgw/radosgw_start.sh
source $CEPH_PATH/src/daos/daos/initialize_daos_bin.sh

if [[ ! "$CEPH_PATH" =~ . ]];
then
    CEPH_PATH=/opt/ceph
fi

function usage()
{
    # turn off echo
    set +x

    require_variables BOOLEAN_VALUES
    echo ""
    echo "./run_tests.sh"
    echo -e "\t-h --help"
    echo -e "\t-y --summary=$BOOLEAN_VALUES default=FALSE"
    echo -e "\t\tSummarize the previous run of these tests"
    echo -e "\t-a --artifacts-folder=<folder> default=/opt"
    echo -e "\t\tFolder to store test artifacts"
    echo -e "\t-r --run-on-test-result=$TEST_RESULT_VALUES"
    echo -e "\t\tRe-run the tests that resulted in the test result"
    echo -e "\t-s --stop-on-test-result=$TEST_RESULT_VALUES"
    echo -e "\t\tStop running the rest of the tests when enountering the test result"
    echo -e "\t-c --cleandaos=$BOOLEAN_VALUES default=TRUE"
    echo -e "\t\tClean out the DAOS pool when restarting"
    echo -e "\t--restart=$ACCEPTABLE_INTEGER_REGEX default=0"
    echo -e "\t\tRestart radosgw/DAOS after the number of tests have run"
    echo -e "\t--start=$ACCEPTABLE_INTEGER_REGEX default=0"
    echo -e "\t\tStart the test run at numbered test"
    echo -e "\t--end=$ACCEPTABLE_INTEGER_REGEX default=1000000"
    echo -e "\t\tEnd the test run at numbered test"
    echo -e "\t--debug-script=$BOOLEAN_VALUES default=false"
    echo -e "\t\tSkip execution of each test"
    echo -e "\t--color-output=$BOOLEAN_VALUES default=TRUE"
    echo -e "\t\tShow ANSI color codes for status/execution time"
    echo -e "\t--verbose"
    echo -e "\t\tEcho commands"
    echo ""
}

declare -A run_on_test_result
declare -A stop_on_test_result

summary=false
skipped=0
start_count=0
end_count=1000000
restart_count=0
clean_daos=true
color_output=true
ARTIFACTS_FOLDER=/opt
COMMAND_PREFIX='sudo'
DEBUG_SCRIPT=false;

while (( $# ))
    do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -h | --help)
            usage
            exit 0
            ;;
        -y | --summary)
            set_boolean summary $VALUE
            echo "summarize results without running tests"
            ;;
        -a | --artifacts-folder)
            ARTIFACTS_FOLDER=$VALUE
            if [[ ! -d $ARTIFACTS_FOLDER ]]; then
                sudo mkdir $ARTIFACTS_FOLDER
                sudo chmod 777 $ARTIFACTS_FOLDER
            fi
            ;;
        -r | --run-on-test-result)
            set_test_results run_on_test_result $VALUE
            ;;
        -s | --stop-on-test-result)
            set_test_results stop_on_test_result $VALUE
            ;;
        -c | --cleandaos)
            set_boolean clean_daos $VALUE
            ;;
        --restart)
            set_integer restart_count $VALUE
            ;;
        --start)
            set_integer start_count $VALUE
            ;;
        --end)
            set_integer end_count $VALUE
            ;;
        --color-output)
            set_boolean color_output $VALUE
            ;;
        --debug-script)
            set_boolean DEBUG_SCRIPT $VALUE
            ;;
        --verbose)
            set -x
            ;;
        *)
            echo "Unknown option $1"
            usage
            exit 1
            ;;
    esac
    shift
done

input="$ARTIFACTS_FOLDER/test_list.txt"
csv_output="$ARTIFACTS_FOLDER/test_output.csv"
declare -A result_summary
csv_summary="$ARTIFACTS_FOLDER/test_summary.csv"
csv_diff="$ARTIFACTS_FOLDER/test_diff.csv"
test_count=0
failed=0
working_folder=`pwd`

if [[ ! -e $input ]];
then
    echo "Creating lists file: $input"
    pushd ${S3TESTS_PATH}
    S3TEST_CONF=s3tests.conf virtualenv/bin/nosetests -v --collect-only 2> $input
    popd
fi

echo "Test name,Results,Count,Time" > $csv_output

check_run_results()
{
    if [[ -e $1 ]];
    then
        # trim empty lines, get the last line
        grep "." $1 | tail -n 1
    else
        echo 'MISSING'
    fi
}

extract_test_count()
{
    # Ran 0 tests in 303.605s
    # Ran 1 test in 23.321s
    local testcount=''
    if [[ -e $1 ]];
    then
        local grep_out=`grep 'Ran [0-9]* tests* in [0-9\.s]*' $1`
        testcount=`echo $grep_out | sed -E 's/Ran ([0-9]*) tests* in ([0-9\.s]*)/\1,\2/'`
    fi
    if [[ $testcount == '' ]];
    then
        testcount='MISSING,MISSING'
    fi
    echo $testcount
}

check_test_result()
{
    if [[ -e $1 ]];
    then
        local test_status=`grep '\.\.\.' $1`
        local result=`echo $test_status | sed -E 's/(^.*) \.\.\. (.*$)/\2/'`
        if [[ $result == '' ]];
        then
            # no results found, check the last line of the file for a test result
            # result=$(check_run_results $1)
            result='MISSING'
        else
            if [[ $result =~ ' ' ]];
            then
                # the debug output is getting in the way of the test result.
                # Find the first test_results_enum followed by EOL
                local grep_pattern=`echo "$test_results_enum\$" | sed 's/|/\$\\|/g' `
                result=`grep -o -e "$grep_pattern" $1`
                if [[ $result == '' ]];
                then
                    result='MISSING'
                fi
            fi
        fi
        echo $result
    else
        echo 'MISSING'
    fi
}

attempt_restart()
{
    if [[ $summary == false ]]; then
        echo "attempting restart"
        index=1
        rados_restart=5
        while [ $index -le $rados_restart ]; do
            ((index++))
            echo "attempt to retart radosgw"
            pushd ${CEPH_PATH}/build
            sudo ../src/stop.sh
            popd
            sudo rm -rf ${CEPH_PATH}/build/out/* /tmp/*
            local result=0
            if [[ $clean_daos == true ]]; then
                restart_daos
                result=$?
            fi
            if [[ ! $result == 0 ]]; then
                echo "daos restart failed"
            else
                radosgw_start
                isRadosgwRunning
                if [[ $? == 1 ]]; then
                    sh $CEPH_PATH/src/daos/docker/s3-tests/setup.sh
                    if [[ $? == 0 ]]; then return; fi
                fi
            fi
        done
        echo "Failed to restart radosgw after $rados_restart attempts"
        exit 1
    fi
}

isTestScheduledToRun()
{
    # check if the count is in range of the requested tests
    if [ $test_count -lt $start_count ] || [ $test_count -gt $end_count ];
    then
        return 0
    fi
    # check if the requested test result is empty or the test result matches the requested test result
    if [ ${#stop_on_test_result[@]} -eq 0 ] || [[ " ${stop_on_test_result[@]} " =~ " ${testresult} " ]];
    then
        return 1
    fi
    return 0
}

checkRestartNeeded()
{
    if [[ $summary == false ]] && [ $restart_count -gt 0 ]; then
        local execution_count=$(($test_count-$skipped-1))
        local mod=$(expr $execution_count % $restart_count)
        if [ $execution_count -gt 0 ] && [ $mod -eq 0 ]; then
            attempt_restart
            echo "Restarted radosgw..."
        fi
    fi
}

run_test()
{
    testname=$1
    testfile=$2
    # replace the last period with a colon
    testcommand=`echo "$testname" | sed -r "s/(.*)\.([^\.]+)/\1:\2/"`
    testresult=''
    testresult=$(check_test_result $testfile)
    checkRestartNeeded
    echo "count=$test_count skipped=$skipped $testname"
    isRadosgwRunning
    if [[ $? == 1 ]]; then
        isTestScheduledToRun
        if [[ $? == 1 ]]; then
            if [[ $summary == false ]]; then
                if [[ $DEBUG_SCRIPT == false ]]; then
                    echo "S3TEST_CONF=s3tests.conf virtualenv/bin/nosetests -v $testcommand" > $testfile
                    S3TEST_CONF=s3tests.conf virtualenv/bin/nosetests -v $testcommand 2>> $testfile
                else
                    # debugging script, don't execute tests
                    echo "$testname ... ok" >> $testfile
                    # read -p "Paused: DEBUG_SCRIPT == true"
                fi
            else
                ((skipped++))
            fi
            testresult=''
            isRadosgwRunning
            if [[ $? == 1 ]]; then
                testresult=$(check_test_result $testfile)
                if [[ ! $testresult == 'ok' ]];
                then
                    ((failed++))
                fi
            else
                attempt_restart
                testresult='CRASHED'
                ((failed++))
                echo "$testname ... $testresult" >> $testfile
            fi
        else
            ((skipped++))
        fi
    else
        if [[ ! $testresult == 'ok' ]]; then
            echo "S3TEST_CONF=s3tests.conf virtualenv/bin/nosetests -v $testcommand" >> $testfile
            echo "$testname ... NOT_RUNNING" >> $testfile
        fi
        attempt_restart
        testresult='NOT_RUNNING'
        ((skipped++))
    fi
    if [[ $testresult == '' ]];
    then
        echo "Test failed - testresult should never be empty: $testfile"
        exit 1
    fi
    count_time=$(extract_test_count $testfile)
    RESULTCOLOR=$(get_status_color $testresult)
    execution_time=`echo $count_time | sed "s/^.*,\(.*\)$/\1/"`
    EXECUTIONTIMECOLOR=$(get_execution_time_color $execution_time)
    execution_count=`echo $count_time | sed "s/^\(.*\),.*$/\1/"`
    COUNTCOLOR=$(get_count_color $execution_count)
    RESTORECOLOR=$(get_restore_color)

    echo -e " ${COUNTCOLOR}count=$execution_count${RESTORECOLOR} ${EXECUTIONTIMECOLOR}time=$execution_time${RESTORECOLOR} ${RESULTCOLOR}result=$testresult${RESTORECOLOR} $testfile"
    if [[ $count_time == '' ]];
    then
        echo "Test failed - count_time should never be empty: $testfile"
        exit 1
    fi
    echo "$testname,$testresult,$count_time" >> $csv_output
    ((result_summary[$testresult]++))

    # check for a stop_on_test_result test result
    if [ ${#stop_on_test_result[@]} -ne 0 ] && [[ " ${stop_on_test_result[@]} " =~ " ${testresult} " ]]; then
        echo "stop_on_test_result test result $testresult found, stopping, summarizing..."
        summarize
        exit 1
    fi
}

get_test_filename()
{
    echo "$ARTIFACTS_FOLDER/test_results/${1}.txt"
}

if [[ ! -d $ARTIFACTS_FOLDER/test_results ]];
then
    mkdir $ARTIFACTS_FOLDER/test_results
fi

create_test_array()
{
    local counter=0
    while IFS= read -r line
    do
        if [[ $line =~ ok$ ]];
        then
            if [[ ! $line =~ \(.*\) ]];
            then
                # strip the space... to the end of the line
                local testname=`echo "$line" | sed -e "s/ .*$//"`
                test_list[$counter]=${testname}
                ((counter++))
            fi
        fi
    done < "$1"
}

test_one()
{
    local test_file=$(get_test_filename $1)
    ((test_count++))
    run_test $1 $test_file
}

test_each()
{
    for test_name in "${test_list[@]}"
    do
        test_one ${test_name}
    done
}

summarize()
{
    set +x
    local total_tests=${#test_list[@]}
    if [[ $total_tests == 0 ]]; then
        echo "Summarize failed due to test results missing"
        return
    fi
    local summary_title="Date|Host|$test_results_enum|Total"
    local csv_line=''
    local csv_title=`echo $summary_title | sed 's/|/,/g'`
    echo $csv_title > $csv_summary
    OLD_IFS=$IFS
    IFS=,
    for el in $csv_title; do
        case $el in
            Date)
                csv_line=`date +%m-%d-%Y`
                ;;
            Total)
                csv_line="$csv_line,$total_tests"
                ;;
            Host)
                csv_line="$csv_line,$HOSTNAME"
                ;;
            *)
                local result_count=${result_summary[$el]}
                if [[ $result_count == '' ]];
                then
                    result_count=0
                fi
                local math_calc="$result_count * 100 / $total_tests"
                local percentage=`bc <<< "scale=1; $math_calc"`
                csv_line="$csv_line,$result_count ($percentage%)"
                ;;
        esac
    done
    IFS=$OLD_IFS
    echo "$csv_line" >> $csv_summary
    # trim the Time column off so diffs are possible
    sed -e 's/,[0-9\.]*s$//' < $csv_output > $csv_diff
}

run_all_tests()
{
    initialize_daos_bin
    pushd ${S3TESTS_PATH}
    create_test_array $input
    test_each

    echo "Execution completed with $failed tests failing"
    summarize
    popd
}
run_all_tests
