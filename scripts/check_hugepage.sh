#!/bin/bash -x

set -x

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
    exit 1
fi

hugepageTotal=$(cat /proc/meminfo | grep -c "HugePages_Total:       0")
if [ $hugepageTotal -ne 0 ]; then
    echo "HugePages_Total is zero"
    exit 1
fi

hugepageFree=$(cat /proc/meminfo | grep -c "HugePages_Free:        0")
if [ $hugepageFree -ne 0 ]; then
    echo "HugePages_Free is zero"
    exit 1
fi

hugepageSize=$(cat /proc/meminfo | grep -c "Hugepagesize:          0 kB")
if [ $hugepageSize -ne 0 ]; then
    echo "Hugepagesize is zero"
    exit 1
fi

sudo mkdir /mnt/nstackhuge -p
if [ "$hugepagesize" == "2048" ]; then
    sudo mount -t hugetlbfs -o pagesize=2M none /mnt/nstackhuge/
elif [ "$hugepagesize" == "1048576" ]; then
    sudo mount -t hugetlbfs -o pagesize=1G none /mnt/nstackhuge/
fi
