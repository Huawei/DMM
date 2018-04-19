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
#!/bin/bash -x

echo "check whether dpdk installed"
cur_directory=${PWD}
check_dpdk=$(rpm -qa | grep dpdk)
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

if [ ! -s dpdk-16.04.tar.gz ]; then
wget http://dpdk.org/browse/dpdk/snapshot/dpdk-16.04.tar.gz
fi

tar xzvf dpdk-16.04.tar.gz
cp dpdk-16.04/pkg/dpdk.spec ~/rpmbuild/SOURCES/

echo "modify the spec"

#get rid of the dependence of xen-devel
sed -i '48d' dpdk.spec
sed -i '47a BuildRequires: kernel-devel, kernel-headers, libpcap-devel' dpdk.spec

#get rid of the dependence of texlive-collection
sed -i '/BuildRequires: texlive-collection/s/^/#&/' dpdk.spec

#delete something about xen
sed -i '/LIBRTE_PMD_XENVIRT/s/^/#&/' dpdk.spec
sed -i '/LIBRTE_XEN_DOM0/s/^/#&/' dpdk.spec

#delete something of generating doc
sed -i '/%{target} doc/s/^/#&/' dpdk.spec
sed -i '94d' dpdk.spec
sed -i '93a      datadir=%{_datadir}/dpdk' dpdk.spec
sed -i '94a #      datadir=%{_datadir}/dpdk docdir=%{_docdir}/dpdk' dpdk.spec
sed -i '/%files doc/s/^/#&/' dpdk.spec
sed -i '/%doc %{_docdir}/s/^/#&/' dpdk.spec

sed -i '76a sed -i 's!CONFIG_RTE_EXEC_ENV=.*!CONFIG_RTE_EXEC_ENV=y!1' config/common_base' dpdk.spec
sed -i '77a sed -i 's!CONFIG_RTE_BUILD_SHARED_LIB=.*!CONFIG_RTE_BUILD_SHARED_LIB=y!1' config/common_base' dpdk.spec
sed -i '78a sed -i 's!CONFIG_RTE_LIBRTE_EAL=.*!CONFIG_RTE_LIBRTE_EAL=y!1' config/common_base' dpdk.spec

echo "build the dependence"
#sudo yum-builddep -y dpdk.spec

sudo yum install -y  libpcap-devel python-sphinx inkscape


echo "generate the rpm package"
rpmbuild -ba dpdk.spec --define "_sourcedir ${PWD}"
if [ $? -eq 0 ]; then
	echo "rpm build success"
else
	echo "rpm build error"
	exit
fi

echo "install the rpm"
cd ../RPMS/x86_64/
sudo rpm -ivh dpdk-16.04-1.x86_64.rpm
sudo rpm -ivh dpdk-devel-16.04-1.x86_64.rpm
cd ${cur_directory}
