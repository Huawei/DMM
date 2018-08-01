# 1. Introduction

  The purpose of this document is to illustrate how to build DMM and run applications on it.

Note:

  Users can easily build DMM by running DMM/scripts/build.sh, which contains following steps.

# 2. Build DPDK

  DPDK needs to be built first for DMM RTE memory dependency.

- Steps:

  Download dpdk-18.02.tar.xz from DPDK release, you can get it from [http://static.dpdk.org/rel](http://static.dpdk.org/rel)

```sh
    wget http://static.dpdk.org/rel/dpdk-18.02.tar.xz
    tar xvf dpdk-18.02.tar.xz
    vi dpdk-18.02/config/common_base
      //make CONFIG_RTE_BUILD_SHARED_LIB=y, CONFIG_RTE_EXEC_ENV=y, CONFIG_RTE_LIBRTE_EAL=y, CONFIG_RTE_EAL_PMD_PATH="/tmp/dpdk/drivers/"
    cd dpdk-18.02
    make install  T=x86_64-native-linuxapp-gcc DESTDIR=/usr -j 4
    cd x86_64-native-linuxapp-gcc
    make           # install the dpdk which will generate .so inside lib folder in the path.
    mkdir -p /tmp/dpdk/drivers/
    cp -f /usr/lib/librte_mempool_ring.so /tmp/dpdk/drivers/
```

Note:
  Under certain kernel versions (e.g. v3.10.0 adopted by CentOS 7.5), compiling DPDK with default
  configuration will encounter errors like below.
  `error: unknown field 'ndo_change_mtu' specified in initializer .no_change_mtu = kni_net_change_mtu`
  This is caused by `kni` compiling, disable `kni` in dpdk config file will figure it out.
  Actually, we have tested ubuntu 14.04 ~ 16.04, CentOS 7.2, and they worked well.

# 3. Build DMM

```sh
    cd $(DMM_DIR)/thirdparty/glog/glog-0.3.4/ && autoreconf -ivf
    cd $(DMM_DIR)/build
    cmake ..
```

Note:
  $(DMM_DIR) is the directory where dmm has been cloned.
  After cmake all the makefiles and dependent .sh files will be copied under build directory.

```sh
    make -j 8
```

  Then we can get libnStackAPI.so

  For centos we can use the command 'make pkg-rpm' to generate the rpm package in release/rpm.

# 4. Build rsocket

```sh
    cd $(DMM_DIR)/build
    make dmm_rsocket
```

Note:
  Make sure Mellanox OFED has been installed in your computer, or run the following command before compiling

```sh
    cd $DMM_DIR/stacks/rsocket
    #Take ubuntu16.04 as an example, the tgz file has the format MLNX_OFED_LINUX-<ver>-<OS label><CPU arch>.tgz
    wget http://www.mellanox.com/downloads/ofed/MLNX_OFED-4.4-1.0.0.0/MLNX_OFED_LINUX-4.4-1.0.0.0-ubuntu16.04-x86_64.tgz
    tar -zxvf MLNX_OFED_LINUX-4.4-1.0.0.0-ubuntu16.04-x86_64.tgz
    cd MLNX_OFED_LINUX-4.4-1.0.0.0-ubuntu16.04-x86_64
    ./mlnxofedinstall --force
```

# 5. Env Setting

- Hugepage setting:

```sh
    sudo sysctl -w vm.nr_hugepages=1024
```

Check hugepage info

```sh
    cat /proc/meminfo
```

- Mount hugepages:

```sh
    sudo mkdir -p /mnt/nstackhuge/
    sudo mount -t hugetlbfs -o pagesize=2M none /mnt/nstackhuge/
    sudo mkdir -p /var/run/ip_module
```

# 6. Biuld and Run the APP

- Link the app with the lib **libnStackAPI.so** first, you can refer to app_example/perf-test

- Enable detail log of nstack by setting env var

```sh
    export LD_LIBRARY_PATH=/dmm/release/lib64
    export NSTACK_LOG_ON=DBG
```

- Copy and update the module_config.json, rd_config.json into the same folder of app

- Run your app
