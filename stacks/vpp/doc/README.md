# 1. What is VPP Host Stack
VPP's host stack is a user space implementation of a number of transport,
session and application layer protocols that leverages VPP's existing
protocol stack.

# 2. How to use VPP Host Stack

## How to integrate VPP Host Stack into DMM
The file CMakeList.txt defined the compiling process, including downloading
the vpp code and patch it. The patch will modify the makefile to adapt dmm.

Target 'libdmm_vcl' could not be get automatically unless you run
'make vpp-stack' manually. It will compile the adaption code and link the
libraries of vpp and finally generate the library of "libdmm_vcl.so".


## Compile
```sh
    #cd dmm/build && cmake ..
    #make vpp-statck
```
Note:
  After these processes, libdmm_vcl.so library would be generated in
vpp/build-root/install-vpp_debug-native/vpp/lib64/.

##Start VPP Host Stack
- Steps 1: copy the plugins to /usr/lib/.
```sh
	#cp -r vpp/build-root/install-vpp_debug-native/vpp/lib64/vpp_plugins /usr/lib/
```

- Steps 2: load dpdk network card driver manually.
```sh
	#cd dpdk-18.02/x86_64-native-linuxapp-gcc/kmod/
	#modprobe uio
	#insmod igb_uio.ko
```

- Steps 3: choose a network card that is not in use and down it.
```sh
	#ifconfig eth1 down
```
- Steps 4: copy the config file and start vpp
```sh
    #cp dmm/stacks/vpp/configure/startup.conf /etc/vpp/
	#cp dmm/stacks/vpp/configure/vpp_config /etc/vpp/
    #cd vpp/build-root/install-vpp_debug-native/vpp/bin
	#./vpp -c /etc/vpp/startup.conf
```
Note:
  1.modify the dev of dpdk in startup.conf.
  2.modify the interface name and ip in vpp_config.

## Test app
Note:
  Before testing, we should anotation the dmm code that "close (listenFd);" in
  function process_server_accept_thread for server. Otherwize the app can
  not send and recieve packets.

- Steps 1: copy the libdmm_vcl.so to dmm/release/lib64/
- Steps 2: copy the config file from dmm/stacks/vpp/configure/*.json to
           dmm/release/bin and modify the rd_config.json.
```sh
	#vim rd_config.json
	  //set "subnet": "192.168.21.1/24"
```
Note:
  Means dmm will hijack data from subnet 192.168.21.*

- Steps 3: Communication test between machine A(as server) with machine B
		(as client)

##### Run in machine A
```sh
    #cd dmm/release/bin/
	#./vs_epoll -p 20000 -d 192.168.21.180 -a 10000 -s 192.168.21.181 -l 1000 -t 500000 -i 0 -f 1 -r 20000 -n 1 -w 10 -u 50000 -e 10 -x 1
```
Note:
  Means the current machine would be server, and it's
destination address is 192.168.21.180 (client address),
source address is 192.168.21.181(server address)

##### Run in machine B
```
	#cd dmm/release/bin/
	#./vc_common -p 20000 -d 192.168.21.181 -a 10000 -s 192.168.21.180 -l 1000 -t 500000 -i 0 -f 1 -r 20000 -n 1 -w 10 -u 50000 -e 10 -x 1
```
Note:
  Means the current machine would be client, and it's
destination address is 192.168.21.181 (server address),
source address is 192.168.21.180(client address)

# 3. Document description

(dmm/stacks/vpp/)

## configure folder
##### module_config.json
- module_config.json is for configuring dmm protocol stack module

##### rd_config.json
- rd_config.json is to choose which module is better to go through. It will go
	through vpp host protocol stack when RD type is nstack-vpp

## patch folder
- modify the makefile to compile the adapt code.

## adapt folder
##### dmm_vcl_adpt.c && dmm_vcl.h
- vpp host stack adaptation code, including initialization and adaptation functions.

# 4. More Information
- https://wiki.fd.io/view/DMM
- https://wiki.fd.io/view/VPP
- https://wiki.fd.io/view/VPP/HostStack
