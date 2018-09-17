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
    make pkg-rpm
    if [ $? -eq 0 ]; then
        echo "DMM rpm build is SUCCESS"
    else
        echo "DMM rpm build has FAILED"
        exit 1
    fi
elif [ "$OS_ID" == "ubuntu" ]; then
    make pkg-deb
    if [ $? -eq 0 ]; then
        echo "DMM deb build is SUCCESS"
    else
        echo "DMM deb build has FAILED"
        exit 1
    fi
fi

echo "DMM build has FINISHED"
