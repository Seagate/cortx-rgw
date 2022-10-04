#!/bin/sh
set -x

GIT_ENUM_STATES=(UP_TO_DATE PULL_NEEDED PUSH_NEEDED DIVERGED)
tam=${#GIT_ENUM_STATES[@]}
for ((i=0; i < $tam; i++)); do
    name=${GIT_ENUM_STATES[i]}
    declare -r ${name}=$i
done

function git_test()
{
    LOCAL=$(git rev-parse @)
    REMOTE=$(git rev-parse "@{u}")
    BASE=$(git merge-base @ "@{u}")

    if [ $LOCAL = $REMOTE ]; then
        return $UP_TO_DATE    # echo "Up-to-date"
    elif [ $LOCAL = $BASE ]; then
        return $PULL_NEEDED    # echo "Need to pull"
    elif [ $REMOTE = $BASE ]; then
        return $PUSH_NEEDED    # echo "Need to push"
    else
        return $DIVERGED    # echo "Diverged"
    fi
}

git_test
RESULT=$?
echo "Result is $RESULT"
echo "${#GIT_ENUM_STATES[@]}"
echo "${GIT_ENUM_STATES[@]}"
case $RESULT in
    $UP_TO_DATE)
        echo "Up-to-date"
        ;;
    $PULL_NEEDED)
        echo "Need to pull"
        ;;
    $PUSH_NEEDED)
        echo "Need to push"
        ;;
    $DIVERGED)
        echo "Diverged"
        ;;
esac
