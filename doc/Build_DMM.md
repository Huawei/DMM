# 1. Introduction:
  This document purpose is to build the DMM and run applications on it.

Note:

  Users can easily build the DMM by running DMM/scripts/build.sh, which contains following steps.

# 2. Build DPDK:
  DPDK need to be built first for DMM RTE memory dependency.

- Steps :

  Download dpdk-16.04.tar.xz from DPDK release, you can get it from [http://static.dpdk.org/rel](http://static.dpdk.org/rel)
```
    #wget http://static.dpdk.org/rel/dpdk-16.04.tar.xz
    #tar xvf dpdk-16.04.tar.xz
    #vi dpdk-16.04/config/common_base //make CONFIG_RTE_BUILD_SHARED_LIB=y CONFIG_RTE_EXEC_ENV=y CONFIG_RTE_LIBRTE_EAL=y
    #cd dpdk-16.04
    #make install  T=x86_64-native-linuxapp-gcc DESTDIR=/root/dpdk_build/tmp2
    #cd x86_64-native-linuxapp-gcc
    #make           //install the dpdk which will generate .so inside lib folder in the path.
```

Note:

    Environment:
        Linux ubuntu 14.04 or some distro which support dpdk-16.04

# 3. Build DMM:

```
    #cd dmm/build
    #cmake -D DMM_DPDK_INSTALL_DIR=$DPDK_INSTALL_PATH ..
```

Note:

    $DPDK_INSTALL_PATH is the path where DPDK has been installed

  After cmake all the makefiles and dependent .sh files will be copied under build directory.

```
    #make -j 8
```
  Then we can get libnStackAPI.so

# 4. Env Setting:

- Hugepage setting:

```
    #vi /etc/default/grub
    //update GRUB_CMDLINE_LINUX_DEFAULT="default_hugepagesz=1G hugepagesz=1G hugepages=8"
    #update-grub
    #reboot
```

Check hugepage info


```
    #cat /proc/meminfo
```

- Mount hugepages:

```
    #mkdir -p /mnt/nstackhuge/
    #mount -t hugetlbfs -o pagesize=1G none /mnt/nstackhuge/    //only 1G pgsize supported now
    #mkdir -p /var/run/ip_module
```


# 5. Build and Run the APP

- Link the app with the lib **libnStackAPI.so** first, you can refer to app_example/perf-test

- Enable detail log of nstack by setting env var

```
    #export NSTACK_LOG_ON=DBG
```

- Copy and update the module_config.json, rd_config.json into the same folder of app

- Run your app
