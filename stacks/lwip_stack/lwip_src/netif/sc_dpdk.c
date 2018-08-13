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

#include "sc_dpdk.h"
#include "common_mem_mbuf.h"
#include "netif/common.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "nsfw_msg_api.h"
#include "nsfw_maintain_api.h"
#include "nsfw_recycle_api.h"
#include "stackx_app_res.h"
#include "stackx_pbuf.h"
#ifdef SYS_MEM_RES_STAT
#include "memp.h"
#endif
#include "spl_instance.h"
#ifdef HAL_LIB
#else
#include "rte_memzone.h"
#endif

#define SPL_MEM_MODULE "spl_mem_module"

#define TMR_TICK_LENGTH TCP_TMR_INTERVAL        /* define the tick length */

u32_t uStackArgIndex = 0;
int stackx_core_mask = 40;

int g_nstack_bind_cpu = 0;
int g_tcpip_thread_sleep_time = 0;

extern int sbr_create_tx_pool ();
extern int stackx_stat_zone_create ();

#define GLOBAL_STACK_CORE_ARG "-c"
#define GLOBAL_STACK_CORE_BINE "-bind_cpu"

u32 g_type;
struct memory_statics memory_used_size[80];

void
printmeminfo ()
{
  unsigned int i = 0;
  long size = 0;

  NSPOL_LOGDBG (SC_DPDK_INFO,
                "*************************************************************");
  for (i = 0; i < g_type; i++)
    {
      NSPOL_LOGDBG (SC_DPDK_INFO, "%s : %ld", memory_used_size[i].name,
                    memory_used_size[i].size);
      size += memory_used_size[i].size;
    }

  size += (g_type * sizeof (struct common_mem_memzone));
  NSPOL_LOGDBG (SC_DPDK_INFO, "total size %ld", size);
  NSPOL_LOGDBG (SC_DPDK_INFO,
                "*************************************************************");
}

void
print_call_stack ()
{
}

/* Parse the argument given in the command line of the application */
void
smp_parse_stack_args (int argc, char **argv)
{
  int i = 0;

  const unsigned int global_core_length = 2;    //GLOBAL_STACK_CORE_ARG "-c" string length is 2

  for (i = uStackArgIndex + 1; i < argc; i++)
    {
      if ((i + 1) < argc)
        {
          if (strncmp (argv[i], "-sleep", 6) == 0)      //compare "-sleep" string, length is 6
            {
              g_tcpip_thread_sleep_time = atoi (argv[++i]);
              NSPOL_LOGDBG (SC_DPDK_INFO, "g_tcpip_thread_sleep_time=%d",
                            g_tcpip_thread_sleep_time);
              continue;
            }

          if (strncmp (argv[i], GLOBAL_STACK_CORE_ARG, global_core_length) ==
              0)
            {
              stackx_core_mask = atoi (argv[++i]);
              if (stackx_core_mask < 1)
                {
                  NSPOL_LOGDBG (SC_DPDK_INFO,
                                "Invalid Args:core_mask can't be less than 1,input value is:%s",
                                argv[i]);
                }

              continue;
            }

          if (strncmp
              (argv[i], GLOBAL_STACK_CORE_BINE,
               sizeof (GLOBAL_STACK_CORE_BINE)) == 0)
            {
              if (argv[++i])
                {
                  g_nstack_bind_cpu = atoi (argv[i]);
                }

              if (g_nstack_bind_cpu < 0)
                {
                  g_nstack_bind_cpu = 0;
                }

              continue;
            }
        }
      else
        {
          NSPOL_LOGDBG (SC_DPDK_INFO, "Invalid args:%s miss value ", argv[i]);  //now ,only support this format ,others maybe supported in future
        }
    }

  return;
}

mpool_handle
create_tx_mbuf_pool ()
{
  mpool_handle mbf_pool_handle = NULL;

  nsfw_mem_mbfpool mbuf_pool;

  mbuf_pool.stname.entype = NSFW_SHMEM;
  int retval =
    spl_snprintf (mbuf_pool.stname.aname, NSFW_MEM_NAME_LENGTH - 1, "%s",
                  get_mempoll_tx_name ());
  if (retval < 0)
    {
      NSPOL_LOGERR ("spl_snprintf failed");
      return NULL;
    }

  mbuf_pool.usnum = TX_MBUF_POOL_SIZE - 1;
  mbuf_pool.uscash_size = 0;
  mbuf_pool.uspriv_size = 0;
  mbuf_pool.usdata_room = TX_MBUF_MAX_LEN;
  mbuf_pool.isocket_id = SOCKET_ID_ANY;
  mbuf_pool.enmptype = NSFW_MRING_SPSC;
  mbf_pool_handle = nsfw_mem_mbfmp_create (&mbuf_pool);
  if (NULL == mbf_pool_handle)
    {
      NSPOL_LOGERR ("create_tx_mbuf_pool failed]name=%s, num=%u, room=%u",
                    mbuf_pool.stname.aname, mbuf_pool.usnum,
                    mbuf_pool.usdata_room);
      return NULL;
    }

  NSPOL_LOGINF (SC_DPDK_INFO,
                "tx_mempool_malloc=%p, num=%u, room=%u, total_mem=%d",
                mbf_pool_handle, TX_MBUF_POOL_SIZE, mbuf_pool.usdata_room,
                nsfw_mem_get_len (mbf_pool_handle, NSFW_MEM_MBUF));
  DPDK_MEMORY_COUNT ((get_mempoll_tx_name ()),
                     nsfw_mem_get_len (mbf_pool_handle, NSFW_MEM_MBUF));
  MEM_STAT (SPL_MEM_MODULE, "spl_mbuf_pool", NSFW_SHMEM,
            nsfw_mem_get_len (mbf_pool_handle, NSFW_MEM_MBUF));

  return mbf_pool_handle;
}

mring_handle
create_segment_pool ()
{
  nsfw_mem_sppool seg_pool;
  seg_pool.stname.entype = NSFW_SHMEM;
  int retval =
    spl_snprintf (seg_pool.stname.aname, NSFW_MEM_NAME_LENGTH - 1, "%s",
                  get_mempoll_seg_name ());
  if (retval < 0)
    {
      NSPOL_LOGERR ("spl_snprintf failed");
      return NULL;
    }

  seg_pool.usnum = 16;
  seg_pool.useltsize = sizeof (struct common_pcb);
  seg_pool.isocket_id = SOCKET_ID_ANY;
  seg_pool.enmptype = NSFW_MRING_SPSC;

  mring_handle seg_mp_handle = nsfw_mem_sp_create (&seg_pool);
  if (NULL == seg_mp_handle)
    {
      NSPOL_LOGERR
        ("create_segment_pool common failed]name=%s, num=%u, size=%u",
         seg_pool.stname.aname, SPL_MEMP_NUM_TCP_SEG, seg_pool.useltsize);
      return NULL;
    }

  NSPOL_LOGINF (SC_DPDK_INFO,
                "common seg_mempool_malloc=%p, num=%u, size=%u, total_mem=%d",
                seg_mp_handle, SPL_MEMP_NUM_TCP_SEG, seg_pool.useltsize,
                nsfw_mem_get_len (seg_mp_handle, NSFW_MEM_SPOOL));
  DPDK_MEMORY_COUNT ((get_mempoll_seg_name ()),
                     nsfw_mem_get_len (seg_mp_handle, NSFW_MEM_SPOOL));
  MEM_STAT (SPL_MEM_MODULE, "spl_seg_pool", NSFW_SHMEM,
            nsfw_mem_get_len (seg_mp_handle, NSFW_MEM_SPOOL));
  return seg_mp_handle;
}

mring_handle
create_msg_pool ()
{
  nsfw_mem_sppool msg_pool;
  msg_pool.stname.entype = NSFW_SHMEM;
  int retval =
    spl_snprintf (msg_pool.stname.aname, NSFW_MEM_NAME_LENGTH - 1, "%s",
                  get_mempoll_msg_name ());
  if (retval < 0)
    {
      NSPOL_LOGERR ("spl_snprintf fail");
      return NULL;
    }

  msg_pool.usnum = TX_MSG_POOL_SIZE;
  msg_pool.useltsize = sizeof (data_com_msg);
  msg_pool.isocket_id = SOCKET_ID_ANY;
  msg_pool.enmptype = NSFW_MRING_MPMC;
  mring_handle msg_mp_handle = nsfw_mem_sp_create (&msg_pool);

  if (NULL == msg_mp_handle)
    {
      NSPOL_LOGERR ("create_msg_pool failed]name=%s, num=%u, size=%u",
                    msg_pool.stname.aname, TX_MSG_POOL_SIZE,
                    msg_pool.useltsize);
      return NULL;
    }

  NSPOL_LOGINF (SC_DPDK_INFO,
                "msg_pool_malloc=%p, num=%u, size=%u, total_mem=%d",
                msg_mp_handle, TX_MSG_POOL_SIZE, msg_pool.useltsize,
                nsfw_mem_get_len (msg_mp_handle, NSFW_MEM_SPOOL));
  DPDK_MEMORY_COUNT ((get_mempoll_msg_name ()),
                     nsfw_mem_get_len (msg_mp_handle, NSFW_MEM_SPOOL));
  MEM_STAT (SPL_MEM_MODULE, "spl_msg_pool", NSFW_SHMEM,
            nsfw_mem_get_len (msg_mp_handle, NSFW_MEM_SPOOL));
  return msg_mp_handle;
}

mring_handle
create_primary_box ()
{
  nsfw_mem_mring mbox_pool;
  mbox_pool.stname.entype = NSFW_SHMEM;
  int retval =
    spl_snprintf (mbox_pool.stname.aname, NSFW_MEM_NAME_LENGTH - 1, "%s",
                  get_stackx_ring_name ());
  if (retval < 0)
    {
      NSPOL_LOGERR ("spl_snprintf failed");
      return NULL;
    }

  mbox_pool.usnum = MBOX_RING_SIZE - 1;
  mbox_pool.isocket_id = SOCKET_ID_ANY;
  mbox_pool.enmptype = NSFW_MRING_MPSC;
  mring_handle mbox_handle = nsfw_mem_ring_create (&mbox_pool);
  if (NULL == mbox_handle)
    {
      NSPOL_LOGERR ("create_primary_mbox failed]name=%s, num=%u",
                    mbox_pool.stname.aname, mbox_pool.usnum + 1);
      return NULL;
    }

  NSPOL_LOGINF (SC_DPDK_INFO, "primary_mbox_malloc=%p, num=%u, total_mem=%d",
                mbox_handle, MBOX_RING_SIZE,
                (nsfw_mem_get_len (mbox_handle, NSFW_MEM_RING)));
  DPDK_MEMORY_COUNT ((get_stackx_ring_name ()),
                     (nsfw_mem_get_len (mbox_handle, NSFW_MEM_RING)));
  MEM_STAT (SPL_MEM_MODULE, "primary_mbox_ring", NSFW_SHMEM,
            (nsfw_mem_get_len (mbox_handle, NSFW_MEM_RING)));
  return mbox_handle;
}

mring_handle
create_priority_box (u32 prio)
{
  nsfw_mem_mring mbox_pool;
  mbox_pool.stname.entype = NSFW_SHMEM;
  int retval =
    spl_snprintf (mbox_pool.stname.aname, NSFW_MEM_NAME_LENGTH - 1, "%s",
                  get_stackx_priority_ring_name (prio));
  if (retval < 0)
    {
      NSPOL_LOGERR ("spl_snprintf failed");
      return NULL;
    }

  mbox_pool.usnum = MBOX_RING_SIZE - 1;
  mbox_pool.isocket_id = SOCKET_ID_ANY;
  mbox_pool.enmptype = NSFW_MRING_MPSC;
  mring_handle mbox_handle = nsfw_mem_ring_create (&mbox_pool);
  if (NULL == mbox_handle)
    {
      NSPOL_LOGERR ("Create priority mbox fail]name=%s, num=%u",
                    mbox_pool.stname.aname, mbox_pool.usnum + 1);
      return NULL;
    }

  NSPOL_LOGINF (SC_DPDK_INFO, "prio=%u, mbox=%p, num=%u, total_mem=%d", prio,
                mbox_handle, MBOX_RING_SIZE,
                (nsfw_mem_get_len (mbox_handle, NSFW_MEM_RING)));
  DPDK_MEMORY_COUNT ((get_stackx_priority_ring_name (prio)),
                     (nsfw_mem_get_len (mbox_handle, NSFW_MEM_RING)));
  MEM_STAT (SPL_MEM_MODULE, mbox_pool.stname.aname, NSFW_SHMEM,
            (nsfw_mem_get_len (mbox_handle, NSFW_MEM_RING)));
  return mbox_handle;

}

int
init_instance ()
{
  int ret;
  p_def_stack_instance =
    (stackx_instance *) malloc (sizeof (stackx_instance));
  if (NULL == p_def_stack_instance)
    {
      NSPOL_LOGERR ("malloc failed");
      return -1;
    }

  ret = MEMSET_S (p_def_stack_instance, sizeof (stackx_instance), 0,
                  sizeof (stackx_instance));
  if (EOK != ret)
    {
      NSPOL_LOGERR ("MEMSET_S failed]ret=%d", ret);
      return -1;
    }

  p_def_stack_instance->rss_queue_id = 0;
  p_def_stack_instance->netif_list = NULL;
  p_def_stack_instance->mp_tx = create_tx_mbuf_pool ();
  if (!p_def_stack_instance->mp_tx)
    {
      return -1;
    }

  (void) spl_reg_res_tx_mgr (p_def_stack_instance->mp_tx);      // will only return 0, no need to check return value

  /* Modified above code to hold common_pcb */
  p_def_stack_instance->cpcb_seg = create_segment_pool ();
  if (!p_def_stack_instance->cpcb_seg)
    {
      return -1;
    }

  p_def_stack_instance->lmsg_pool = create_msg_pool ();
  if (!p_def_stack_instance->lmsg_pool)
    {
      return -1;
    }

  mring_handle mbox_array[SPL_MSG_BOX_NUM] = { NULL };
  p_def_stack_instance->lstack.primary_mbox.llring = create_primary_box ();
  if (!p_def_stack_instance->lstack.primary_mbox.llring)
    {
      return -1;
    }
  mbox_array[0] = p_def_stack_instance->lstack.primary_mbox.llring;

  u32 m = 0;
  while (m < MSG_PRIO_QUEUE_NUM)
    {
      p_def_stack_instance->lstack.priority_mbox[m].llring =
        create_priority_box (m);
      if (!p_def_stack_instance->lstack.priority_mbox[m].llring)
        {
          return -1;
        }
      mbox_array[m + 1] =
        p_def_stack_instance->lstack.priority_mbox[m].llring;
      m++;
    }

  (void) spl_add_mbox (mbox_array, SPL_MSG_BOX_NUM);

  g_nsfw_rti_primary_stat = &p_def_stack_instance->lstat.primary_stat;  //save to g_nsfw_rti_primary_stat(this is a SH addr)
  return 0;
}

void
spl_free_msgs_in_box (mring_handle r)
{
  i32 count = 0, i = 0;

  void **msgs = NULL;
  data_com_msg *m = NULL;

  while ((count = nsfw_mem_ring_dequeuev (r, msgs, 32)) > 0)
    {
      /* drop all of them */
      if (msgs == NULL)
        break;

      for (i = 0; i < count; i++)
        {
          m = (data_com_msg *) msgs[i];
          if (m->param.op_type == MSG_ASYN_POST)
            ASYNC_MSG_FREE (m);
          else
            SYNC_MSG_ACK (m);
        }
    }
}

inline int
spl_msg_malloc (data_com_msg ** p_msg_entry)
{
  mring_handle msg_pool = NULL;
  int rslt;
  stackx_instance *instance = p_def_stack_instance;
  msg_pool = instance->lmsg_pool;
  if (!msg_pool)
    {
      NSPOL_LOGERR ("msg_pool is NULL");
      return -1;
    }

  rslt = nsfw_mem_ring_dequeue (msg_pool, (void **) p_msg_entry);
  if ((rslt == 0) || (*p_msg_entry == NULL))
    {
      NSPOL_LOGERR ("failed to get msg from ring");
      return -1;
    }

  res_alloc (&(*p_msg_entry)->param.res_chk);

  (*p_msg_entry)->param.msg_from = msg_pool;
  (*p_msg_entry)->param.err = ERR_OK;
  return 0;
}

struct spl_pbuf *
spl_mbuf_malloc (uint16_t len, spl_pbuf_type Type, u16_t * count)
{
  struct common_mem_mbuf *mbuf = NULL;
  struct common_mem_mbuf *mbuf_first = NULL;
  struct common_mem_mbuf *mbuf_tail = NULL;
  struct spl_pbuf *buf = NULL;
  struct spl_pbuf *first = NULL;
  struct spl_pbuf *tail = NULL;

  mpool_handle mp = NULL;

  mp = p_def_stack_instance->mp_tx;
  if (mp == NULL)
    {
      return NULL;              /*if mp is NULL when init app will Inform */
    }

  while (len > 0)
    {
      mbuf = (struct common_mem_mbuf *) nsfw_mem_mbf_alloc (mp, NSFW_SHMEM);
      if (unlikely (mbuf == NULL))
        {
          if (mbuf_first != NULL)
            {
              if (res_free
                  (&
                   (((struct spl_pbuf *) ((char *) mbuf_first +
                                          sizeof (struct
                                                  common_mem_mbuf)))->res_chk)))
                {
                  NSPOL_LOGERR ("res_free failed");
                }
              spl_mbuf_free (mbuf_first);
            }

          return NULL;
        }

      uint16_t alloc = TX_MBUF_MAX_LEN;
      if (len < TX_MBUF_MAX_LEN)
        {
          alloc = len;
        }

      (*count)++;
      mbuf->data_len = alloc;
      mbuf->next = NULL;
      buf =
        (struct spl_pbuf *) ((char *) mbuf + sizeof (struct common_mem_mbuf));
      res_alloc (&buf->res_chk);

      buf->next_a = 0;
      buf->payload_a = ADDR_LTOSH_EXT (common_pktmbuf_mtod (mbuf, void *));
      buf->tot_len = len;
      buf->len = alloc;
      buf->type = Type;
      buf->flags = 0;

      buf->freeNext = NULL;

      buf->conn_a = 0;

      if (first == NULL)
        {
          first = buf;
          mbuf_first = mbuf;
          tail = buf;
          mbuf_tail = mbuf;
          mbuf_first->nb_segs = 1;
          mbuf_first->pkt_len = alloc;
        }
      else
        {
          /* Already there is a check for the return value of rtp_pktmbuf_alloc,
             hence not an issue */

          tail->next_a = ADDR_LTOSH_EXT (buf);
          tail = buf;
#ifdef HAL_LIB
#else
          mbuf_tail->next = mbuf;
#endif
          mbuf_tail = mbuf;

          mbuf_first->pkt_len = (mbuf_first->pkt_len + mbuf->data_len);
          mbuf_first->nb_segs++;

        }

      len -= alloc;
    }

  return first;
}

/*
  * Ring distribution function: protocol stack once a packet processing, so there is no use of bulk package
  * @param buf pbuf means * @param packet_inport the packet from which the port to enter, for the configuration table with the ip comparison
  * @ Protocol stack add location: ip.c-> ip_input () -> (if (netif == NULL) branch)
  * @ Return value: 0 for the send into * 0 for the transmission failed: send the original failure 1,
  *   err = -20 did not match to the client, err = -1Ring full, overflow, will release the package
*/

inline void
spl_mbuf_free (void *mbuf)
{
  (void) nsfw_mem_mbf_free ((mbuf_handle) mbuf, NSFW_SHMEM);
}

inline uint16_t
spl_mbuf_refcnt_update (void *mbuf, int16_t value)
{
  common_mbuf_refcnt_set ((struct common_mem_mbuf *) mbuf,
                          common_mbuf_refcnt_read ((struct common_mem_mbuf *)
                                                   mbuf) + value);
  return 1;
}
