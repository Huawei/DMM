#!/bin/bash -x
#########################################################################
# Copyright (c) 2018 Huawei Technologies Co.,Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#########################################################################

set -x

TIMESTAMP=$(date +%Y-%m-%d_%H-%M-%S)
log_file="/tmp/build_log.txt-$TIMESTAMP"
exec 1> >(tee -a "$log_file")  2>&1

# Get Command Line arguements if present
DMM_DIR=$1
if [ "$1" == "all" ]; then
    BUILD_ALL="YES"
fi

if [ "x$1" != "x" ] && [ "$1" != "all" ]; then
    DMM_DIR=$1
else
    DMM_DIR=`dirname $(readlink -f $0)`/../
fi

echo 0:$0
echo 1:$1
echo 2:$2
echo DMM_DIR: $DMM_DIR

BUILD_DIR=${DMM_DIR}/build

#print os information
bash -x $DMM_DIR/scripts/print_os_info.sh

# add inherited proxy for sudo user
LINE='Defaults env_keep += "ftp_proxy http_proxy https_proxy no_proxy"'
FILE=/etc/sudoers
grep -qF -- "$LINE" "$FILE" || sudo echo "$LINE" >> "$FILE"

#set and check the environment for Linux
bash -x $DMM_DIR/scripts/build_dmm_dep.sh || exit 1

#===========build DPDK================
bash -x $DMM_DIR/scripts/build_dpdk.sh || exit 1

#===========build DMM=================
bash -x $DMM_DIR/scripts/compile_dmm.sh || exit 1

if [ "${BUILD_ALL}" == "YES" ]; then
    #===========build LWIP================
    bash -x $DMM_DIR/stacks/lwip_stack/vagrant/build.sh "from-base-build" || exit 1
    #============build rsocket============================
    bash -x $DMM_DIR/scripts/build_rsocket.sh || exit 1
    #=======other new stacks build can be called from here
    echo "SUCCESSFULLY built all the stacks"
fi
