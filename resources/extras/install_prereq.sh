#!/bin/bash -x
log_file="/tmp/install_log.txt-`date +'%Y-%m-%d_%H-%M-%S'`"
exec 1> >(tee -a "$log_file")  2>&1

if [ "$(uname)" <> "Darwin" ]; then
    OS_ID=$(grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')
    OS_VERSION_ID=$(grep '^VERSION_ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')
fi

if [ "$OS_ID" == "ubuntu" ]; then
	# Standard update + upgrade dance
	cat << EOF >> /etc/apt/sources.list
	deb http://in.archive.ubuntu.com/ubuntu/ trusty main restricted
	deb-src http://in.archive.ubuntu.com/ubuntu/ trusty main restricted
	deb http://in.archive.ubuntu.com/ubuntu/ trusty-updates main restricted
	deb-src http://in.archive.ubuntu.com/ubuntu/ trusty-updates main restricted
	deb http://in.archive.ubuntu.com/ubuntu/ trusty universe
	deb-src http://in.archive.ubuntu.com/ubuntu/ trusty universe
	deb http://in.archive.ubuntu.com/ubuntu/ trusty-updates universe
	deb-src http://in.archive.ubuntu.com/ubuntu/ trusty-updates universe
	deb http://in.archive.ubuntu.com/ubuntu/ trusty multiverse
	deb-src http://in.archive.ubuntu.com/ubuntu/ trusty multiverse
	deb http://in.archive.ubuntu.com/ubuntu/ trusty-updates multiverse
	deb-src http://in.archive.ubuntu.com/ubuntu/ trusty-updates multiverse
	deb http://in.archive.ubuntu.com/ubuntu/ trusty-backports main restricted universe multiverse
	deb-src http://in.archive.ubuntu.com/ubuntu/ trusty-backports main restricted universe multiverse
	deb http://security.ubuntu.com/ubuntu trusty-security main restricted
	deb-src http://security.ubuntu.com/ubuntu trusty-security main restricted
	deb http://security.ubuntu.com/ubuntu trusty-security universe
	deb-src http://security.ubuntu.com/ubuntu trusty-security universe
	deb http://security.ubuntu.com/ubuntu trusty-security multiverse
	deb-src http://security.ubuntu.com/ubuntu trusty-security multiverse
	deb http://extras.ubuntu.com/ubuntu trusty main
	deb-src http://extras.ubuntu.com/ubuntu trusty main
EOF
elif [ "$OS_ID" == "centos" ]; then

	echo centos
fi
