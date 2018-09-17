#!/bin/bash -x

set -x

# add inherited proxy for sudo user
LINE='Defaults env_keep += "ftp_proxy http_proxy https_proxy no_proxy"'
FILE=/etc/sudoers
grep -qF -- "$LINE" "$FILE" || sudo echo "$LINE" >> "$FILE"

OS_ID=$(grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')

#set and check the environment for Linux
if [ "$OS_ID" == "ubuntu" ]; then
    export DEBIAN_FRONTEND=noninteractive
    export DEBCONF_NONINTERACTIVE_SEEN=true

    APT_OPTS="--assume-yes --no-install-suggests --no-install-recommends -o Dpkg::Options::=\"--force-confdef\" -o Dpkg::Options::=\"--force-confold\""
    sudo apt-get update ${APT_OPTS}
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -yq git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump libpcre3 libpcre3-dev zlibc zlib1g zlib1g-dev vim pkg-config tcl libnl-route-3-200 flex graphviz tk debhelper dpatch gfortran ethtool libgfortran3 bison dkms quilt chrpath swig python-libxml2 unzip
elif [ "$OS_ID" == "debian" ]; then
    echo "not tested for debian and exit"
    exit 1
    export DEBIAN_FRONTEND=noninteractive
    export DEBCONF_NONINTERACTIVE_SEEN=true

    APT_OPTS="--assume-yes --no-install-suggests --no-install-recommends -o Dpkg::Options::=\"--force-confdef\" -o Dpkg::Options::=\"--force-confold\""
    sudo apt-get update ${APT_OPTS}
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -yq git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump libpcre3 libpcre3-dev zlibc zlib1g zlib1g-dev vim
elif [ "$OS_ID" == "centos" ]; then
    sudo yum install -y deltarpm git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump vim sudo yum-utils pcre-devel zlib-devel libiverbs tk tcl tcsh redhat-lsb-core
elif [ "$OS_ID" == "opensuse" ]; then
    echo "not tested for opensuse and exit"
    exit 1
    sudo yum install -y git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump vim sudo yum-utils pcre-devel zlib-devel
fi
