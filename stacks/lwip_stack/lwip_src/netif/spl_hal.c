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

#include "sys_arch.h"
#include "netif.h"
#include "spl_sockets.h"
//#include <netinet/in.h>

#include "stackx_spl_share.h"
#include "stackx_pbuf.h"
#include "spl_api.h"
#include "sharedmemory.h"
//#include "nettool.h"
#include "lwip/etharp.h"
#include "ip_module_api.h"
#include "sc_dpdk.h"
#include "nstack_log.h"
#include "common.h"
#include "nstack_securec.h"
//#include "ip.h"
#include "configuration_reader.h"
#include "spl_hal.h"
#include "nsfw_maintain_api.h"
#include "stackx_common.h"
#include "spl_instance.h"
#include <netinet/in.h>
#include "prot/tcp.h"

extern u32_t g_mbuf_size[MAX_THREAD_NUM];
extern u32_t uStackArgIndex;
extern void smp_parse_stack_args (int argc, char **argv);
extern void spl_do_dump (struct spl_pbuf *p, u16 direction);

#define SPL_HAL_SEND_TRY 100000

#define SPL_HAL_MODULE "SPL_HAL_MODULE"
extern u16_t g_offSetArry[SPL_PBUF_MAX_LAYER];

/* structure to store the rx and tx packets. Put two per cache line as ports
 * used in pairs */
struct port_stats
{
  unsigned rx;
  unsigned tx;
  unsigned drop;
  u64_t rx_size;
  u64_t tx_size;
  u64_t recv_last_cycles;
  u64_t send_last_cycles;
} __attribute__ ((aligned (COMMON_CACHE_LINE_SIZE / 2)));

struct port_capa
{
  u32_t tx_ipv4_cksum_offload;
  u32_t tx_udp_cksum_offload;
  u32_t tx_tcp_cksum_offload;
};

struct rx_pkts
{
  u16_t num;
  u16_t index;
  struct common_mem_mbuf *pkts[PKT_BURST];
};

struct tx_pkts
{
  u16_t num;
  struct common_mem_mbuf *pkts[PKT_BURST];
};

struct port_pkts
{
  struct rx_pkts rx;
  struct tx_pkts tx;
};

struct psd_header
{
  u32_t src_addr;               /* IPaddress of source host. */
  u32_t dst_addr;               /* IPaddress of destination host(s). */
  u8_t zero;                    /* zero. */
  u8_t proto;                   /* L4 protocol type. */
  u16_t len;                    /* L4 length. */
} __attribute__ ((__packed__));

NSTACK_STATIC unsigned num_ports_NIC = 0;
NSTACK_STATIC unsigned num_ports_NIC_start = 0;

struct stackx_port_info *head_used_port_list;
struct stackx_port_zone *p_stackx_port_zone = NULL;

struct bond_ports_info bond_ports_array = {.cnt = 0 };

static u8_t bond_ports_array_cnt_start = 0;

static struct port_capa spl_hal_capa = { 0 };

static struct port_pkts spl_hal_pkts[HAL_MAX_NIC_NUM];

NSTACK_STATIC inline u16_t
get_ipv4_16b_sum (u16_t * ptr16, u32_t nr)
{
  u32_t sum = 0;

  while (nr > 1)
    {
      sum += *ptr16;
      nr -= sizeof (u16_t);
      ptr16++;

      if (sum > UINT16_MAX)
        {
          sum -= UINT16_MAX;
        }
    }

  /* If length is in odd bytes */
  if (nr)
    {
      sum += *((u8_t *) ptr16);
    }

  sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
  sum &= 0x0ffff;
  return (u16_t) sum;
}

NSTACK_STATIC inline u16_t
get_ipv4_bswap16 (u16_t x)
{
  return (u16_t) (((x & 0x00ffU) << 8) | ((x & 0xff00U) >> 8));
}

NSTACK_STATIC inline u16_t
get_ipv4_psd_sum (struct ip_hdr * iphdr, u64_t ol_flags)
{
  struct psd_header psd_hdr;

  psd_hdr.src_addr = iphdr->src.addr;
  psd_hdr.dst_addr = iphdr->dest.addr;
  psd_hdr.zero = 0;
  psd_hdr.proto = iphdr->_proto;

  if (ol_flags & PKT_TX_TCP_SEG)
    {
      psd_hdr.len = 0;
    }
  else
    {
      psd_hdr.len = get_ipv4_bswap16 ((get_ipv4_bswap16 (iphdr->_len)
                                       - sizeof (struct ip_hdr)));
    }

  return get_ipv4_16b_sum ((u16_t *) & psd_hdr, sizeof (struct psd_header));
}

/* should be called after head_used_port_list is initialized */
NSTACK_STATIC hal_hdl_t
get_port_hdl_by_name (const char *name)
{
  unsigned int i = 0;
  struct stackx_port_info *p = p_stackx_port_zone->stackx_one_port;

  while (i < p_stackx_port_zone->port_num)
    {
      if (!strncasecmp (p->linux_ip.if_name, name, strlen (name)))
        {
          return p->linux_ip.hdl;
        }

      p = &p_stackx_port_zone->stackx_one_port[++i];
    }

  NSPOL_LOGERR ("failed to find port id]name=%s", name);
  return hal_get_invalid_hdl ();
}

NSTACK_STATIC struct stackx_port_info *
get_port_info_by_name (const char *name)
{
  struct stackx_port_info *p = p_stackx_port_zone->stackx_one_port;
  unsigned int i = 0;

  while (i < p_stackx_port_zone->port_num)
    {
      if (!strncasecmp (p->linux_ip.if_name, name, strlen (name)))
        {
          return p;
        }

      p = &p_stackx_port_zone->stackx_one_port[++i];
    }

  return NULL;
}

NSTACK_STATIC int
del_port_in_port_list (const char *name)
{
  struct stackx_port_info *inf = head_used_port_list;
  struct stackx_port_info *prev = NULL;

  while (inf)
    {
      if (!strncasecmp (inf->linux_ip.if_name, name, strlen (name)))
        {
          if (prev != NULL)
            {
              prev->next_use_port = inf->next_use_port;
            }
          else
            {
              head_used_port_list = inf->next_use_port;
            }

          break;
        }

      prev = inf;
      inf = inf->next_use_port;
    }

  return 0;
}

extern void create_netif (struct stackx_port_info *p_port_info);

NSTACK_STATIC int
add_port_in_port_list (struct stackx_port_info *p)
{
  char *name;
  struct stackx_port_info *inf = head_used_port_list;
  struct stackx_port_info *prev = NULL;

  name = p->linux_ip.if_name;

  while (inf)
    {
      if (!strncasecmp (inf->linux_ip.if_name, name, strlen (name)))
        {
          NSPOL_LOGERR ("ERROR: add an existing port!");
          return -1;
        }

      prev = inf;
      inf = inf->next_use_port;
    }

  if (prev == NULL)
    {
      head_used_port_list = p;
    }
  else
    {
      prev->next_use_port = p;
    }

  p->next_use_port = NULL;
  create_netif (p);
  return 0;
}

/* Queries the link status of a port and prints it to screen */
NSTACK_STATIC void
report_port_link_status (struct stackx_port_info *p)
{
  /* get link status */
  u32 status;

  status = hal_link_status (p->linux_ip.hdl);

  if (status)
    {
      NSPOL_LOGINF (SC_DPDK_INFO, "Port=%s: Link Up", p->linux_ip.if_name);
    }
  else
    {
      NSPOL_LOGINF (SC_DPDK_INFO, "Port=%s: Link Down", p->linux_ip.if_name);
    }
}

int
spl_hal_ether_etoa (const unsigned char *e, int e_len, char *a, int a_len)
{
  char *c = a;
  int i;
  int retVal;

  if (!e || !a || e_len < 0)
    return -1;

  if (e_len > NETIF_ETH_ADDR_LEN)
    e_len = NETIF_ETH_ADDR_LEN;

  if (a_len < e_len * 3)
    return -1;

  for (i = 0; i < e_len; i++)
    {
      if (i)
        {
          *c++ = ':';
        }
      retVal = SPRINTF_S (c, a_len - (c - a), "%02x", e[i] & 0xff);
      if (-1 == retVal)
        {
          NSPOL_LOGERR ("SPRINTF_S failed]ret=%d.", retVal);
          return -1;
        }
      c = c + retVal;
    }

  return 0;
}

NSTACK_STATIC inline void
spl_hal_buf_convert (struct common_mem_mbuf *mbuf, struct spl_pbuf **buf)
{
  struct common_mem_mbuf *before = NULL;
  struct spl_pbuf *last = NULL;
  struct spl_pbuf *first = NULL;
  struct spl_pbuf *tmp = NULL;

  while (mbuf != NULL)
    {
      //dpdk 2.1
      tmp =
        (struct spl_pbuf *) ((char *) mbuf + sizeof (struct common_mem_mbuf));
      res_alloc (&tmp->res_chk);
      tmp->payload = common_pktmbuf_mtod (mbuf, void *);
      tmp->tot_len = mbuf->pkt_len;
      tmp->len = mbuf->data_len;
      tmp->type = SPL_PBUF_HUGE;
      tmp->proto_type = SPL_PBUF_PROTO_NONE;
      tmp->next = NULL;
      tmp->flags = 0;

      if (first == NULL)
        {
          first = tmp;
          last = first;
        }
      else
        {
          /* Always the "if(first == NULL)" code segment is executed and then
             "else" segment code is executed, so the "last" variable is not
             NULL always when "else" case is executed */
          last->next = tmp;
          last = tmp;
        }

      before = mbuf;
      mbuf = mbuf->next;

      before->next = NULL;
    }

  *buf = first;
}

NSTACK_STATIC int
spl_hal_port_zone_init ()
{
  int retVal;
  nsfw_mem_zone create_port_zone;
  nsfw_mem_zone create_port_info;
  struct stackx_port_info *mz_port_info;
  INITPOL_LOGINF ("RTP", "spl_hal_port_zone_init", NULL_STRING,
                  LOG_INVALID_VALUE, MODULE_INIT_START);

  if ((CUR_CFG_HAL_PORT_NUM < 1)
      || (SIZE_MAX / sizeof (struct stackx_port_info) < CUR_CFG_HAL_PORT_NUM))
    {
      NSPOL_LOGERR ("malloc parameter incorrect]max_linux_port=%u",
                    CUR_CFG_HAL_PORT_NUM);
      return -1;
    }

  if (spl_snprintf
      (create_port_zone.stname.aname, NSFW_MEM_NAME_LENGTH - 1, "%s",
       MP_STACKX_PORT_ZONE) < 0)
    {
      NSPOL_LOGERR ("spl_snprintf fail");

      return -1;
    }

  create_port_zone.stname.entype = NSFW_SHMEM;
  create_port_zone.isocket_id = SOCKET_ID_ANY;
  create_port_zone.length = sizeof (struct stackx_port_zone);
  create_port_zone.ireserv = 0;
  p_stackx_port_zone =
    (struct stackx_port_zone *) nsfw_mem_zone_create (&create_port_zone);

  if (NULL == p_stackx_port_zone)
    {
      INITPOL_LOGERR ("RTP", "spl_hal_port_zone_init",
                      "Cannot create memory zone for MP_STACKX_PORT_ZONE information",
                      LOG_INVALID_VALUE, MODULE_INIT_FAIL);
      common_exit (EXIT_FAILURE,
                   "Cannot create memory zone for MP_STACKX_PORT_ZONE information");
    }

  retVal =
    MEMSET_S (p_stackx_port_zone, sizeof (struct stackx_port_zone), 0,
              sizeof (struct stackx_port_zone));

  if (EOK != retVal)
    {
      INITPOL_LOGERR ("RTP", "spl_hal_port_zone_init", "MEMSET_S return fail",
                      retVal, MODULE_INIT_FAIL);
      nsfw_mem_zone_release (&create_port_zone.stname);
      return -1;
    }

  if (spl_snprintf
      (create_port_info.stname.aname, NSFW_MEM_NAME_LENGTH - 1, "%s",
       MP_STACKX_PORT_INFO) < 0)
    {
      NSPOL_LOGERR ("VSNPRINTF_S fail");
      return -1;
    }

  create_port_info.stname.entype = NSFW_SHMEM;
  create_port_info.isocket_id = SOCKET_ID_ANY;
  create_port_info.length =
    CUR_CFG_HAL_PORT_NUM * sizeof (struct stackx_port_info);
  create_port_info.ireserv = 0;
  mz_port_info =
    (struct stackx_port_info *) nsfw_mem_zone_create (&create_port_info);

  if (NULL == mz_port_info)
    {
      INITPOL_LOGERR ("RTP", "spl_hal_port_zone_init",
                      "Cannot create memory zone for MP_STACKX_PORT_INFO information",
                      LOG_INVALID_VALUE, MODULE_INIT_FAIL);
      common_exit (EXIT_FAILURE,
                   "Cannot create memory zone for MP_STACKX_PORT_INFO information");
    }

  retVal =
    MEMSET_S (mz_port_info, create_port_info.length, 0,
              create_port_info.length);

  if (EOK != retVal)
    {
      INITPOL_LOGERR ("RTP", "spl_hal_port_zone_init", "MEMSET_S return fail",
                      retVal, MODULE_INIT_FAIL);
      nsfw_mem_zone_release (&create_port_info.stname);
      nsfw_mem_zone_release (&create_port_zone.stname);
      return -1;
    }

  MEM_STAT (SPL_HAL_MODULE, create_port_zone.stname.aname, NSFW_SHMEM,
            create_port_info.length);

  p_stackx_port_zone->stackx_one_port = mz_port_info;

  INITPOL_LOGINF ("RTP", "spl_hal_port_zone_init", NULL_STRING,
                  LOG_INVALID_VALUE, MODULE_INIT_SUCCESS);

  return 0;
}

int
spl_hal_init (int argc, char *argv[])
{
  int retval = -1;
  int idx_init;

  NSPOL_LOGINF (SC_DPDK_INFO, "spl_hal_init start");

  /* Get nstack args */
  smp_parse_stack_args (argc, argv);

  if (0 == uStackArgIndex)
    {
      NSPOL_LOGERR ("uStackArgIndex is 0, can lead to long loop]");
      return retval;
    }

  /* Init DPDK */
  argc = uStackArgIndex--;
  INITPOL_LOGINF ("RTP", "hal_init_global", NULL_STRING, LOG_INVALID_VALUE,
                  MODULE_INIT_START);

  for (idx_init = 0; idx_init < argc; idx_init++)
    {
      NSPOL_LOGINF (SC_DPDK_INFO,
                    "hal_init_global]idx_init=%d,argv[idx_init]=%s", idx_init,
                    argv[idx_init]);
    }

  retval = hal_init_global (argc, argv);

  if (0 != retval)
    {
      NSPOL_LOGERR ("call hal_init_global fail]retval = %d", retval);
      return -1;
    }

  retval = hal_init_local ();

  if (0 != retval)
    {
      NSPOL_LOGERR ("call hal_init_local fail]retval = %d", retval);
      return -1;
    }

  retval = spl_hal_port_zone_init ();
  if (0 != retval)
    {
      NSPOL_LOGERR ("call hal_init_local fail]retval = %d", retval);
      return -1;
    }

  NSPOL_LOGDBG (SC_DPDK_INFO, "Finished Process Init");

  return 0;

}

static inline int
spl_hal_rx_mbuf_free (void *data, void *arg)
{
  struct spl_pbuf *tmp = NULL;
  struct common_mem_mbuf *mbuf = (struct common_mem_mbuf *) data;
  (void) arg;

  tmp = (struct spl_pbuf *) ((char *) mbuf + sizeof (struct common_mem_mbuf));
  if (tmp->res_chk.alloc_flag == TRUE)
    return 1;

  if (common_mbuf_refcnt_read (mbuf) == 0)
    return 1;

  NSPOL_LOGDBG (SC_DPDK_INFO, "rx_pool init in fault case: free mbuf=%p",
                mbuf);
  spl_mbuf_free (mbuf);
  return 0;
}

struct common_mem_mempool *
spl_hal_rx_pool_create (int nic_id, int queue_id, int start_type)
{
  int retval;
  struct common_mem_mempool *mp;
  nsfw_mem_mbfpool create_mbuf_pool;
  nsfw_mem_name lookup_mbuf_pool;
  struct common_mem_ring *ring;

  if (start_type == 1)
    {
      create_mbuf_pool.stname.entype = NSFW_SHMEM;
      create_mbuf_pool.uscash_size = 0;
      create_mbuf_pool.uspriv_size = 0;
      create_mbuf_pool.isocket_id = SOCKET_ID_ANY;
      create_mbuf_pool.enmptype = NSFW_MRING_SPSC;

      retval =
        spl_snprintf (create_mbuf_pool.stname.aname, NSFW_MEM_NAME_LENGTH - 1,
                      "%s", get_mempoll_rx_name (queue_id, nic_id));

      if (-1 == retval)
        {
          NSPOL_LOGERR ("spl_snprintf fail");
          return NULL;
        }

      create_mbuf_pool.usnum = RX_MBUF_POOL_SIZE - 1;
      /*performance, rx buf cap is special, ((size - HEADROOM) >> 10) <<10, see ixgbe_dev_rx_init;
         if want cap size == TX_MBUF_MAX_LEN, must let data_root=TX_MBUF_MAX_LEN+COMMON_PKTMBUF_HEADROOM and
         TX_MBUF_MAX_LEN must N*1024;
       */
      create_mbuf_pool.usdata_room =
        TX_MBUF_MAX_LEN + COMMON_PKTMBUF_HEADROOM;

      NSPOL_LOGDBG (SC_DPDK_INFO, "hal_rx_pool.usnum=%u, usdata_room=%u",
                    create_mbuf_pool.usnum, create_mbuf_pool.usdata_room);

      mp =
        (struct common_mem_mempool *)
        nsfw_mem_mbfmp_create (&create_mbuf_pool);

      if (mp == NULL)
        {
          NSPOL_LOGERR ("nsfw_mem_mbfmp_create fail");
          return NULL;
        }

      MEM_STAT (SPL_HAL_MODULE, create_mbuf_pool.stname.aname, NSFW_SHMEM,
                nsfw_mem_get_len (mp, NSFW_MEM_MBUF));
      NSPOL_LOGDBG (SC_DPDK_INFO, "create:thread=%d,nic_id=%d,mp=%p,size=%d",
                    queue_id, nic_id, mp, nsfw_mem_get_len (mp,
                                                            NSFW_MEM_MBUF));

      char rx_msg_arr_name[NSFW_MEM_NAME_LENGTH];
      data_com_msg *rx_msg_array = NULL;
      retval = spl_snprintf (rx_msg_arr_name, NSFW_MEM_NAME_LENGTH, "%s",
                             get_mempoll_rxmsg_name (queue_id, nic_id));

      if (-1 != retval)
        {
          rx_msg_array = (data_com_msg *) sbr_create_mzone (rx_msg_arr_name,
                                                            (size_t)
                                                            sizeof
                                                            (data_com_msg) *
                                                            RX_MBUF_POOL_SIZE);
        }

      if (!rx_msg_array)
        {
          NSSBR_LOGERR
            ("Create rx_msg_array zone fail]name=%s, num=%u, size=%zu",
             rx_msg_arr_name, RX_MBUF_POOL_SIZE,
             (size_t) sizeof (data_com_msg) * RX_MBUF_POOL_SIZE);
        }
      else
        {
          /*bind msg to pbuf */
          MEM_STAT (SPL_HAL_MODULE, rx_msg_arr_name, NSFW_SHMEM,
                    (size_t) sizeof (data_com_msg) * RX_MBUF_POOL_SIZE);
          NSSBR_LOGINF
            ("Create rx_msg_array zone ok]name=%s, ptr=%p, num=%u, size=%zu",
             rx_msg_arr_name, rx_msg_array, RX_MBUF_POOL_SIZE,
             sizeof (data_com_msg) * RX_MBUF_POOL_SIZE);

          struct common_mem_mbuf *mbuf = NULL;
          struct spl_pbuf *buf = NULL;
          u32 loop = 0;

          for (; loop < RX_MBUF_POOL_SIZE; loop++)
            {
              mbuf = nsfw_mem_mbf_alloc (mp, NSFW_SHMEM);

              if (!mbuf)
                {
                  /* alloc failed , still can work, no prebind success just not so faster */
                  NSSBR_LOGERR
                    ("nsfw_mem_mbf_alloc failed,this can not happen");
                  break;
                }

              buf =
                (struct spl_pbuf *) ((char *) mbuf +
                                     sizeof (struct common_mem_mbuf));
              sys_sem_init (&rx_msg_array[loop].param.op_completed);
              rx_msg_array[loop].param.msg_from = NULL;
              buf->msg = (void *) &rx_msg_array[loop];
              (void) res_free (&buf->res_chk);  //no need to check return value, as it will do free operation depends on alloc_flag

              if (nsfw_mem_mbf_free (mbuf, NSFW_SHMEM) < 0)
                {
                  /* free failed , still can work, no prebind work just not so faster */
                  NSSBR_LOGERR
                    ("nsfw_mem_mbf_free failed,this can not happen");
                  break;
                }
            }

        }

    }
  else
    {
      retval =
        spl_snprintf (lookup_mbuf_pool.aname, NSFW_MEM_NAME_LENGTH - 1, "%s",
                      get_mempoll_rx_name (queue_id, nic_id));

      if (-1 == retval)
        {
          NSPOL_LOGERR ("spl_snprintf fail");
          return NULL;
        }

      lookup_mbuf_pool.entype = NSFW_SHMEM;
      lookup_mbuf_pool.enowner = NSFW_PROC_MAIN;
      mp =
        (struct common_mem_mempool *)
        nsfw_mem_mbfmp_lookup (&lookup_mbuf_pool);

      if (mp == NULL)
        {
          NSPOL_LOGERR ("nsfw_mem_mbfmp_lookup fail, name=%s, try to create",
                        lookup_mbuf_pool.aname);
          return spl_hal_rx_pool_create (nic_id, queue_id, 1);
        }

      NSPOL_LOGDBG (SC_DPDK_INFO, "lookup:thread=%d,nic_id=%d,mp=%p,size=%d",
                    queue_id, nic_id, mp, nsfw_mem_get_len (mp,
                                                            NSFW_MEM_MBUF));

      /*We have to recycle RX mbufs hold by DPDK when fault recovering of upgrading nstack */
      if (start_type == 3 || start_type == 2)
        {
          ring = (struct common_mem_ring *) (mp->pool_data);
          NSPOL_LOGINF (SC_DPDK_INFO,
                        "BEFORE clear rx_mpool]prod.head=%u, prod.tail=%u, "
                        "cons.head=%u, cons.tail=%u", ring->prod.head,
                        ring->prod.tail, ring->cons.head, ring->cons.tail);

          if (nsfw_mem_mbuf_iterator (mp, 0, mp->size,
                                      spl_hal_rx_mbuf_free, NULL) < 0)
            {
              NSPOL_LOGERR ("nsfw_mem_mbuf_iterator return fail");
              return NULL;
            }

          NSPOL_LOGINF (SC_DPDK_INFO,
                        "AFTER clear rx_mpool]prod.head=%u, prod.tail=%u, "
                        "cons.head=%u, cons.tail=%u", ring->prod.head,
                        ring->prod.tail, ring->cons.head, ring->cons.tail);
        }

    }

  return mp;

}

int
spl_hal_bond_config (struct network_configuration *network)
{
  struct phy_net *phynet = network->phy_net;
  struct ref_nic *phead = phynet->header;
  static u8_t bond_index = 0;   /* for auto-generating bond_name */
  unsigned int check_bond = 0, check_name;
  int retVal;
  u8_t j, k, idx = 0;

  /* get bond info from network configuration */
  if (phynet->bond_mode != -1 && bond_ports_array.cnt < MAX_BOND_PORT_NUM)
    {
      struct ref_nic *phead_bond = phead;
      char *name = phynet->bond_name;
      struct bond_set *s = &bond_ports_array.ports[bond_ports_array.cnt];

      while (phead_bond != NULL)
        {
          /* check slave name, repeated slave nic cannot be added to bond set. */
          check_name = 0;

          for (j = 0; j < idx; j++)
            {
              if (strcmp (s->slave_ports[j], phead_bond->nic_name) == 0)
                {
                  check_name = 1;
                  break;
                }
            }

          if (check_name)
            {
              break;
            }

          /* if this nic has been added to a bond_set, ignore it */
          check_bond = 0;

          for (k = 0; k < bond_ports_array.cnt && !check_bond; k++)
            {
              for (j = 0;
                   j < bond_ports_array.ports[k].slave_port_cnt
                   && !check_bond; j++)
                {
                  if (strcmp
                      (bond_ports_array.ports[k].slave_ports[j],
                       phead_bond->nic_name) == 0)
                    {
                      check_bond = 1;

                      if (name[0] == 0)
                        {
                          retVal =
                            STRNCPY_S (name, IP_MODULE_MAX_NAME_LEN,
                                       bond_ports_array.
                                       ports[k].bond_port_name,
                                       strlen (bond_ports_array.
                                               ports[k].bond_port_name));
                          if (EOK != retVal)
                            {
                              NSPOL_LOGERR ("STRNCPY_S failed]ret=%d.",
                                            retVal);
                              return -1;
                            }
                        }

                      if (strcmp
                          (name,
                           bond_ports_array.ports[k].bond_port_name) != 0)
                        {
                          NSOPR_SET_ERRINFO (-1, "%s init failed!\n", name);
                          NSPOL_LOGERR
                            ("%s init failed! %s in both %s and %s", name,
                             phead_bond->nic_name, name,
                             bond_ports_array.ports[k].bond_port_name);
                          return -1;
                        }
                    }
                }
            }

          if (check_bond == 1)
            {
              break;
            }

          /* copy slave ports name to bond array */
          retVal =
            STRNCPY_S (s->slave_ports[idx], HAL_MAX_NIC_NAME_LEN,
                       phead_bond->nic_name, strlen (phead_bond->nic_name));

          if (EOK != retVal)
            {
              NSPOL_LOGERR ("STRNCPY_S failed]ret=%d.", retVal);
              return -1;
            }

          idx++;
          phead_bond = phead_bond->next;

          if (idx >= HAL_MAX_SLAVES_PER_BOND)
            {
              break;
            }
        }

      if (check_bond == 0)
        {
          if (name[0] == 0)
            {
              /* if bond_name is a empty string, generate a new bond name */
              retVal =
                SPRINTF_S (name, HAL_MAX_NIC_NAME_LEN, "bond%u_auto",
                           bond_index++);

              if (-1 == retVal)
                {
                  NSPOL_LOGERR ("SPRINTF_S failed]ret=%d.", retVal);
                  return -1;
                }
            }

          /* copy bond_name to bond array */
          retVal =
            STRNCPY_S (s->bond_port_name, HAL_MAX_NIC_NAME_LEN, name,
                       strlen (name));

          if (EOK != retVal)
            {
              NSPOL_LOGERR ("STRNCPY_S failed]ret=%d.", retVal);
              return -1;
            }

          s->slave_port_cnt = idx;
          bond_ports_array.cnt++;
          NSPOL_LOGINF (SC_DPDK_INFO,
                        "bond_ports_array.cnt=%u,slave_port_cnt=%u",
                        bond_ports_array.cnt, s->slave_port_cnt);
        }
    }

  return 0;
}

int
spl_hal_port_config (unsigned int *port_num)
{
  int retVal;
  unsigned int check;
  struct phy_net *phynet;

  struct network_configuration *network = get_network_list ();

  if (!network)
    {
      NSPOL_LOGERR ("fail to get_provider_node");
      return -1;
    }

  unsigned int port_index = p_stackx_port_zone->port_num;

  while (network && (phynet = network->phy_net))
    {
      struct ref_nic *phead = phynet->header;
      NSPOL_LOGINF (SC_DPDK_INFO, "network=%p,network_name=%s", network,
                    network->network_name);

      if (spl_hal_bond_config (network) < 0)
        {
          NSPOL_LOGERR ("spl_hal_bond_config fail.");
          return -1;
        }

      while (phead != NULL)
        {
          /* check if the NIC is inited */
          for (check = 0; check < port_index; ++check)
            {
              if (strcmp
                  (p_stackx_port_zone->stackx_one_port[check].
                   linux_ip.if_name, phead->nic_name) == 0)
                {
                  break;
                }
            }

          if (check != port_index)
            {
              phead = phead->next;
              continue;
            }

          /* check if the number of VF exceeds MAX_VF_NUM */
          if (port_index >= MAX_VF_NUM + p_stackx_port_zone->bonded_port_num)
            {
              NSOPR_SET_ERRINFO (-1, "Support Only %d VF. %s init failed!\n",
                                 MAX_VF_NUM, phead->nic_name);
              NSPOL_LOGERR ("Support Only %d VF. %s init failed!", MAX_VF_NUM,
                            phead->nic_name);
              NSOPR_SET_ERRINFO (-1, "Add network %s failed!\n",
                                 network->network_name);
              break;
            }

          if (strlen (phead->nic_name) >=
              sizeof (p_stackx_port_zone->
                      stackx_one_port[port_index].linux_ip.if_name) - 1
              || strlen (phead->nic_name) <= 3)
            {
              NSPOL_LOGERR ("Invalid configuration");
              return -1;
            }

          retVal =
            STRCPY_S (p_stackx_port_zone->
                      stackx_one_port[port_index].linux_ip.if_name,
                      sizeof (p_stackx_port_zone->
                              stackx_one_port[port_index].linux_ip.if_name),
                      phead->nic_name);

          if (EOK != retVal)
            {
              NSPOL_LOGERR ("STRCPY_S failed]ret=%d.", retVal);
              return -1;
            }

          NSPOL_LOGINF (SC_DPDK_INFO, "if_name %s",
                        p_stackx_port_zone->
                        stackx_one_port[port_index].linux_ip.if_name);
          retVal =
            STRCPY_S (p_stackx_port_zone->
                      stackx_one_port[port_index].linux_ip.ip_addr_linux,
                      sizeof (p_stackx_port_zone->
                              stackx_one_port[port_index].linux_ip.
                              ip_addr_linux), "0.0.0.0");

          if (EOK != retVal)
            {
              NSPOL_LOGERR ("STRCPY_S failed]ret=%d.", retVal);
              return -1;
            }

          retVal =
            STRCPY_S (p_stackx_port_zone->
                      stackx_one_port[port_index].linux_ip.mask_linux,
                      sizeof (p_stackx_port_zone->
                              stackx_one_port[port_index].linux_ip.
                              mask_linux), "0.0.0.0");

          if (EOK != retVal)
            {
              NSPOL_LOGERR ("STRCPY_S failed]ret=%d.", retVal);
              return -1;
            }

          retVal =
            STRCPY_S (p_stackx_port_zone->
                      stackx_one_port[port_index].linux_ip.bcast_linux,
                      sizeof (p_stackx_port_zone->
                              stackx_one_port[port_index].linux_ip.
                              bcast_linux), "0.0.0.0");

          if (EOK != retVal)
            {
              NSPOL_LOGERR ("STRCPY_S failed]ret=%d.", retVal);
              return -1;
            }

          ++port_index;
          NSPOL_LOGINF (SC_DPDK_INFO, "port_index=%u", port_index);

          if (CUR_CFG_HAL_PORT_NUM <= port_index + bond_ports_array.cnt)
            {
              // TODO: Invalid configuration received,  return immediately
              NSPOL_LOGERR
                ("Insufficient nStack configuration when compared to configuration from network.json");
              return -1;
            }

          /* [TA33636] [2017-04-11] Do not need provider.json */
          if (phynet->bond_mode ==
              -1 /*&& strncmp(network->network_name, "provider", 8) != 0 */ )
            {
              break;
            }
          else
            {
              phead = phead->next;
            }
        }

      network = network->next;
    }

  *port_num = port_index;

  return ERR_OK;
}

void
spl_hal_capa_init ()
{
  u32_t ipv4_cksum_offload = 1;
  u32_t udp_cksum_offload = 1;
  u32_t tcp_cksum_offload = 1;
  hal_netif_capa_t info = { 0 };
  struct stackx_port_info *p_port_info = head_used_port_list;

  while (p_port_info)
    {
      hal_get_capability (p_port_info->linux_ip.hdl, &info);

      if ((info.tx_offload_capa & HAL_ETH_TX_OFFLOAD_IPV4_CKSUM) == 0)
        {
          ipv4_cksum_offload = 0;

          NSPOL_LOGDBG (SC_DPDK_INFO, "Port %s TX_OFFLOAD_IPV4_CKSUM Disable",
                        p_port_info->linux_ip.if_name);
        }

      if ((info.tx_offload_capa & HAL_ETH_TX_OFFLOAD_UDP_CKSUM) == 0)
        {
          udp_cksum_offload = 0;

          NSPOL_LOGDBG (SC_DPDK_INFO, "Port %s TX_OFFLOAD_UDP_CKSUM Disable",
                        p_port_info->linux_ip.if_name);
        }

      if ((info.tx_offload_capa & HAL_ETH_TX_OFFLOAD_TCP_CKSUM) == 0)
        {
          tcp_cksum_offload = 0;

          NSPOL_LOGDBG (SC_DPDK_INFO, "Port %s TX_OFFLOAD_TCP_CKSUM Disable",
                        p_port_info->linux_ip.if_name);
        }

      p_port_info = p_port_info->next_use_port;
    }

  spl_hal_capa.tx_ipv4_cksum_offload = ipv4_cksum_offload;
  spl_hal_capa.tx_udp_cksum_offload = udp_cksum_offload;
  spl_hal_capa.tx_tcp_cksum_offload = tcp_cksum_offload;

  NSPOL_LOGINF (SC_DPDK_INFO,
                "ipv4_cksum_offload(%u),udp_cksum_offload(%u),tcp_cksum_offload(%u)",
                ipv4_cksum_offload, udp_cksum_offload, tcp_cksum_offload);

}

NSTACK_STATIC void
spl_hal_bond_info_init (hal_hdl_t hdl, struct bond_set *s,
                        struct stackx_port_info *p)
{
#define MAX_MAC_STR_LEN     20
  char mac_string[MAX_MAC_STR_LEN];
  int retVal;
  struct ether_addr addr;

  p->linux_ip.hdl = hdl;

  struct stackx_port_info *slave_port;
  slave_port = get_port_info_by_name (s->slave_ports[0]);

  if (slave_port == NULL)
    {
      NSPOL_LOGERR ("get_port_info_by_name failed]bond_port_name=%s",
                    s->bond_port_name);
      return;
    }

  /* check the lenght of bond_port_name */
  retVal =
    STRCPY_S (p->linux_ip.if_name, sizeof (p->linux_ip.if_name),
              s->bond_port_name);
  if (EOK != retVal)
    {
      NSPOL_LOGERR ("STRCPY_S failed]ret=%d", retVal);
      return;
    }

  hal_get_macaddr (hdl, &addr);
  retVal =
    spl_hal_ether_etoa (addr.addr_bytes, sizeof (addr.addr_bytes), mac_string,
                        sizeof (mac_string));
  if (retVal < 0)
    {
      NSPOL_LOGERR ("spl_hal_ether_etoa failed]ret=%d", retVal);
      return;
    }

  retVal =
    STRCPY_S (p->linux_ip.mac_addr, sizeof (p->linux_ip.mac_addr),
              mac_string);
  if (EOK != retVal)
    {
      NSPOL_LOGERR ("STRCPY_S failed]ret=%d", retVal);
      return;
    }

  retVal =
    STRCPY_S (p->linux_ip.ip_addr_linux, sizeof (p->linux_ip.ip_addr_linux),
              slave_port->linux_ip.ip_addr_linux);
  if (EOK != retVal)
    {
      NSPOL_LOGERR ("STRCPY_S failed]ret=%d", retVal);
      return;
    }

  retVal =
    STRCPY_S (p->linux_ip.mask_linux, sizeof (p->linux_ip.mask_linux),
              slave_port->linux_ip.mask_linux);
  if (EOK != retVal)
    {
      NSPOL_LOGERR ("STRCPY_S failed]ret=%d", retVal);
      return;
    }

  retVal =
    STRCPY_S (p->linux_ip.bcast_linux, sizeof (p->linux_ip.bcast_linux),
              slave_port->linux_ip.bcast_linux);
  if (EOK != retVal)
    {
      NSPOL_LOGERR ("STRCPY_S failed]ret=%d", retVal);
      return;
    }

  NSPOL_LOGINF (SC_DPDK_INFO, "===== the bond port info ======");
  NSPOL_LOGINF (SC_DPDK_INFO, "bond port name=%s", p->linux_ip.if_name);
  NSPOL_LOGINF (SC_DPDK_INFO, "bond port mac=%s", p->linux_ip.mac_addr);
  NSPOL_LOGINF (SC_DPDK_INFO, "bond port ip=%s", p->linux_ip.ip_addr_linux);
  NSPOL_LOGINF (SC_DPDK_INFO, "bond port netmask=%s", p->linux_ip.mask_linux);
  NSPOL_LOGINF (SC_DPDK_INFO, "bond port broad_cast addr=%s",
                p->linux_ip.bcast_linux);

}

NSTACK_STATIC int
spl_hal_bond_start (void)
{
  u8_t i, j = 0;
  struct stackx_port_info *bond_port = NULL;
  hal_hdl_t hdl;
  hal_hdl_t slave_hdl[HAL_MAX_SLAVES_PER_BOND];

  NSPOL_LOGINF (SC_DPDK_INFO, "bond_ports_array.cnt=%u",
                bond_ports_array.cnt);

  for (i = bond_ports_array_cnt_start; i < bond_ports_array.cnt; i++)
    {
      struct bond_set *s = &bond_ports_array.ports[i];
      NSPOL_LOGINF (SC_DPDK_INFO, "i=%u,bond_port_name=%s", i,
                    s->bond_port_name);

      u8_t slave_num = 0;
      for (j = 0; j < s->slave_port_cnt; j++)
        {
          NSPOL_LOGINF (SC_DPDK_INFO, "s->slave_ports[%u]=%s", j,
                        s->slave_ports[j]);
          hdl = get_port_hdl_by_name (s->slave_ports[j]);

          if (!hal_is_valid (hdl))
            {
              continue;
            }

          slave_hdl[slave_num++] = hdl;

          /* here we didn't release the port mem allocated in p_stackx_port_zone */
          del_port_in_port_list (s->slave_ports[j]);
        }

      hdl = hal_bond (s->bond_port_name, slave_num, slave_hdl);

      if (!hal_is_valid (hdl))
        {
          NSPOL_LOGERR ("hal_bond fail: bond_name =%s", s->bond_port_name);
          return -1;
        }

      bond_port =
        &p_stackx_port_zone->stackx_one_port[p_stackx_port_zone->port_num];
      num_ports_NIC++;
      p_stackx_port_zone->port_num++;
      p_stackx_port_zone->bonded_port_num++;

      spl_hal_bond_info_init (hdl, s, bond_port);
      add_port_in_port_list (bond_port);

    }

  bond_ports_array_cnt_start = bond_ports_array.cnt;
  return 0;
}

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */

NSTACK_STATIC inline int
spl_hal_port_start (uint16_t nic_id, struct stackx_port_info *p_port_info,
                    u16_t num_queues)
{
  u16_t num_queues_request, q;
  hal_hdl_t hdl;
  struct common_mem_mempool *mp;
  hal_netif_config_t conf;
#define MAX_MAC_STR_LEN     20
  char mac_string[MAX_MAC_STR_LEN];
  int retVal;
  struct ether_addr addr;

  // change the queues number per configuration.
  // even we only receive packets from one rx queue when dispatch mode is on, the tx queue
  // shoule set to the queues number requested.
  num_queues_request = num_queues;
  if (num_queues_request > HAL_ETH_MAX_QUEUE_NUM)
    {
      NSPOL_LOGERR
        ("no enougth  queue num for thread!]num_queues_request=%u,MAX_QUEUE_NUM=%u",
         num_queues_request, HAL_ETH_MAX_QUEUE_NUM);
      return -1;
    }

  NSPOL_LOGDBG (SC_DPDK_INFO, "# Initialising index=%s... ",
                p_port_info->linux_ip.if_name);
  /* used to have fflush,no use code ,remove it. */

  conf.bit.hw_vlan_filter = 1;
  conf.bit.hw_vlan_strip = 1;

  conf.rx.queue_num = num_queues_request;
  conf.tx.queue_num = num_queues_request;

  for (q = 0; q < num_queues_request; q++)
    {
      mp =
        (struct common_mem_mempool *) spl_hal_rx_pool_create (nic_id, q, 1);

      if (mp == NULL)
        {
          NSPOL_LOGERR
            ("spl_hal_rx_pool_create fail]mp=NULL,nic_id=%u,if_name=%s",
             nic_id, p_port_info->linux_ip.if_name);
          return -1;
        }

      (void) spl_reg_res_txrx_mgr ((mpool_handle *) mp);        // will only return 0, no need to check return value
      conf.rx.ring_pool[q] = mp;
      conf.rx.ring_size[q] = HAL_RX_RING_SIZE;
      conf.tx.ring_size[q] = HAL_TX_RING_SIZE;
    }

  hdl = hal_create (p_port_info->linux_ip.if_name, &conf);

  if (!hal_is_valid (hdl))
    {
      NSPOL_LOGERR ("hal_create fail]if_name =%s",
                    p_port_info->linux_ip.if_name);
      return -1;
    }

  p_port_info->linux_ip.hdl = hdl;

  /* add mac address  */
  hal_get_macaddr (hdl, &addr);
  retVal =
    spl_hal_ether_etoa (addr.addr_bytes, sizeof (addr.addr_bytes), mac_string,
                        sizeof (mac_string));
  if (retVal < 0)
    {
      NSPOL_LOGERR ("spl_hal_ether_etoa failed]ret=%d", retVal);
      return -1;
    }

  retVal =
    STRCPY_S (p_port_info->linux_ip.mac_addr,
              sizeof (p_port_info->linux_ip.mac_addr), mac_string);
  if (EOK != retVal)
    {
      NSPOL_LOGERR ("STRCPY_S failed]ret=%d", retVal);
      return -1;
    }

  return 0;
}

NSTACK_STATIC int
spl_hal_port_setup ()
{
  unsigned int i;
  struct stackx_port_info *p_port_info = NULL;

  INITPOL_LOGINF ("RTP", "spl_hal_port_setup", NULL_STRING, LOG_INVALID_VALUE,
                  MODULE_INIT_START);

  for (i = num_ports_NIC_start; i < num_ports_NIC; i++)
    {
      p_port_info = &(p_stackx_port_zone->stackx_one_port[i]);

      if (spl_hal_port_start (i, p_port_info, (u16_t) 1) < 0)
        {
          NSPOL_LOGERR ("Error initialising]nic_id=%u", i);

          INITPOL_LOGERR ("RTP", "spl_hal_port_setup", NULL_STRING,
                          LOG_INVALID_VALUE, MODULE_INIT_FAIL);

          return -1;
        }
      else
        {
          report_port_link_status (p_port_info);
          add_port_in_port_list (p_port_info);
        }
    }

  if (spl_hal_bond_start () < 0)
    {
      NSPOL_LOGERR ("bond port init failed!");

      INITPOL_LOGERR ("RTP", "spl_hal_port_setup", NULL_STRING,
                      LOG_INVALID_VALUE, MODULE_INIT_FAIL);

      return -1;
    }

  spl_hal_capa_init ();

  INITPOL_LOGINF ("RTP", "spl_hal_port_setup", NULL_STRING, LOG_INVALID_VALUE,
                  MODULE_INIT_SUCCESS);

  return 0;

}

int
spl_hal_port_init ()
{
  int retval;
  unsigned int i, port_num = 0;

  int port_num_start = p_stackx_port_zone->port_num;
  num_ports_NIC_start = num_ports_NIC;

  //Read network info
  INITPOL_LOGINF ("IP", "spl_hal_port_config", NULL_STRING, LOG_INVALID_VALUE,
                  MODULE_INIT_START);
  retval = spl_hal_port_config (&port_num);

  if (retval != ERR_OK)
    {
      INITPOL_LOGERR ("IP", "spl_hal_port_config", NULL_STRING,
                      LOG_INVALID_VALUE, MODULE_INIT_FAIL);
      return -1;
    }

  p_stackx_port_zone->port_num = port_num;

  NSPOL_LOGINF (SC_DPDK_INFO, "port_num=%u", port_num);
  INITPOL_LOGINF ("IP", "spl_hal_port_config", NULL_STRING, LOG_INVALID_VALUE,
                  MODULE_INIT_SUCCESS);

  if (port_num_start == p_stackx_port_zone->port_num)
    {
      NSPOL_LOGERR ("No new NIC find.");
      return 0;
    }

  //Get ports num
  for (i = port_num_start; i < p_stackx_port_zone->port_num; i++)
    {
      if (p_stackx_port_zone->stackx_one_port[i].linux_ip.if_name[0] != 0)
        {
          /* right now hard coded, */
          int eth_num =
            atoi (p_stackx_port_zone->stackx_one_port[i].linux_ip.if_name +
                  3);

          num_ports_NIC++;

          NSPOL_LOGDBG (SC_DPDK_INFO, "port_mask=%d ,eth_name=%s", eth_num,
                        p_stackx_port_zone->stackx_one_port[i].
                        linux_ip.if_name);
        }
    }

  if (num_ports_NIC > HAL_MAX_NIC_NUM)
    {
      NSPOL_LOGERR ("just support one eth");
      common_exit (EXIT_FAILURE, "just surport one eth");
    }

  if (num_ports_NIC == num_ports_NIC_start)
    {
      NSPOL_LOGERR ("No new NIC find.");
      return 0;
    }

  retval = spl_hal_port_setup ();

  if (retval == -1)
    {
      return -1;
    }

  NSPOL_LOGDBG (SC_DPDK_INFO, "Finished Process Init.");

  return 1;
}

inline NSTACK_STATIC void
spl_hal_send (struct netif *pnetif)
{
  u16_t i, sent = 0;
  struct netifExt *pnetifExt = NULL;
  u16_t netif_id = pnetif->num;
  u16_t tx_num = spl_hal_pkts[netif_id].tx.num;
  struct common_mem_mbuf **tx_ptks = spl_hal_pkts[netif_id].tx.pkts;

  for (i = 0; i < tx_num; i++)
    {
      (void)
        res_free (&
                  (((struct spl_pbuf *) (((char *) tx_ptks[i]) +
                                         sizeof (struct
                                                 common_mem_mbuf)))->res_chk));
    }

  int _retry = 0;

  pnetifExt = getNetifExt (pnetif->num);
  if (NULL == pnetifExt)
    return;

  do
    {
      sent +=
        hal_send_packet (pnetifExt->hdl, 0, &(tx_ptks[sent]), tx_num - sent);
      _retry++;

      if (_retry > SPL_HAL_SEND_TRY)
        {
          NSPOL_LOGERR ("send loop %d times but dpdk send data fail ",
                        SPL_HAL_SEND_TRY);
          break;
        }
    }
  while (unlikely (sent != tx_num));

  if (unlikely (sent != tx_num))
    {
      for (i = sent; i < tx_num; i++)
        {
          (void) nsfw_mem_mbf_free ((mbuf_handle) (tx_ptks[i]), NSFW_SHMEM);
        }
    }
  for (i = 0; i < tx_num; i++)
    {
      /* set dpdk_send flag */
      ((struct spl_pbuf *) (((char *) tx_ptks[i]) +
                            sizeof (struct common_mem_mbuf)))->
        res_chk.u8Reserve |= DPDK_SEND_FLAG;
    }

  spl_hal_pkts[netif_id].tx.num = 0;

}

inline u16_t
spl_hal_recv (struct netif *pnetif, u8_t id)
{
  u16_t netif_id, rx_c = 0;
  struct netifExt *pnetifExt = NULL;

  netif_id = pnetif->num;

  pnetifExt = getNetifExt (pnetif->num);
  if (NULL == pnetifExt)
    return 0;

  rx_c =
    hal_recv_packet (pnetifExt->hdl, 0, spl_hal_pkts[netif_id].rx.pkts,
                     PKT_BURST);

  if (rx_c <= 0)
    {
      return 0;
    }

  spl_hal_pkts[netif_id].rx.num = rx_c;
  spl_hal_pkts[netif_id].rx.index = 0;

  return rx_c;
}

/*needflush set 1 has pbuf release problem, ref maybe set 0 before release*/
NSTACK_STATIC inline void
spl_hal_set_cksum (struct spl_pbuf *buf, struct common_mem_mbuf *mbuf)
{

  //need to be careful, special when small packet oversize
  if (buf->tot_len > mbuf->pkt_len)
    {
      NSPOL_LOGWAR (SC_DPDK_INFO,
                    "small packet OVERSIZE]pbuf_len=%u,mbuf_len=%u", buf->len,
                    mbuf->pkt_len);
      mbuf->pkt_len = buf->len;
    }

  if (!spl_hal_tx_ip_cksum_enable () || !spl_hal_tx_tcp_cksum_enable ()
      || !spl_hal_tx_udp_cksum_enable ())
    {
      struct tcp_hdr *t_hdr;
      struct udp_hdr *u_hdr;
      u16_t flag_offset;
      u64_t ol_flags = (mbuf->ol_flags);        //& (~PKT_TX_L4_MASK));

      struct eth_hdr *ethhdr = (struct eth_hdr *) ((char *) buf->payload);

      if (ethhdr->type == 8)
        {
          struct ip_hdr *iphdr =
            (struct ip_hdr *) ((char *) buf->payload +
                               sizeof (struct eth_hdr));

          if (!spl_hal_tx_ip_cksum_enable ())
            {
              ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM;
              iphdr->_chksum = 0;
            }

          flag_offset = spl_ntohs (iphdr->_offset);

          /*ip frag, only the first packet has udp or tcp head */
          if (0 == (flag_offset & IP_OFFMASK))
            {
              switch (iphdr->_proto)
                {
                case IPPROTO_TCP:
                  if (!spl_hal_tx_tcp_cksum_enable ())
                    {
                      t_hdr =
                        (struct tcp_hdr *) ((char *) buf->payload +
                                            sizeof (struct eth_hdr) +
                                            sizeof (struct ip_hdr));
                      t_hdr->chksum = get_ipv4_psd_sum (iphdr, ol_flags);
                      ol_flags |= PKT_TX_TCP_CKSUM;
                    }

                  break;

                case IPPROTO_UDP:
                  {
                    if ((mbuf->ol_flags & PKT_TX_UDP_CKSUM) ==
                        PKT_TX_UDP_CKSUM)
                      {
                        u_hdr = (struct udp_hdr *) ((char *) buf->payload + sizeof (struct eth_hdr) + sizeof (struct ip_hdr));  //l2_len + l3_len);
                        u_hdr->chksum =
                          get_ipv4_psd_sum (iphdr, mbuf->ol_flags);
                      }
                  }

                  break;

                default:

                  break;
                }
            }
          mbuf->l2_len = sizeof (struct eth_hdr);       //l2_len;
          mbuf->l3_len = sizeof (struct ip_hdr);
          mbuf->ol_flags = ol_flags;
        }
    }
}

/*needflush set 1 has pbuf release problem, ref maybe set 0 before release*/
err_t
spl_hal_output (struct netif *pnetif, struct pbuf *buf)
{
  u16_t netif_id, idx;
  struct common_mem_mbuf *mbuf;
  struct spl_pbuf *spbuf = NULL;
  //spl_pbuf_layer layer = SPL_PBUF_TRANSPORT;
  //u16_t offset;

  if (!p_def_stack_instance)
    {
      NSPOL_LOGERR ("p_def_stack_instance is NULL");
      return -1;
    }

  u16_t proc_id = spl_get_lcore_id ();

  NSPOL_LOGINF (SC_DPDK_INFO, "spl_hal_output. len %d totlen %d", buf->len,
                buf->tot_len);
  print_pbuf_payload_info (buf, true);

  if (buf->tot_len > DEF_MBUF_DATA_SIZE)
    {
      NSPOL_LOGINF (TCP_DEBUG, "spl_pbuf_alloc_hugepage Failed!!!");
      return ERR_MEM;

    }
  spbuf = spl_pbuf_alloc_hugepage (SPL_PBUF_RAW,
                                   buf->tot_len,
                                   SPL_PBUF_HUGE, proc_id, NULL);

  if (!spbuf)
    {
      NSPOL_LOGINF (TCP_DEBUG, "spl_pbuf_alloc_hugepage Failed!!!");
      return ERR_MEM;
    }

  if (ERR_OK != pbuf_to_splpbuf_copy (spbuf, buf))
    {
      NSPOL_LOGERR ("pbuf to splpbuf copy failed");
      return -1;
    }

  mbuf =
    (struct common_mem_mbuf *) ((char *) spbuf -
                                sizeof (struct common_mem_mbuf));

  if (spbuf->tot_len > mbuf->pkt_len)
    {
      NSPOL_LOGWAR (SC_DPDK_INFO,
                    "small packet OVERSIZE]pbuf_len=%u,mbuf_len=%u",
                    spbuf->len, mbuf->pkt_len);
      mbuf->pkt_len = spbuf->len;
    }

  spl_hal_set_cksum (spbuf, mbuf);

  netif_id = pnetif->num;
  idx = spl_hal_pkts[netif_id].tx.num++;
  spl_hal_pkts[netif_id].tx.pkts[idx] = mbuf;
  spl_do_dump (spbuf, DUMP_SEND);
  spl_hal_send (pnetif);

  return 0;
}

void
spl_hal_input (struct netif *pnetif, struct spl_pbuf **buf)
{
  u16_t netif_id;

  struct common_mem_mbuf *mbuf;

  netif_id = pnetif->num;

  if (likely
      (spl_hal_pkts[netif_id].rx.num > spl_hal_pkts[netif_id].rx.index))
    {
      mbuf = spl_hal_pkts[netif_id].rx.pkts[spl_hal_pkts[netif_id].rx.index];
      spl_hal_pkts[netif_id].rx.index++;
      spl_hal_buf_convert (mbuf, buf);
      spl_do_dump (*buf, DUMP_RECV);
    }
  else
    {
      NSPOL_LOGERR
        ("recv from spl_dev has a problem]pnetif=%p, num=%u, index=%u",
         pnetif, spl_hal_pkts[netif_id].rx.num,
         spl_hal_pkts[netif_id].rx.index);
      *buf = NULL;
    }
  return;
}

int
spl_hal_tx_ip_cksum_enable ()
{
  return !spl_hal_capa.tx_ipv4_cksum_offload;
}

int
spl_hal_tx_udp_cksum_enable ()
{
  return !spl_hal_capa.tx_udp_cksum_offload;
}

int
spl_hal_tx_tcp_cksum_enable ()
{
  return !spl_hal_capa.tx_tcp_cksum_offload;
}

u32
spl_hal_is_nic_exist (const char *name)
{
  return hal_is_nic_exist (name);
}

int
spl_hal_is_bond_netif (struct netif *pnetif)
{
  int i;
  struct bond_set *s;
  struct netifExt *pnetifExt = NULL;

  pnetifExt = getNetifExt (pnetif->num);
  if (NULL == pnetifExt)
    return 0;

  for (i = 0; i < bond_ports_array.cnt; i++)
    {
      s = &bond_ports_array.ports[i];
      if (!strncmp
          (pnetifExt->if_name, s->bond_port_name, HAL_MAX_NIC_NAME_LEN))
        {
          return 1;
        }
    }

  return 0;
}
