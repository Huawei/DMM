#########################################################################
#
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
#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR=${DIR}/../build
LIB_PATH=${DIR}/../release/lib64

#DPDK download path
DPDK_DOWNLOAD_PATH=/root/dpdk

#dpdk installation path
DPDK_INSTALL_PATH=/root/dpdk_install/tmp

#set and check the environment for Ubuntu
#set env
apt-get install git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump

#check env
isInFile=$(cat /etc/default/grub | grep -c "default_hugepagesz=1G hugepagesz=1G hugepages=8")
if [ $isInFile -eq 0 ]; then
  echo "hugepage need to be set, set it by doing"
  echo "1. vi /etc/default/grub "
  echo "2. add the line GRUB_CMDLINE_LINUX_DEFAULT="default_hugepagesz=1G hugepagesz=1G hugepages=8" "
  echo "3. perform " update-grub " "
  echo "4. reboot"
  exit
else
    echo "hugepage has been set already....."
fi

#DPDK will be having dependancy on linux headers
apt-get install git build-essential linux-headers-`uname -r`

hugepageTotal=$(cat /proc/meminfo | grep -c "HugePages_Total:       0")
if [ $hugepageTotal -ne 0 ]; then
  echo "HugePages_Total is zero"
  exit
fi

hugepageFree=$(cat /proc/meminfo | grep -c "HugePages_Free:        0")
if [ $hugepageFree -ne 0 ]; then
  echo "HugePages_Free is zero"
  exit
fi

hugepageSize=$(cat /proc/meminfo | grep -c "Hugepagesize:          0 kB")
if [ $hugepageSize -ne 0 ]; then
  echo "Hugepagesize is zero"
  exit
fi

pdpe1gbFlag=$(cat /proc/cpuinfo | grep -c "pdpe1gb")
if [ $pdpe1gbFlag -eq 0 ]; then
  echo "/proc/cpuinfo doesn't include pdpe1gb to make hugepage work"
  exit
fi

mkdir /mnt/nstackhuge -p
mount -t hugetlbfs -o pagesize=1G none /mnt/nstackhuge/

mkdir -p /var/run/ip_module/

#===========build DPDK================
if [ -d $DPDK_INSTALL_PATH ]; then
  rm -rf $DPDK_INSTALL_PATH
fi

mkdir -p $DPDK_DOWNLOAD_PATH

cd $DPDK_DOWNLOAD_PATH
rm -rf dpdk-16.04/
wget https://fast.dpdk.org/rel/dpdk-16.04.tar.xz
tar xvf dpdk-16.04.tar.xz
cd dpdk-16.04/

sed -i 's!CONFIG_RTE_EXEC_ENV=.*!CONFIG_RTE_EXEC_ENV=y!1' config/common_base
sed -i 's!CONFIG_RTE_BUILD_SHARED_LIB=.*!CONFIG_RTE_BUILD_SHARED_LIB=y!1' config/common_base
sed -i 's!CONFIG_RTE_LIBRTE_EAL=.*!CONFIG_RTE_LIBRTE_EAL=y!1' config/common_base

make install  T=x86_64-native-linuxapp-gcc DESTDIR=${DPDK_INSTALL_PATH}
cd x86_64-native-linuxapp-gcc
make

export LD_LIBRARY_PATH=$LIB_PATH
export NSTACK_LOG_ON=DBG

#===========build DMM=================
echo "DMM build started....."

cd $LIB_PATH
rm -rf *

cd ../../thirdparty/glog/glog-0.3.4/
sudo autoreconf -ivf

cd $BUILD_DIR
rm -rf *
cmake -D DMM_DPDK_INSTALL_DIR=$DPDK_INSTALL_PATH ..
make -j 8

#DPDK install path need to be updated below if it is changed
sed -i 's!export DPDK_INSTALL_PATH.*!export DPDK_INSTALL_PATH="/root/dpdk_install/tmp/"!1' ../thirdparty/stackpool/release/script/nstack_var.sh
echo "DMM build finished....."
