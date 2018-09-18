#!/bin/bash -x

set -x

BUILD_DIR=`dirname $(readlink -f $0)`/../build

OS_ID=$(grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')

echo "DMM build started....."

cd $BUILD_DIR
rm -rf *
cmake ..
make -j 8

if [ $? -eq 0 ]; then
    echo "DMM build is SUCCESS"
else
    echo "DMM build has FAILED"
    exit 1
fi

if [ "$OS_ID" == "centos" ]; then
    make pkg-rpm || exit 1
elif [ "$OS_ID" == "ubuntu" ]; then
    make pkg-deb || exit 1
fi

echo "DMM build has FINISHED"
