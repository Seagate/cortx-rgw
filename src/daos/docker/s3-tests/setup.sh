#!/bin/bash

# create conf file from s3tests.conf.SAMPLE
# prepare the tests

source $CEPH_PATH/src/daos/silent_pushd_popd.sh

user_file=$(mktemp /tmp/john.doe.XXXXXXXXX.json)

pushd ${CEPH_PATH}/build
sudo bin/radosgw-admin user create --email johndoe@dgw.com --uid johndoe --display-name John-Doe --no-mon-config > $user_file
if [ $? -ne 0 ]; then
    sudo bin/radosgw-admin user info --uid johndoe > $user_file
    if [ $? -ne 0 ]; then
        echo "failed to get info for johndoe"
        exit 1
    fi
fi
popd

# the rest of the command should cause a failure of the script
set -e
trap 'echo "Failed: $BASH_COMMAND' ERR

pushd $S3TESTS_PATH
sudo chmod 666 $user_file
# remove all lines that start with 4 digits (log lines from the admin tool)
# and lines that contain "dfs  ERR"
sed -i '/^\d{4}/d; /dfs  ERR  /d' $user_file

trim_quotes()
{
    echo $1 | sed -e 's/^[" \t]*//;s/[" \t]*$//'
}

user_name=$(trim_quotes `jq '.keys[0].user' $user_file`)
access_key=$(trim_quotes `jq '.keys[0].access_key' $user_file`)
secret_key=$(trim_quotes `jq '.keys[0].secret_key' $user_file`)
email=$(trim_quotes `jq .email $user_file`)

echo "access_key = $access_key" > ~/.s3cfg
echo "secret_key = $secret_key" >> ~/.s3cfg
echo "host_base = localhost:8000" >> ~/.s3cfg
echo "host_bucket = %(bucket)localhost" >> ~/.s3cfg
echo "use_https = True" >> ~/.s3cfg

pwd
cp s3tests.conf.SAMPLE s3tests.conf
sudo chmod 777 s3tests.conf
crudini --set s3tests.conf fixtures 'bucket prefix' testbucket3-{random}
# crudini --set s3tests.conf 's3 main' display_name $name
crudini --set s3tests.conf 's3 main' user_id $user_name
crudini --set s3tests.conf 's3 main' email $email
crudini --set s3tests.conf 's3 main' access_key $access_key
crudini --set s3tests.conf 's3 main' secret_key $secret_key

crudini --set s3tests.conf 's3 alt' access_key $access_key
crudini --set s3tests.conf 's3 alt' secret_key $secret_key

crudini --set s3tests.conf 's3 tenant' access_key $access_key
crudini --set s3tests.conf 's3 tenant' secret_key $secret_key

crudini --set s3tests.conf 's3 cloud' access_key $access_key
crudini --set s3tests.conf 's3 cloud' secret_key $secret_key

crudini --set s3tests.conf 'iam' access_key $access_key
crudini --set s3tests.conf 'iam' secret_key $secret_key

s3cmd --no-ssl mb s3://testbucket3
popd
