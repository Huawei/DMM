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

DPDK_DOWNLOAD_PATH=/tmp/dpdk
ifname=enp0s8

if [ "x$1" != "x" ] && [ "$1" != "from-base-build" ]; then
    TEMP_DIR=$1
    DMM_BUILD_SCRIPT_DIR=${TEMP_DIR}/scripts
    LWIP_BUILD_DIR=${TEMP_DIR}/stacks/lwip_stack/build/
else
    TEMP_DIR=`dirname $(readlink -f $0)`/..
    DMM_BUILD_SCRIPT_DIR=${TEMP_DIR}/../../scripts
    LWIP_BUILD_DIR=${TEMP_DIR}/build/
fi

LIB_PATH=$LWIP_BUILD_DIR/../release/lib64/

echo 0:$0
echo 1:$1
echo 2:$2
echo TEMP_DIR: $TEMP_DIR
echo DMM_BUILD_SCRIPT_DIR: $DMM_BUILD_SCRIPT_DIR
echo LWIP_BUILD_DIR: $LWIP_BUILD_DIR
echo LIB_PATH: $LIB_PATH

#===========check hugepages=================
source $DMM_BUILD_SCRIPT_DIR/check_hugepage.sh

OS_ID=$(grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')
if [ "$OS_ID" == "centos" ]; then
    ifaddress1=$(ifconfig $ifname | grep 'inet' | cut -d: -f2 | awk '{print $2}')
    echo $ifaddress1
    ifaddresscut=$(ifconfig $ifname | grep 'inet' | head -n 1 | awk -F " " '{print $2}' | awk -F "." '{print $1"."$2"."$3}')
    echo $ifaddresscut
    ifmac=$(ifconfig $ifname | grep 'ether' | awk -F " " '{print $2}')
    echo $ifmac
elif [ "$OS_ID" == "ubuntu" ]; then
    ifaddress1=$(ifconfig $ifname | grep 'inet' | head -n 1 | cut -d: -f2 | awk '{print $1}')
    echo $ifaddress1
    ifaddresscut=$(ifconfig $ifname | grep 'inet' | head -n 1 | cut -d: -f2 | awk '{print $1}' | awk -F "." '{print $1"."$2"."$3}')
    echo $ifaddresscut
    ifmac=$(ifconfig $ifname | grep 'HWaddr' | awk -F " " '{print $5}')
    echo $ifmac
fi

cd $LWIP_BUILD_DIR/../
cd release/script
sed -i 's!DPDK_INSTALL_PATH=.*!DPDK_INSTALL_PATH='$DPDK_DOWNLOAD_PATH'/dpdk-18.02!1' nstack_var.sh

cd ../
chmod 775 *
cp ./configure/*.json bin/
cd bin

if [ "$OS_ID" == "centos" ]; then
    sed -i 's!eth7!'$ifname'!1' ip_data.json
elif [ "$OS_ID" == "ubuntu" ]; then
    sed -i 's!eth7!'$ifname'!1' ip_data.json
fi

sed -i 's!00:54:32:19:3d:19!'$ifmac'!1' ip_data.json
sed -i 's!192.168.1.207!'$ifaddress1'!1' ip_data.json

sed -i 's!192.168.1.1!'$ifaddresscut'.0!1' network_data_tonStack.json
sed -i 's!192.168.1.254!'$ifaddresscut'.1!1' network_data_tonStack.json
sed -i 's!192.168.1.098!'$ifaddresscut'.5!1' network_data_tonStack.json
sed -i 's!192.168.1.209!'$ifaddresscut'.254!1' network_data_tonStack.json
sed -i 's!192.168.1.0!'$ifaddresscut'.0!1' network_data_tonStack.json
sed -i 's!192.168.1.254!'$ifaddresscut'.1!1' network_data_tonStack.json

if [ "$OS_ID" == "centos" ]; then
    sed -i 's!eth7!'$ifname'!1' network_data_tonStack.json
elif [ "$OS_ID" == "ubuntu" ]; then
    sed -i 's!eth7!'$ifname'!1' network_data_tonStack.json
fi
sed -i 's!eth7!'$ifname'!1' network_data_tonStack.json

cd $DMM_BUILD_SCRIPT_DIR/../release/bin
cp -r . ../../stacks/lwip_stack/app_test
cd $DMM_BUILD_SCRIPT_DIR/../stacks/lwip_stack/app_test
cp -r ../app_conf/*.json .

sed -i 's!192.168.1.1!'$ifaddresscut'.0!1' rd_config.json

sudo mkdir -p /var/run/ip_module/
sudo mkdir -p /var/log/nStack/ip_module/

export LD_LIBRARY_PATH=$LIB_PATH
export NSTACK_LOG_ON=DBG

cd $LWIP_BUILD_DIR/../release
bash -x ./stop_nstack.sh
bash -x ./start_nstack.sh
check_result=$(pgrep nStackMain)
if [ -z "$check_result"  ]; then
    echo "nStackMain execute failed"
    exit 1
else
    echo "nStackMain execute successful"
    exit 0
fi
