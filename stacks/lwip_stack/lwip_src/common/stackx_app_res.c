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

#include "stackx_app_res.h"
#include "nstack_securec.h"
#include "common_pal_bitwide_adjust.h"
#include "nstack_log.h"
#include "stackx_common.h"
#include "nsfw_maintain_api.h"
#include "stackx_tx_box.h"
#include "nsfw_msg_api.h"
#include "nsfw_recycle_api.h"
#include "common_mem_mbuf.h"
#include "stackx_pbuf.h"
#include "nsfw_mt_config.h"
#include "nsfw_mem_api.h"
#include "spl_opt.h"
#define SPL_MAX_MSG_NUM (MBOX_RING_SIZE*8*MAX_THREAD_NUM)

spl_app_res_group *g_spl_app_res_group = NULL;

spl_app_res_group_array *g_res_group_array = NULL;

#ifdef SYS_MEM_RES_STAT
mpool_handle g_app_tx_pool[SBR_TX_POOL_NUM];
#endif

/***************************************************
*   description:
***************************************************/
int
spl_init_group_array ()
{
  g_res_group_array =
    (spl_app_res_group_array *) sbr_create_mzone (SPL_RES_GROUP_ARRAY,
                                                  sizeof
                                                  (spl_app_res_group_array));
  if (!g_res_group_array)
    {
      NSPOL_LOGERR ("create g_res_group_array failed");
      return -1;
    }

  u32 i;
  for (i = 0; i < SBR_TX_POOL_NUM; ++i)
    {
      g_res_group_array->pid_array[i] = 0;
    }

  for (i = 0; i < MAX_THREAD_NUM; ++i)
    {
      g_res_group_array->res_group[i] = NULL;
    }

  g_res_group_array->thread_num = 0;
  return 0;
}

/***************************************************
*   description:
***************************************************/
int
sbr_attach_group_array ()
{
  g_res_group_array =
    (spl_app_res_group_array *) sbr_lookup_mzone (SPL_RES_GROUP_ARRAY);
  if (!g_res_group_array)
    {
      NSPOL_LOGERR ("attach g_res_group_array failed");
      return -1;
    }

  return 0;
}

/***************************************************
*   description:
***************************************************/
int
spl_add_instance_res_group (u32 thread_index, spl_app_res_group * group)
{
  if (thread_index >= MAX_THREAD_NUM)
    {
      NSPOL_LOGERR
        ("thread_index >= MAX_THREAD_NUM]thread_index=%u, MAX_THREAD_NUM=%u",
         thread_index, MAX_THREAD_NUM);
      return -1;
    }

  if (g_res_group_array->res_group[thread_index] != NULL)
    {
      NSPOL_LOGERR
        ("g_res_group_array in thread_index is not NULL, this can not happen");
      return -1;
    }

  g_res_group_array->res_group[thread_index] = group;
  __sync_add_and_fetch (&g_res_group_array->thread_num, 1);
  return 0;
}

/***************************************************
*   description:
***************************************************/
int
spl_add_mbox (mring_handle mbox_array[], u32 array_size)
{
  if (array_size != SPL_MSG_BOX_NUM)
    {
      NSPOL_LOGERR ("array_size must be %u, but not", SPL_MSG_BOX_NUM);
      return -1;
    }

  u32 i;
  for (i = 0; i < array_size; ++i)
    {
      g_spl_app_res_group->mbox_array[i] = mbox_array[i];
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : spl_create_group
*   Description  : create group
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
spl_app_res_group *
spl_create_group ()
{
  spl_app_res_group *group =
    (spl_app_res_group *) sbr_create_mzone (SPL_APP_RES_GROUP_NAME,
                                            sizeof (spl_app_res_group));
  if (!group)
    {
      NSPOL_LOGERR ("Create app_res_group zone fail]name=%s, size=%u",
                    SPL_APP_RES_GROUP_NAME, sizeof (spl_app_res_group));
      return NULL;
    }

  group->msg_pool = NULL;
  group->conn_pool = NULL;
  group->conn_array = NULL;
  group->recv_ring_pool = NULL;

  u32 i;
  for (i = 0; i < SBR_TX_POOL_NUM; ++i)
    {
      group->tx_pool_array[i] = NULL;
    }

  for (i = 0; i < SPL_MSG_BOX_NUM; ++i)
    {
      group->mbox_array[i] = NULL;
    }

  group->extend_member_bit = 0;

  NSPOL_LOGINF (SC_DPDK_INFO, "Create app_res_group zone ok]name=%s, size=%u",
                SPL_APP_RES_GROUP_NAME, sizeof (spl_app_res_group));
  MEM_STAT (SPL_APP_RES, SPL_APP_RES_GROUP_NAME, NSFW_SHMEM,
            sizeof (spl_app_res_group));
  return group;
}

/*****************************************************************************
*   Prototype    : _spl_create_ring_pool
*   Description  : create ring pool
*   Input        : char* pool_name
*                  char* array_name
*                  nsfw_mpool_type type
*   Output       : None
*   Return Value : static inline mring_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline mring_handle
_spl_create_ring_pool (char *pool_name, char *array_name,
                       nsfw_mpool_type type, u32 num, u32 ring_size)
{
  mring_handle pool = sbr_create_ring (pool_name, num - 1);

  if (!pool)
    {
      return NULL;
    }

  NSPOL_LOGINF (SC_DPDK_INFO, "Create ring pool ok]name=%s, num=%u, size=%d",
                pool_name, num, nsfw_mem_get_len (pool, NSFW_MEM_RING));
  MEM_STAT (SPL_APP_RES, pool_name, NSFW_SHMEM,
            nsfw_mem_get_len (pool, NSFW_MEM_RING));

  mring_handle *array = malloc (num * sizeof (mring_handle));
  if (!array)
    {
      NSPOL_LOGERR ("malloc fail]size=%u", num * sizeof (mring_handle));
      return NULL;
    }

  if (sbr_create_multi_ring (array_name, ring_size - 1, num,
                             array, type) != 0)
    {
      free (array);
      return NULL;
    }

  NSPOL_LOGINF (SC_DPDK_INFO,
                "Create multi rings ok]name=%s, ring_size=%u, ring_num=%u, total_mem=%d",
                array_name, ring_size, num,
                (nsfw_mem_get_len (array[0], NSFW_MEM_RING) * num));
  MEM_STAT (SPL_APP_RES, array_name, NSFW_SHMEM,
            nsfw_mem_get_len (array[0], NSFW_MEM_RING) * num);

  unsigned int i = 0;
  while (i < num)
    {
      if (nsfw_mem_ring_enqueue (pool, (void *) array[i]) != 1)
        {
          NSPOL_LOGERR ("nsfw_mem_ring_enqueue failed,this can not happen");
          free (array);
          return NULL;
        }

      i++;
    }

  free (array);
  return pool;
}

/*****************************************************************************
*   Prototype    : spl_create_ring_pool
*   Description  : create ring pool
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
spl_create_ring_pool (spl_app_res_group * group)
{
  group->recv_ring_pool =
    _spl_create_ring_pool (SPL_RECV_RING_POOL_NAME, SPL_RECV_RING_ARRAY_NAME,
                           NSFW_MRING_SPSC, CUR_CFG_SOCKET_NUM,
                           SPL_MAX_RING_SIZE);
  if (!group->recv_ring_pool)
    {
      NSPOL_LOGERR ("Create recv ring pool failed");
      return -1;
    }

  NSPOL_LOGINF (SC_DPDK_INFO, "Create recv ring pool ok]name=%s",
                SPL_RECV_RING_POOL_NAME);
  return 0;
}

int
spl_force_netconn_free (void *data)
{
  spl_netconn_t *conn = (spl_netconn_t *) data;
  if (TRUE == conn->res_chk.alloc_flag)
    {
      if (NULL != conn->recycle.accept_from)
        {
          return FALSE;
        }

      if (TRUE != nsfw_pidinfo_empty (&conn->recycle.pid_info))
        {
          return FALSE;
        }
    }
  ss_reset_conn (conn);
  (void) res_free (&conn->res_chk);

  if (nsfw_mem_ring_enqueue (ss_get_conn_pool (conn), (void *) conn) != 1)
    {
      NSSBR_LOGERR ("nsfw_mem_ring_enqueue failed,this can not happen");
    }
  NSFW_LOGINF ("force free conn]conn=%p", conn);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : spl_create_netconn_pool
*   Description  : create netconn pool
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
spl_create_netconn_pool (spl_app_res_group * group)
{
  spl_netconn_t **conn_array =
    (spl_netconn_t **) sbr_create_mzone (SPL_CONN_ARRAY_NAME,
                                         sizeof (spl_netconn_t *) *
                                         CUR_CFG_SOCKET_NUM);

  if (!conn_array)
    {
      return -1;
    }

  NSPOL_LOGINF (SC_DPDK_INFO, "Create connn_array zone ok]name=%s, size=%zu",
                SPL_CONN_ARRAY_NAME,
                sizeof (spl_netconn_t *) * CUR_CFG_SOCKET_NUM);
  mring_handle pool =
    sbr_create_pool (SPL_CONN_POOL_NAME, CUR_CFG_SOCKET_NUM - 1,
                     SBR_FD_NETCONN_SIZE);

  if (!pool)
    {
      return -1;
    }

  NSPOL_LOGINF (SC_DPDK_INFO,
                "Create conn_pool ok]name=%s, num=%u, total_mem=%d",
                SPL_CONN_POOL_NAME, CUR_CFG_SOCKET_NUM,
                nsfw_mem_get_len (pool, NSFW_MEM_SPOOL));

  sbr_recycle_group *recycle_group =
    (sbr_recycle_group *) sbr_create_mzone (SPL_RECYCLE_GROUP,
                                            sizeof (sbr_recycle_group));
  if (!recycle_group)
    {
      return -1;
    }

  NSPOL_LOGINF (SC_DPDK_INFO, "Create recycle_group zone ok]name=%s, size=%u",
                SPL_RECYCLE_GROUP, sizeof (sbr_recycle_group));

  recycle_group->conn_array = conn_array;
  recycle_group->conn_num = spl_get_conn_num ();
  recycle_group->conn_pool = pool;
  recycle_group->extend_member_bit = 0;
  recycle_group->msg_pool = group->msg_pool;

  spl_netconn_t *conn = NULL;
  unsigned int i = 0;
  while (i < CUR_CFG_SOCKET_NUM)
    {
      if (nsfw_mem_ring_dequeue (pool, (void **) &conn) != 1)
        {
          NSPOL_LOGERR ("nsfw_mem_ring_dequeue failed,this can not happen");
          return -1;
        }

      ss_reset_conn (conn);
      conn->recycle.group = recycle_group;

      if (nsfw_mem_ring_dequeue (group->recv_ring_pool, &conn->recv_ring) !=
          1)
        {
          NSPOL_LOGERR ("nsfw_mem_ring_dequeue failed,this can not happen");
          return -1;
        }

      conn_array[i] = conn;

      if (nsfw_mem_ring_enqueue (pool, (void *) conn) != 1)
        {
          NSPOL_LOGERR ("nsfw_mem_ring_enqueue failed,this can not happen");
          return -1;
        }

      i++;
    }

  group->conn_pool = pool;
  group->conn_array = conn_array;

  MEM_STAT (SPL_APP_RES, SPL_RECYCLE_GROUP, NSFW_SHMEM,
            sizeof (sbr_recycle_group));
  MEM_STAT (SPL_APP_RES, SPL_CONN_ARRAY_NAME, NSFW_SHMEM,
            sizeof (spl_netconn_t *) * CUR_CFG_SOCKET_NUM);
  MEM_STAT (SPL_APP_RES, SPL_CONN_POOL_NAME, NSFW_SHMEM,
            nsfw_mem_get_len (group->conn_pool, NSFW_MEM_SPOOL));
  return 0;
}

int
spl_force_msg_free (void *data)
{
  data_com_msg *m = (data_com_msg *) data;
  if (NULL == m)
    {
      return FALSE;
    }

  NSFW_LOGINF ("force free msg]msg=%p", data);
  (void) res_free (&m->param.res_chk);
  m->param.recycle_pid = 0;
  if (nsfw_mem_ring_enqueue (m->param.msg_from, (void *) m) != 1)
    {
      NSFW_LOGERR ("nsfw_mem_ring_enqueue failed,this can not happen");
    }
  return TRUE;
}

/*****************************************************************************
*   Prototype    : spl_create_msg_pool
*   Description  : create msg pool
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
spl_create_msg_pool (spl_app_res_group * group)
{
  mring_handle pool =
    sbr_create_pool (SPL_MSG_POOL_NAME, SPL_MAX_MSG_NUM - 1, MAX_MSG_SIZE);

  if (!pool)
    {
      return -1;
    }

  data_com_msg *m = NULL;
  unsigned int i = 0;
  while (i < SPL_MAX_MSG_NUM)
    {
      if (nsfw_mem_ring_dequeue (pool, (void **) &m) != 1)
        {
          NSPOL_LOGERR ("nsfw_mem_ring_dequeue failed,this can not happen");
          return -1;
        }

      m->param.msg_from = pool;
      m->param.recycle_pid = 0;
      sys_sem_init (&m->param.op_completed);

      if (nsfw_mem_ring_enqueue (pool, (void *) m) != 1)
        {
          NSPOL_LOGERR ("nsfw_mem_ring_enqueue failed,this can not happen");
          return -1;
        }

      i++;
    }

  group->msg_pool = pool;

  NSPOL_LOGINF (SC_DPDK_INFO,
                "Create app msg pool ok]name=%s, num=%u, size=%u, total_mem=%d",
                SPL_MSG_POOL_NAME, SPL_MAX_MSG_NUM, MAX_MSG_SIZE,
                nsfw_mem_get_len (group->msg_pool, NSFW_MEM_SPOOL));
  MEM_STAT (SPL_APP_RES, SPL_MSG_POOL_NAME, NSFW_SHMEM,
            nsfw_mem_get_len (group->msg_pool, NSFW_MEM_SPOOL));
  return 0;
}

/*****************************************************************************
*   Prototype    : spl_regist_recycle
*   Description  : regist recycle
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
spl_regist_recycle (spl_app_res_group * group)
{
  void *ret =
    nsfw_recycle_reg_obj (NSFW_REC_PRO_DEFAULT, NSFW_REC_SBR_SOCKET, NULL);

  if (!ret)
    {
      NSSBR_LOGERR ("failed");
      return -1;
    }

  if (NULL == group)
    {
      NSSBR_LOGERR ("g_spl_app_res_group null");
      return -1;
    }

  nsfw_res_scn_cfg scn_cfg = { NSFW_RES_SCAN_SPOOL, 90, 3, 16,
    CUR_CFG_SOCKET_NUM / 128, CUR_CFG_SOCKET_NUM - 1,
    SBR_FD_NETCONN_SIZE,
    offsetof (spl_netconn_t, res_chk),
    (void *) group->conn_pool,
    (void *) group->conn_pool,
    spl_force_netconn_free
  };

  (void) nsfw_res_mgr_reg (&scn_cfg);

  nsfw_res_scn_cfg scn_cfg_msg = { NSFW_RES_SCAN_SPOOL, 60, 3, 16,
    SPL_MAX_MSG_NUM / 128, SPL_MAX_MSG_NUM,
    MAX_MSG_SIZE,
    offsetof (data_com_msg, param.res_chk),
    (void *) group->msg_pool,
    (void *) group->msg_pool,
    spl_force_msg_free
  };
  (void) nsfw_res_mgr_reg (&scn_cfg_msg);

  return 0;
}

void
spl_reg_tx_pool_mgr (mpool_handle pool_array[], u32 num)
{
  u32 loop;
  for (loop = 0; loop < num; loop++)
    {
      (void) spl_reg_res_txrx_mgr (pool_array[loop]);   // will only return 0, no need to check return value
    }

  return;
}

/*****************************************************************************
*   Prototype    : sbr_create_tx_pool
*   Description  : create tx pool,spl call this fun
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
spl_create_tx_pool (mpool_handle pool_array[], u32 array_size)
{
  data_com_msg *tx_msg_array =
    (data_com_msg *) sbr_create_mzone (SBR_TX_MSG_ARRAY_NAME,
                                       (size_t) sizeof (data_com_msg) *
                                       SBR_TX_MSG_NUM);

  if (!tx_msg_array)
    {
      NSSBR_LOGERR ("Create tx_msg_array zone fail]name=%s, num=%u, size=%u",
                    SBR_TX_MSG_ARRAY_NAME, SBR_TX_MSG_NUM,
                    (size_t) sizeof (data_com_msg) * SBR_TX_MSG_NUM);
      return -1;
    }

  MEM_STAT (SBR_TX_POOL_NAME, SBR_TX_MSG_ARRAY_NAME, NSFW_SHMEM,
            (size_t) sizeof (data_com_msg) * SBR_TX_MSG_NUM);
  NSSBR_LOGINF
    ("Create tx_msg_array zone ok]name=%s, ptr=%p, num=%u, size=%u",
     SBR_TX_MSG_ARRAY_NAME, tx_msg_array, SBR_TX_MSG_NUM,
     sizeof (data_com_msg) * SBR_TX_MSG_NUM);

  mpool_handle mp;
  int ret;
  char tx_pool_name[64];
  u32 loop;
  for (loop = 0; loop < array_size; loop++)
    {
      ret =
        SPRINTF_S (tx_pool_name, sizeof (tx_pool_name), "%s_%d",
                   SBR_TX_POOL_NAME, loop);
      if (-1 == ret)
        {
          NSSBR_LOGERR ("SPRINTF_S failed]ret=%d", ret);
          return -1;
        }

      nsfw_mem_mbfpool pool_param;
      if (STRCPY_S
          (pool_param.stname.aname, NSFW_MEM_NAME_LENTH, tx_pool_name) != 0)
        {
          NSSBR_LOGERR ("STRCPY_S failed]name=%s", tx_pool_name);
          return -1;
        }

      pool_param.isocket_id = -1;
      pool_param.stname.entype = NSFW_SHMEM;
      pool_param.enmptype = NSFW_MRING_MPMC;
      pool_param.uscash_size = 0;
      pool_param.uspriv_size = 0;
      pool_param.usdata_room = TX_MBUF_MAX_LEN;
      pool_param.usnum = SBR_TX_POOL_MBUF_NUM - 1;

      mp = nsfw_mem_mbfmp_create (&pool_param);
      if (!mp)
        {
          NSSBR_LOGERR ("Create tx_mbuf_pool fail]name=%s, num=%u, room=%u",
                        tx_pool_name, SBR_TX_POOL_MBUF_NUM,
                        pool_param.usdata_room);
          return -1;
        }
      else
        {
          struct common_mem_mbuf *mbuf = NULL;
          struct spl_pbuf *buf = NULL;
          int i = 0;
          while (i < (int) SBR_TX_POOL_MBUF_NUM)
            {
              mbuf = nsfw_mem_mbf_alloc (mp, NSFW_SHMEM);
              if (!mbuf)
                {
                  NSSBR_LOGERR
                    ("nsfw_mem_mbf_alloc failed,this can not happen");
                  return -1;
                }

              buf =
                (struct spl_pbuf *) ((char *) mbuf +
                                     sizeof (struct common_mem_mbuf));
              int idx = loop * (int) SBR_TX_POOL_MBUF_NUM + i;
              sys_sem_init (&tx_msg_array[idx].param.op_completed);
              tx_msg_array[idx].param.msg_from = NULL;
              buf->msg = (void *) &tx_msg_array[idx];
              (void) res_free (&buf->res_chk);  //no need to check return value, as it will do free operation depends on alloc_flag

              if (nsfw_mem_mbf_free (mbuf, NSFW_SHMEM) < 0)
                {
                  NSSBR_LOGERR
                    ("nsfw_mem_mbf_free failed,this can not happen");
                  return -1;
                }

              i++;
            }

          pool_array[loop] = mp;
#ifdef SYS_MEM_RES_STAT
          g_app_tx_pool[loop] = mp;
#endif
        }
    }

  spl_reg_tx_pool_mgr (pool_array, array_size);

  NSSBR_LOGINF ("Create all tx_mbuf_pool ok]pool_num=%d, total_mem=%d",
                array_size, nsfw_mem_get_len (pool_array[0],
                                              NSFW_MEM_MBUF) * array_size);
  MEM_STAT (SBR_TX_POOL_NAME, SBR_TX_POOL_NAME, NSFW_SHMEM,
            nsfw_mem_get_len (pool_array[0], NSFW_MEM_MBUF) * array_size);
  return 0;
}

spl_app_res_group *
spl_create_res_group ()
{
  spl_app_res_group *group = spl_create_group ();
  if (NULL == group)
    {
      NSPOL_LOGERR ("spl_create_group failed");
      return NULL;
    }

  NSPOL_LOGDBG (SC_DPDK_INFO, "spl_create_group ok");

  if (spl_create_ring_pool (group) != 0)
    {
      NSPOL_LOGERR ("spl_create_ring_pool failed");
      return NULL;
    }

  NSPOL_LOGDBG (SC_DPDK_INFO, "spl_create_ring_pool ok");

  if (spl_create_msg_pool (group) != 0)
    {
      NSPOL_LOGERR ("spl_create_msg_pool failed");
      return NULL;
    }

  NSPOL_LOGDBG (SC_DPDK_INFO, "spl_create_msg_pool ok");

  if (spl_create_netconn_pool (group) != 0)
    {
      NSPOL_LOGERR ("spl_create_netconn_pool failed");
      return NULL;
    }

  NSPOL_LOGDBG (SC_DPDK_INFO, "spl_create_netconn_pool ok");

  if (spl_regist_recycle (group) != 0)
    {
      NSPOL_LOGERR ("spl_regist_recycle failed");
      return NULL;
    }

  NSPOL_LOGDBG (SC_DPDK_INFO, "spl_regist_recycle ok");

  if (spl_create_tx_pool (group->tx_pool_array, SBR_TX_POOL_NUM) != 0)
    {
      NSPOL_LOGERR ("spl_create_tx_pool failed");
      return NULL;
    }

  NSPOL_LOGDBG (SC_DPDK_INFO, "spl_create_tx_pool ok");

  return group;
}

/*****************************************************************************
*   Prototype    : spl_init_app_res
*   Description  : init app res
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
spl_init_app_res ()
{
  g_spl_app_res_group = spl_create_res_group ();
  if (NULL == g_spl_app_res_group)
    {
      NSPOL_LOGERR ("spl_create_group failed");
      return -1;
    }

  return spl_add_instance_res_group (0, g_spl_app_res_group);
}

/*****************************************************************************
*   Prototype    : spl_get_conn_array
*   Description  : get conn array
*   Input        : u32 pid
*   Output       : None
*   Return Value : spl_netconn_t**
*   Calls        :
*   Called By    :
*
*****************************************************************************/
spl_netconn_t **
spl_get_conn_array (u32 pid)
{
  return (spl_netconn_t **) ADDR_SHTOL (g_spl_app_res_group->conn_array);
}

/*****************************************************************************
*   Prototype    : spl_get_conn_pool
*   Description  : get conn pool
*   Input        : None
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mring_handle
spl_get_conn_pool ()
{
  return (mring_handle) ADDR_SHTOL (g_spl_app_res_group->conn_pool);
}

/*****************************************************************************
*   Prototype    : spl_get_msg_pool
*   Description  : get msg pool
*   Input        : None
*   Output       : None
*   Return Value : mring_handle
*   Calls        :
*   Called By    :
*
*****************************************************************************/
mring_handle
spl_get_msg_pool ()
{
  return (mring_handle) ADDR_SHTOL (g_spl_app_res_group->msg_pool);
}

/*****************************************************************************
*   Prototype    : spl_recycle_msg
*   Description  : recycle msg
*   Input        : void *data
*                  void* argv
*   Output       : None
*   Return Value : static int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static int
spl_recycle_msg (void *data, void *argv)
{
  if (!data)
    {
      NSPOL_LOGERR ("data is NULL]data=%p,pid=%p", data, argv);
      return -1;
    }

  data_com_msg *m = (data_com_msg *) data;
  u64 pid_64 = (u64) argv;
  u32 pid = (u32) pid_64;
  if (pid == m->param.recycle_pid)
    {
      ASYNC_MSG_FREE (m);
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : spl_recycle_msg_pool
*   Description  : recycle msg pool
*   Input        : u32 pid
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
spl_recycle_msg_pool (u32 pid)
{
  mring_handle msg_pool = spl_get_msg_pool ();
  if (!msg_pool)
    {
      NSPOL_LOGERR ("msg_pool is NULL,this can not happen]pid=%u", pid);
      return;
    }

  u64 pid_64 = pid;
  (void) nsfw_mem_sp_iterator (msg_pool, 0, SPL_MAX_MSG_NUM, spl_recycle_msg,
                               (void *) pid_64);
}

/*****************************************************************************
*   Prototype    : spl_get_conn_num
*   Description  : get conn num
*   Input        : None
*   Output       : None
*   Return Value : u32
*   Calls        :
*   Called By    :
*
*****************************************************************************/
u32
spl_get_conn_num ()
{
  return CUR_CFG_SOCKET_NUM;
}

/***************************************************
*   description:
***************************************************/
int
sbr_malloc_tx_pool (u32 pid, mpool_handle pool[], u32 pool_num)
{
  int loop;
  for (loop = 0; loop < SBR_TX_POOL_NUM; loop++)
    {
      if ((0 == g_res_group_array->pid_array[loop])
          &&
          __sync_bool_compare_and_swap (&g_res_group_array->pid_array[loop],
                                        0, pid))
        {
          u32 i;
          for (i = 0; i < g_res_group_array->thread_num && i < pool_num; ++i)
            {
              spl_app_res_group *group =
                (spl_app_res_group *)
                ADDR_SHTOL (g_res_group_array->res_group[i]);
              pool[i] = ADDR_SHTOL (group->tx_pool_array[loop]);
            }

          NSSBR_LOGINF ("get tx pool]pid=%d,loop=%d.", pid, loop);
          return 0;
        }
    }

  for (loop = 0; loop < SBR_TX_POOL_NUM; loop++)
    {
      NSSBR_LOGERR ("no free pool]loop=%d,pid=%d", loop,
                    g_res_group_array->pid_array[loop]);
    }

  return -1;
}

/***************************************************
*   description:
***************************************************/
void
spl_free_tx_pool (u32 pid)
{
  int loop;
  for (loop = 0; loop < SBR_TX_POOL_NUM; loop++)
    {
      if (pid == g_res_group_array->pid_array[loop])
        {
          u32 i;
          for (i = 0; i < g_res_group_array->thread_num; ++i)
            {
              (void)
                nsfw_mem_mbuf_pool_recycle (g_res_group_array->res_group
                                            [i]->tx_pool_array[loop]);
            }

          if (!__sync_bool_compare_and_swap
              (&g_res_group_array->pid_array[loop], pid, 0))
            {
              NSSBR_LOGERR ("free tx_pool failed]loop=%d,pid=%d", loop, pid);
            }
          else
            {
              NSSBR_LOGDBG ("free tx_pool ok]loop=%d,pid=%d", loop, pid);
            }

          break;
        }
    }
}

/***************************************************
*   description:
***************************************************/
mring_handle
sbr_get_instance_conn_pool (u32 thread_index)
{
  if (thread_index >= g_res_group_array->thread_num)
    {
      return NULL;
    }

  spl_app_res_group *group =
    (spl_app_res_group *)
    ADDR_SHTOL (g_res_group_array->res_group[thread_index]);
  return ADDR_SHTOL (group->conn_pool);
}

/***************************************************
*   description:
***************************************************/
mring_handle *
ss_get_instance_msg_box (u16 thread_index, u16 idx)
{
  if (thread_index >= g_res_group_array->thread_num)
    {
      thread_index = 0;
    }

  if (idx >= SPL_MSG_BOX_NUM)
    {
      idx = 0;
    }

  spl_app_res_group *group =
    (spl_app_res_group *)
    ADDR_SHTOL (g_res_group_array->res_group[thread_index]);
  return ADDR_SHTOL (group->mbox_array[idx]);
}
