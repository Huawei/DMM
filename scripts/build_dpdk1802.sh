#!/bin/bash -x
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

echo "check whether dpdk installed"
cur_directory=${PWD}
check_dpdk=$(rpm -qa | grep dpdk-devel)
if [ -z "$check_dpdk"  ]; then
    echo "system will install the dpdk"
else
    echo "system has installed the dpdk"
    echo "$check_dpdk"
    exit 0
fi

cd ~
mkdir -p rpmbuild/SOURCES

cd ~/rpmbuild/SOURCES

if [ ! -s dpdk-18.02.tar.gz ]; then
wget http://dpdk.org/browse/dpdk/snapshot/dpdk-18.02.tar.gz
fi

tar xzvf dpdk-18.02.tar.gz
cp dpdk-18.02/pkg/dpdk.spec ~/rpmbuild/SOURCES/

echo "modify the spec"

#get rid of the dependence of texlive-collection
sed -i '/BuildRequires: texlive-collection/s/^/#&/' dpdk.spec

#delete something of generating doc
sed -i '/%{target} doc/s/^/#&/' dpdk.spec
sed -i '98d' dpdk.spec
sed -i '97a      datadir=%{_datadir}/dpdk' dpdk.spec
sed -i '98a #      datadir=%{_datadir}/dpdk docdir=%{_docdir}/dpdk' dpdk.spec
sed -i '/%files doc/s/^/#&/' dpdk.spec
sed -i '/%doc %{_docdir}/s/^/#&/' dpdk.spec

sed -i '82a sed -i '\''s!CONFIG_RTE_EXEC_ENV=.*!CONFIG_RTE_EXEC_ENV=y!1'\'' config/common_base' dpdk.spec
sed -i '83a sed -i '\''s!CONFIG_RTE_BUILD_SHARED_LIB=.*!CONFIG_RTE_BUILD_SHARED_LIB=y!1'\'' config/common_base' dpdk.spec
sed -i '84a sed -i '\''s!CONFIG_RTE_LIBRTE_EAL=.*!CONFIG_RTE_LIBRTE_EAL=y!1'\'' config/common_base' dpdk.spec
sed -i '85a sed -i '\''s!CONFIG_RTE_EAL_PMD_PATH=.*!CONFIG_RTE_EAL_PMD_PATH="/tmp/dpdk/drivers/"!1'\'' config/common_base' dpdk.spec

#disable KNI mode by default
sed -i '93a sed -ri '\''s!CONFIG_RTE_LIBRARY_KNI=.*!CONFIG_RTE_LIBRARY_KNI=n!1'\'' %{target}/.config' dpdk.spec
sed -i '94a sed -ri '\''s!CONFIG_RTE_LIBRARY_PMD_KNI=.*!CONFIG_RTE_LIBRARY_PMD_KNI=n!1'\'' %{target}/.config' dpdk.spec
sed -i '95a sed -ri '\''s!CONFIG_RTE_KNI_KMOD=.*!CONFIG_RTE_KNI_KMOD=n!1'\'' %{target}/.config' dpdk.spec
sed -i '96a sed -ri '\''s!CONFIG_RTE_KNI_PREEMPT_DEFAULT=.*!CONFIG_RTE_KNI_PREEMPT_DEFAULT=n!1'\'' %{target}/.config' dpdk.spec

#Add debug info
sed -i '98s!$! EXTRA_CFLAGS="-O0 -g" !' dpdk.spec

#Not strip the debug info when generate the rpm
sed -i '43 i%global __os_install_post %{nil}\n%define debug_package %{nil}'  dpdk.spec

echo "build the dependence"
#sudo yum-builddep -y dpdk.spec
sudo yum install -y  libpcap-devel python-sphinx inkscape kernel-devel-`uname -r` doxygen libnuma-devel kernel-`uname -r`


echo "generate the rpm package"
rpmbuild -ba dpdk.spec --define "_sourcedir ${PWD}"
if [ $? -eq 0 ]; then
	echo "dpdk rpm build success"
else
	echo "dpdk rpm build error"
	exit 1
fi

echo "install the rpm"
cd ../RPMS/x86_64/
sudo rpm -ivh dpdk-18.02-1.x86_64.rpm || exit 1
sudo rpm -ivh dpdk-devel-18.02-1.x86_64.rpm || exit 1

mkdir -p /tmp/dpdk/drivers/
cp -f /usr/lib64/librte_mempool_ring.so /tmp/dpdk/drivers/
cp -f /usr/share/dpdk/usertools/dpdk-devbind.py /usr/sbin/

cd ${cur_directory}
