# 1. Introduction : 
 This document purpose is to build the DMM.

# 2. Build DPDK:
DPDK need to be built for DMM RTE dependency.

- Steps :

  download dpdk-16.04.tar.xz from DPDK release, you can get it from [http://static.dpdk.org/rel](http://static.dpdk.org/rel)
```
   #wget http://static.dpdk.org/rel/dpdk-16.04.tar.xz
   #tar xvf dpdk-16.04.tar.xz
   #vi dpdk-16.04/config/common_base #make CONFIG_RTE_BUILD_SHARED_LIB=y
   #cd dpdk-16.04
   #make install  T=x86_64-native-linuxapp-gcc DESTDIR=/root/dpdk_build/tmp2
   #cd x86_64-native-linuxapp-gcc
   #make #to install the dpdk which will generate .so inside lib folder in the path.
```
Make sure the path same as list up
```
    #mkdir -p /root/dpdk_build/tmp2/opt/uvp/evs/
    #cp /root/dpdk/dpdk-16.04/x86_64-native-linuxapp-gcc/kmod/igb_uio.ko             /root/dpdk_build/tmp2/opt/uvp/evs/
```
Note:

    Environment:
        Linux ubuntu 14.04 and above.
    Make sure you have linux-headers-4.4.0-89  linux-headers-4.4.0-89-generic and check uname -a it should have 4.4.0-89-generic header

- Hugepage setting:

```
    #vi /etc/default/grub
```
update GRUB\_CMDLINE\_LINUX\_DEFAULT=&quot;default\_hugepagesz=1G hugepagesz=1G hugepages=8&quot;

```
    #update-grub
    #reboot
```
Check hugepage info HugePages_

```
    #cat /proc/meminfo
```
- Mount hugepages:

``` 
    #mount -t hugetlbfs -o pagesize=1G none /mnt/nstackhuge/
```

```
    #mkdir -p /var/run/ip_module
    #export LD_LIBRARY_PATH=/root/xxxx/DMM/release/lib64/
```
Note:

    no need LD_PRELOAD, This macro is for the same purpose.

- Enable detail log of nstack

```
    #export NSTACK_LOG_ON=DBG
```


# 3. Build DMM:

```
    #cd DMM/build
    #cmake -D DMM_DPDK_INSTALL_DIR=$DPDK_INSTALL_PATH ..
	#Note: DPDK_INSTALL_PATH - the path in which DPDK has been installed
```
 after cmake all the makefiles and dependent .sh files will be copied under build directory.

```
    #make -j 8
```
