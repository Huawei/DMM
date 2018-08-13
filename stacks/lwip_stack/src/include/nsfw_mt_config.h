/*
*
* Copyright (c) 2018 Huawei Technologies Co.,Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at:
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef _FW_MT_CONFIG_H
#define _FW_MT_CONFIG_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define NSFW_CONFIG_MODULE "nsfw_config"
#define NSTACK_SHARE_CONFIG "nstack_share_config"

#define CFG_PATH "NSTACK_CONFIG_PATH"
#define CFG_FILE_NAME "nStackConfig.json"
#define MAX_FILE_NAME_LEN 512
#define CFG_BUFFER_LEN 2048
#define MAX_CFG_ITEM 128
#define CFG_ITEM_LENGTH 64

enum NSTACK_BASE_CFG
{
  CFG_BASE_SOCKET_NUM = 0,
  CFG_BASE_RING_SIZE,
  CFG_BASE_HAL_PORT_NUM,
  CFG_BASE_ARP_STALE_TIME,
  CFG_BASE_ARP_BC_RETRANS_NUM,
  MAX_BASE_CFG
};
COMPAT_PROTECT (MAX_BASE_CFG, 5);

enum NSTACK_CUSTOM_CFG
{
  /* mBuf config */
  CFG_MBUF_DATA_SIZE,
  CFG_TX_MBUF_NUM,
  CFG_RX_MBUF_NUM,

  /* memory pool config */
  CFG_MP_TCPSEG_NUM,
  CFG_MP_MSG_NUM,

  /* RING config */
  CFG_HAL_TX_RING_SIZE,
  CFG_HAL_RX_RING_SIZE,
  CFG_MBOX_RING_SIZE,
  CFG_SPL_MAX_RING_SIZE,

  /* PCB config */
  CFG_TCP_PCB_NUM,
  CFG_UDP_PCB_NUM,
  CFG_RAW_PCB_NUM,

  CFG_ARP_QUEUE_NUM,

  MAX_CUSTOM_CFG
};
COMPAT_PROTECT (CFG_SPL_MAX_RING_SIZE, 8);

enum EN_CFG_SEG
{
  CFG_SEG_BASE = 0,
  CFG_SEG_LOG,
  CFG_SEG_PATH,
  CFG_SEG_PRI,
  CFG_SEG_MAX
};

enum EN_CFG_ITEM_TYPE
{
  CFG_ITEM_TYPE_INT = 0,
  CFG_ITEM_TYPE_STRING
};

enum EN_SEG_BASE_ITEM
{
  CFG_SEG_BASE_SOCKET_NUM = 0,
  CFG_SEG_BASE_ARP_STALE_TIME,
  CFG_SEG_BASE_ARP_BC_RETRANS_NUM,
  CFG_SEG_BASE_MAX
};

enum EN_SEG_THREAD_PRI_ITEM
{
  CFG_SEG_THREAD_PRI_POLICY = 0,
  CFG_SEG_THREAD_PRI_PRI,
  CFG_SEG_THREAD_PRI_MAX
};

typedef void (*custom_check_fn) (void *pitem);

// pack size?
struct cfg_item_info
{
  char *name;
  int type;
  int min_value;
  int max_value;
  int default_value;
  char *default_str;
  custom_check_fn custom_check;
  union
  {
    int value;
    char *pvalue;
  };
};

typedef struct _cfg_module_param
{
  u32 proc_type;
  i32 argc;
  u8 **argv;
} cfg_module_param;

extern u32 g_custom_cfg_items[MAX_CUSTOM_CFG];
extern u32 g_base_cfg_items[MAX_BASE_CFG];

#define get_base_cfg(tag) g_base_cfg_items[(tag)]
#define get_custom_cfg(tag) g_custom_cfg_items[(tag)]
#define set_custom_cfg_item(tag, value) g_custom_cfg_items[(tag)] = (value)

/* stackx config data definition */

/* app socket num */
#define DEF_APP_SOCKET_NUM          1024

/* socket num config */
#define SOCKET_NUM_PER_THREAD       1024        /* socket number per thread */
COMPAT_PROTECT (SOCKET_NUM_PER_THREAD, 1024);

/*
 MAX_SOCKET_NUM: max socket fd number one app can use, it should equal the max socket
 number nstack support(CUR_CFG_SOCKET_NUM)
*/

#define DEF_SOCKET_NUM               1024       /* default socket number */
COMPAT_PROTECT (DEF_SOCKET_NUM, 1024);
#define MIN_SOCKET_NUM               1024       /* min socket number */

#define MAX_SOCKET_NUM               8192       /* default: 8K sockets */

#define CUR_CFG_SOCKET_NUM          get_base_cfg(CFG_BASE_SOCKET_NUM)   /* max socket numbere nstack support */

#define DEF_ARP_STACLE_TIME          300        /* default arp stale time: second */
#define MIN_ARP_STACLE_TIME          30 /* min arp stale time: second */
#define MAX_ARP_STACLE_TIME          1200       /* max arp stale time: second */
#define ARP_STALE_TIME               get_base_cfg(CFG_BASE_ARP_STALE_TIME)

#define DEF_ARP_BC_RETRANS_NUM       5  /* default arp broadcast retransmission times */
#define MIN_ARP_BC_RETRANS_NUM       1  /* min arp broadcast retransmission times */
#define MAX_ARP_BC_RETRANS_NUM       20 /* max arp broadcast retransmission times */
#define ARP_BC_RETRANS_NUM           get_base_cfg(CFG_BASE_ARP_BC_RETRANS_NUM)

/* application mumber config */
#define APP_POOL_NUM                 32 /* max application number */
COMPAT_PROTECT (APP_POOL_NUM, 32);

/* thread number config */
#define DEF_THREAD_NUM               1  /* default stackx thread number */
#define MIN_THREAD_NUM               1  /* min thread number */
#define MAX_THREAD_NUM               1  /* max thread number */
COMPAT_PROTECT (MAX_THREAD_NUM, 1);

/* hash size */
#define MAX_TCP_HASH_SIZE  4096

/* hal port number config */
#define DEF_HAL_PORT_NUM             20 /* port number */
COMPAT_PROTECT (DEF_HAL_PORT_NUM, 20);
#define MIN_HAL_PORT_NUM             1
#define MAX_HAL_PORT_NUM             255
#define CUR_CFG_HAL_PORT_NUM        get_base_cfg(CFG_BASE_HAL_PORT_NUM)

/* vm number config */
#define MAX_VF_NUM                    4 /* max vf number */
COMPAT_PROTECT (MAX_VF_NUM, 4);

/* base ring size config */
#define DEF_RING_BASE_SIZE           2048       /* base ring size */
COMPAT_PROTECT (DEF_RING_BASE_SIZE, 2048);
#define MIN_RING_BASE_SIZE           1024
#define MAX_RING_BASE_SIZE           4096
#define POOL_RING_BASE_SIZE         get_base_cfg(CFG_BASE_RING_SIZE)

/* mbuf data size config */
#define DEF_MBUF_DATA_SIZE           2048       /* mbuf data size */
COMPAT_PROTECT (DEF_MBUF_DATA_SIZE, 2048);
#define TX_MBUF_MAX_LEN              get_custom_cfg(CFG_MBUF_DATA_SIZE)

/* tx mbuf pool size config */
#define DEF_TX_MBUF_POOL_SIZE      (4*POOL_RING_BASE_SIZE)      /* tx mbuf pool size */

#define TX_MBUF_POOL_SIZE           get_custom_cfg(CFG_TX_MBUF_NUM)

/* rx mbuf pool size config */
#define DEF_RX_MBUF_POOL_SIZE      (8*POOL_RING_BASE_SIZE)      /* rx mbuf pool size */

#define RX_MBUF_POOL_SIZE           get_custom_cfg(CFG_RX_MBUF_NUM)

/* hal netif rx/tx ring size config */
#define DEF_HAL_RX_RING_SIZE        2048        /* hal rx ring size */

#define DEF_HAL_TX_RING_SIZE        2048        /* hal tx ring size */
#define HAL_RX_RING_SIZE            get_custom_cfg(CFG_HAL_RX_RING_SIZE)
#define HAL_TX_RING_SIZE            get_custom_cfg(CFG_HAL_TX_RING_SIZE)

/* stackx recv ring size config */
#define DEF_SPL_MAX_RING_SIZE      1024
COMPAT_PROTECT (DEF_SPL_MAX_RING_SIZE, 1024);

#define SPL_MAX_RING_SIZE           get_custom_cfg(CFG_SPL_MAX_RING_SIZE)       /* ring size config, used in recv ring(per socket) */

/* pcb number config */
#define DEF_TCP_PCB_NUM              4096       /* tcp pcb number, per thread */
COMPAT_PROTECT (DEF_TCP_PCB_NUM, 4096);
#define DEF_UDP_PCB_NUM              512        /* udp pcb number, per thread */
COMPAT_PROTECT (DEF_UDP_PCB_NUM, 512);
#define DEF_RAW_PCB_NUM              600        /* raw pcb number, per thread */
COMPAT_PROTECT (DEF_RAW_PCB_NUM, 600);

#define DEF_ARP_QUEUE_NUM           300
#define LARGE_ARP_QUEUE_NUM         (512*1024)

#define SPL_MEMP_NUM_TCP_PCB             get_custom_cfg(CFG_TCP_PCB_NUM)

#define SPL_MEMP_NUM_UDP_PCB             get_custom_cfg(CFG_UDP_PCB_NUM)
#define SPL_MEMP_NUM_RAW_PCB             get_custom_cfg(CFG_RAW_PCB_NUM)

#define SPL_MEMP_NUM_ARP_QUEUE           get_custom_cfg(CFG_ARP_QUEUE_NUM)

/* tcp seg number config */
#define DEF_MEMP_NUM_TCP_SEG        (2*APP_POOL_NUM*DEF_TX_MBUF_POOL_SIZE)
#define SPL_MEMP_NUM_TCP_SEG             get_custom_cfg(CFG_MP_TCPSEG_NUM)      /* tcp segment number, per thread */

/* stackx internal msg number config */
#define DEF_TX_MSG_POOL_SIZE        (DEF_TX_MBUF_POOL_SIZE*APP_POOL_NUM + MAX_VF_NUM*DEF_RX_MBUF_POOL_SIZE + DEF_RING_BASE_SIZE)

#define TX_MSG_POOL_SIZE             get_custom_cfg(CFG_MP_MSG_NUM)     /* msg number, used by stackx internal, per thread */

/* mbox ring size config */
#define DEF_MBOX_RING_SIZE          (DEF_RING_BASE_SIZE/4)
COMPAT_PROTECT (DEF_MBOX_RING_SIZE, 512);

#define MBOX_RING_SIZE               get_custom_cfg(CFG_MBOX_RING_SIZE) /* mbox ring size config, per thread */

/*some probem if CUSOTM_RECV_RING_SIZE more than 4096*/
#define CUSOTM_RECV_RING_SIZE   4096
COMPAT_PROTECT (CUSOTM_RECV_RING_SIZE, 4096);

/*==============================================*
 *      constants or macros define              *
 *----------------------------------------------*/
#define set_cfg_info(tag, item, min, max, def)  { \
    g_cfg_item_info[tag][item].min_value = (min); \
    g_cfg_item_info[tag][item].max_value = (max); \
    g_cfg_item_info[tag][item].default_value = (def);\
    g_cfg_item_info[tag][item].value = (def);\
}

#define get_cfg_info(tag, item)  g_cfg_item_info[tag][item].value

u32 get_cfg_share_mem_size ();

int get_share_cfg_from_mem (void *mem);

int set_share_cfg_to_mem (void *mem);

void config_module_init (cfg_module_param * param);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
