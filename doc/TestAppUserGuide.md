# 1.  Introduction:

This document will help to configure the stack module and protocol routing module,
and run sample app with DMM.

# 2.  Configuration files:

There are two configuration files need to modify for setting up the
topology.

release/configure

**module_config.json** is the stack module config file.

**rd_config.json** is the rd config file.

Copy the two json files to release/bin and configure in next step.

<!-- -->

Note:

    If users have run scripts/build.sh, configuration files were generated in /dmm/config/app_test.

# 3.  JSON files configuration.

We need to setup configuration as given below.

# 3.1  module_config.json for example:

```
{
    "default_stack_name": "kernel",                /*when rd can't be find maybe choose the defualt one*/
    "module_list": [
    {
        "stack_name": "kernel",                    /*stack name*/
        "function_name": "kernel_stack_register",  /*function name*/
        "libname": "./",                           /*library name, if loadtype is static, this maybe
                                                     null, else must give a library name*/
        "loadtype": "static",                      /*library load type: static or dynamic*/
        "deploytype": "1",                         /*deploy model type:model type1, model type2,
                                                     model type3. Indicating single or multi process
                                                     deployment. Used during shared memory initialization.*/
        "maxfd": "1024",                           /*the max fd supported*/
        "minfd": "0",                              /*the min fd supported*/
        "priorty": "1",                            /*priorty when executing, reserv*/
        "stackid": "0",                            /*stack id, this must be ordered and not be repeated*/
        },
/***********************************
    {
        "stack_name": "stackx",                    /*this is not a real stack, just an example for multiple
                                                     stack configurations*/
        "function_name": "stackx_register",
        "libname": "libstackx.so",
        "loadtype": "dynmic",
        "deploytype": "3",
        "maxfd": "1024",
        "minfd": "0",
        "priorty": "1",
        "stackid": "1",
        },
    ]
}
***********************************/
```

# 3.2  rd_config.json for example:

```
{
"ip_route": [
    {
        "subnet": "192.165.1.1/16",
        "type": "nstack-kernel",
                /* output interface type,
                   nstack-kernel: indicate this ip should go through kernel protocol
                   nstack-dpdk: go through stackx protocol*/
    },
    {
        "subnet": "172.16.25.125/16",
        "type": "nstack-kernel",
    }
    ],
"prot_route": [
        {
            "proto_type": "11",
            "type": "nstack-kernel",
        },
    ]
}
```
# 4.  Run sample APP:

-  **perf-test:**

    Add test code with kernel stack or userspace protocol stack


Usage:

After building the DMM, inside the DMM/release/bin directory below perf-test app will be generated.

*kc_common, ks_epoll, ks_select, vc_common, vs_epoll, vs_select*

The use of ks_epoll,ks_select,vs_epoll and vs_select are the same.

Before executing the app, we should disable ASLR (Address space layout randomization).
  echo 0 > /proc/sys/kernel/randomize_va_space

Examples:

**With Kernel stack(With direct kernel stack):**

server:
```
    #./ks_epoll -p 20000 -d 172.16.25.125 -a 10000 -s 172.16.25.126 -l 200 -t 5000000 -i 0 -f 1 -r 20000 -n 1 -w 10 -u 10000 -e 10 -x 1
```
client:
```
    #./kc_common -p 20000 -d 172.16.25.126 -a 10000 -s 172.16.25.125 -l 200 -t 5000000 -i 0 -f 1 -r 20000 -n 1 -w 10 -u 10000 -e 10 -x 1
```

**With DMM nStack(Stack is choosed by congfigure file):**
Note:

   Currently DMM nstack only support kernel mode. please check the config file.

server:
```
    #./vs_epoll -p 20000 -d 172.16.25.125 -a 10000 -s 172.16.25.126 -l 200 -t 5000000 -i 0 -f 1 -r 20000 -n 1 -w 10 -u 10000 -e 10 -x 1
```
client:
```
    #./vc_common -p 20000 -d 172.16.25.126 -a 10000 -s 172.16.25.125 -l 200 -t 5000000 -i 0 -f 1 -r 20000 -n 1 -w 10 -u 10000 -e 10 -x 1
```

- **NGNIX with DMM nStack**:

Nginx build process:

*step 1: Compile nginx code*

```
    #wget "http://hg.nginx.org/nginx"
    #cd nginx-1.1.15
    #./configure --with-ld-opt="-L /root/Work/xxx/release/lib64/ -lnStackAPI"
    #make
    #make install
```
Note:

    a. ./configure **--with-ld-opt="-L /root/xxx/DMM/release/lib64/ -lnStackAPI"** can be done if you want to use nStack otherwise just give the ./configure.
    b. Make sure before building edit the configurtaion if you want specific server IP.
    For Ex:
        Server: ifconfig eth3 172.16.25.125 netmask 255.255.255.224 up
        Client: ifconfig eth3 172.16.25.126 netmask 255.255.255.224 up

    nginx.conf file modifiecations:
        listen      172.16.25.125:80

*step 2: run the nginx code*

```
    #cd /usr/local/nginx/sbin
    #cp -r * ../../sbin
    #cd ../../sbin
    #./nginx
```
Note:

    if you want to run ./nginx with nStack then perform following before run
    a. export LD_LIBRARY_PATH=/root/xxx/DMM/release/lib64/
    b. export NSTACK_LOG_ON=DBG ##to display nStack consol logs.

*step 3: Check whether ngnix service is running.*

```
    #ps -ax | grep nginx
```
*step 4: Test the nginx server is up or not!*

At client board, perform below command and check the output:
```
    #curl http://172.16.25.125:80/
```
Note:

    curl is the tool need to be installed to browse the html page in linux without GUI.
