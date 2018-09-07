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
#!/bin/bash -x

set -x

TIMESTAMP=$(date +%Y-%m-%d_%H-%M-%S)
log_file="/tmp/build_log.txt-$TIMESTAMP"
exec 1> >(tee -a "$log_file")  2>&1

# Get Command Line arguements if present
if [ "$1" == "from-base-build" ]; then
    WAS_DMM_BUILT="YES"
fi

if [ "x$1" != "x" ] && [ "$1" != "from-base-build" ]; then
    TEMP_DIR=$1
    DMM_BUILD_SCRIPT_DIR=${TEMP_DIR}/scripts
    LWIP_BUILD_DIR=${TEMP_DIR}/stacks/lwip_stack/build/
else
    TEMP_DIR=`dirname $(readlink -f $0)`/..
    DMM_BUILD_SCRIPT_DIR=${TEMP_DIR}/../../scripts
    LWIP_BUILD_DIR=${TEMP_DIR}/build/
fi

echo 0:$0
echo 1:$1
echo 2:$2
echo TEMP_DIR: $TEMP_DIR
echo DMM_BUILD_SCRIPT_DIR: $DMM_BUILD_SCRIPT_DIR
echo LWIP_BUILD_DIR: $LWIP_BUILD_DIR

if [ 'x$WAS_DMM_BUILT' !=  "xYES" ]; then
    bash -x $DMM_BUILD_SCRIPT_DIR/build.sh
fi

#===========build LWIP===========
echo "LWIP build started....."
cd $LWIP_BUILD_DIR
cmake ..
make -j 8
if [ $? -eq 0 ]; then
    echo "LWIP build is SUCCESS"
else
    echo "LWIP build has FAILED"
    exit 1
fi
