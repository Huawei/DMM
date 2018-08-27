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
TEMP_DIR=$1
if [ "x$1" != "x" ]; then
    TEMP_DIR=$1
    DMM_BUILD_DIR=${TEMP_DIR}/build
    DPDK_BUILD_SCRIPT_DIR=${DMM_BUILD_DIR}/../scripts
    LWIP_BUILD_DIR=${TEMP_DIR}/stacks/lwip_stack/build/
else
    TEMP_DIR=`dirname $(readlink -f $0)`/..
    DMM_BUILD_DIR=${TEMP_DIR}/../../build
    DPDK_BUILD_SCRIPT_DIR=${DMM_BUILD_DIR}/../scripts
    LWIP_BUILD_DIR=${TEMP_DIR}/build/
fi

echo 0:$0
echo 1:$1
echo 2:$2
echo TEMP_DIR: $TEMP_DIR
echo DMM_BUILD_DIR: $DMM_BUILD_DIR
echo DPDK_BUILD_SCRIPT_DIR: $DPDK_BUILD_SCRIPT_DIR
echo LWIP_BUILD_DIR: $LWIP_BUILD_DIR

OS_ID=$(grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')
OS_VERSION_ID=$(grep '^VERSION_ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')
KERNEL_OS=`uname -o`
KERNEL_MACHINE=`uname -m`
KERNEL_RELEASE=`uname -r`
KERNEL_VERSION=`uname -v`

echo KERNEL_OS: $KERNEL_OS
echo KERNEL_MACHINE: $KERNEL_MACHINE
echo KERNEL_RELEASE: $KERNEL_RELEASE
echo KERNEL_VERSION: $KERNEL_VERSION
echo OS_ID: $OS_ID
echo OS_VERSION_ID: $OS_ID

#DPDK download path
DPDK_DOWNLOAD_PATH=/tmp/dpdk

#dpdk installation path
DPDK_INSTALL_PATH=/usr

#set and check the environment for Linux
if [ "$OS_ID" == "ubuntu" ]; then
    export DEBIAN_FRONTEND=noninteractive
    export DEBCONF_NONINTERACTIVE_SEEN=true

    APT_OPTS="--assume-yes --no-install-suggests --no-install-recommends -o Dpkg::Options::=\"--force-confdef\" -o Dpkg::Options::=\"--force-confold\""
    sudo apt-get update ${APT_OPTS}
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -yq git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump libpcre3 libpcre3-dev zlibc zlib1g zlib1g-dev vim ethtool unzip
elif [ "$OS_ID" == "debian" ]; then
    echo "not tested for debian and exit"
    exit 1
    export DEBIAN_FRONTEND=noninteractive
    export DEBCONF_NONINTERACTIVE_SEEN=true

    APT_OPTS="--assume-yes --no-install-suggests --no-install-recommends -o Dpkg::Options::=\"--force-confdef\" -o Dpkg::Options::=\"--force-confold\""
    sudo apt-get update ${APT_OPTS}
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -yq git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump libpcre3 libpcre3-dev zlibc zlib1g zlib1g-dev vim
elif [ "$OS_ID" == "centos" ]; then
    sudo yum install -y git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump vim sudo yum-utils pcre-devel zlib-devel
elif [ "$OS_ID" == "opensuse" ]; then
    echo "not tested for opensuse and exit"
    exit 1
    sudo yum install -y git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump vim sudo yum-utils pcre-devel zlib-devel
fi

#DPDK will be having dependancy on linux headers
if [ "$OS_ID" == "ubuntu" ]; then
    sudo apt-get -y install git build-essential linux-headers-`uname -r`
    sudo apt-get -y install libnuma-dev
elif [ "$OS_ID" == "debian" ]; then
    sudo apt-get -y install git build-essential linux-headers-`uname -r`
elif [ "$OS_ID" == "centos" ]; then
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y kernel-headers
    sudo yum install -y numactl-devel
elif [ "$OS_ID" == "opensuse" ]; then
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y kernel-headers
fi
#===========build DPDK================

if [ ! -d  /usr/include/dpdk ] || [ ! -d  /usr/share/dpdk ] || [ ! -d  /usr/lib/modules/4.4.0-31-generic/extra/dpdk ]; then
     mkdir -p $DPDK_DOWNLOAD_PATH

     cd $DPDK_DOWNLOAD_PATH
     wget -N https://fast.dpdk.org/rel/dpdk-18.02.tar.xz --no-check-certificate
     tar xvf dpdk-18.02.tar.xz
     cd dpdk-18.02/


     sed -i 's!CONFIG_RTE_EXEC_ENV=.*!CONFIG_RTE_EXEC_ENV=y!1' config/common_base
     sed -i 's!CONFIG_RTE_BUILD_SHARED_LIB=.*!CONFIG_RTE_BUILD_SHARED_LIB=y!1' config/common_base
     sed -i 's!CONFIG_RTE_LIBRTE_EAL=.*!CONFIG_RTE_LIBRTE_EAL=y!1' config/common_base
     sed -i 's!CONFIG_RTE_EAL_PMD_PATH=.*!CONFIG_RTE_EAL_PMD_PATH="/tmp/dpdk/drivers/"!1' config/common_base

     sudo make install  T=x86_64-native-linuxapp-gcc DESTDIR=${DPDK_INSTALL_PATH} -j 4

     mkdir -p /tmp/dpdk/drivers/
     cp -f /usr/lib/librte_mempool_ring.so /tmp/dpdk/drivers/
fi
#===========build DMM=================
echo "DMM build started....."

cd $DMM_BUILD_DIR
ldconfig
rm -rf *
cmake ..
make -j 8
if [ $? -eq 0 ]; then
    echo "DMM build is SUCCESS"
else
    echo "DMM build has FAILED"
    exit 1
fi
echo "DMM build finished....."

#===========build Stackpool===========
echo "Stackpool build started....."
cd $LWIP_BUILD_DIR
cmake ..
make -j 8
if [ $? -eq 0 ]; then
    echo "Stackpool build is SUCCESS"
else
    echo "Stackpool build has FAILED"
    exit 1
fi
echo "Stackpool build finished....."

#===========check running env =================
hugepagesize=$(cat /proc/meminfo | grep Hugepagesize | awk -F " " {'print$2'})
if [ "$hugepagesize" == "2048" ]; then
    pages=1536
elif [ "$hugepagesize" == "1048576" ]; then
    pages=3
fi
sudo sysctl -w vm.nr_hugepages=$pages
HUGEPAGES=`sysctl -n  vm.nr_hugepages`
if [ $HUGEPAGES != $pages ]; then
    echo "ERROR: Unable to get $pages hugepages, only got $HUGEPAGES.  Cannot finish."
    exit
fi


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


sudo mkdir /mnt/nstackhuge -p
if [ "$hugepagesize" == "2048" ]; then
sudo mount -t hugetlbfs -o pagesize=2M none /mnt/nstackhuge/
elif [ "$hugepagesize" == "1048576" ]; then
    sudo mount -t hugetlbfs -o pagesize=1G none /mnt/nstackhuge/
fi
sudo mkdir -p /var/run/ip_module/
sudo mkdir -p /var/log/nStack/ip_module/

export LD_LIBRARY_PATH=$LIB_PATH
export NSTACK_LOG_ON=DBG



#===========set environment===========
if [ "$OS_ID" == "centos" ]; then
    ifaddress1=$(ifconfig enp0s8 | grep 'inet' | cut -d: -f2 | awk '{print $2}')
    echo $ifaddress1
    ifaddresscut=$(ifconfig enp0s8 | grep 'inet' | head -n 1 | awk -F " " '{print $2}' | awk -F "." '{print $1"."$2"."$3}')
    echo $ifaddresscut
    ifmac=$(ifconfig enp0s8 | grep 'ether' | awk -F " " '{print $2}')
    echo $ifmac
elif [ "$OS_ID" == "ubuntu" ]; then
    ifaddress1=$(ifconfig enp0s8 | grep 'inet' | head -n 1 | cut -d: -f2 | awk '{print $1}')
    echo $ifaddress1
    ifaddresscut=$(ifconfig enp0s8 | grep 'inet' | head -n 1 | cut -d: -f2 | awk '{print $1}' | awk -F "." '{print $1"."$2"."$3}')
    echo $ifaddresscut
    ifmac=$(ifconfig enp0s8 | grep 'HWaddr' | awk -F " " '{print $5}')
    echo $ifmac
fi

cd $LWIP_BUILD_DIR/../
bash ./release_tar.sh
cd nStackServer/script
sed -i 's!/root/dpdk/dpdk-18.02!'$DPDK_DOWNLOAD_PATH'/dpdk-18.02!1' nstack_var.sh

cd ../
cp ./configure/*.json bin/
cd bin

if [ "$OS_ID" == "centos" ]; then
    sed -i 's!eth7!enp0s8!1' ip_data.json
elif [ "$OS_ID" == "ubuntu" ]; then
    sed -i 's!eth7!enp0s8!1' ip_data.json
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
    sed -i 's!eth7!enp0s8!1' network_data_tonStack.json
elif [ "$OS_ID" == "ubuntu" ]; then
    sed -i 's!eth7!enp0s8!1' network_data_tonStack.json
fi
sed -i 's!eth7!enp0s8!1' network_data_tonStack.json

cd $DMM_BUILD_DIR/../release/bin
cp -r . ../../stacks/lwip_stack/app_test
cd $DMM_BUILD_DIR/../stacks/lwip_stack/app_test
cp -r ../app_conf/*.json .

sed -i 's!192.168.1.1!'$ifaddresscut'.0!1' rd_config.json

cd $LWIP_BUILD_DIR/../nStackServer
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
