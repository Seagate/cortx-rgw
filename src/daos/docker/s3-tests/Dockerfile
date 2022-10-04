# Copyright 2022 Seagate Corporation
# All rights reserved.
#
# 'recipe' for Docker to build an image of radosgw + DAOS + s3-tests
#

# Pull base image
ARG BASE_DISTRO=dgw-single-host:latest
FROM $BASE_DISTRO
LABEL maintainer="seagate-daos@seagate.com"

# get s3-tests source
RUN sudo dnf install python3 virtualenv jq crudini s3cmd bc -y
RUN cd /opt && sudo mkdir -p s3-tests && sudo chmod 777 s3-tests && git clone https://github.com/Seagate/s3-tests

# add rocky to the distro
RUN sed -i 's/ centos|fedora|rhel/ rocky|centos|fedora|rhel/' /opt/s3-tests/bootstrap 
RUN cd /opt/s3-tests && ./bootstrap
ENV S3TESTS_PATH=/opt/s3-tests
