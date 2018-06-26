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

#include <stdint.h>
#include "nstack_securec.h"
#include "nstack_log.h"
#include "nsfw_ring_fun.h"
#include "nsfw_shmem_ring.h"
#include "nsfw_shmem_mng.h"
#include "common_mem_buf.h"
#include "common_mem_common.h"

#include "common_func.h"

/*get the base address of msg data */
#define NSFW_SHMEM_GET_DATA(pmsg, type) (type *)&((pmsg)->aidata[0])

/*if input point is nun, just return null*/
#define NSFW_POINT_CHK_RET_NULL(p, desc) \
    if (NULL == (p))  \
    {  \
        NSCOMM_LOGERR("point check fail] desc_para=%s", desc); \
        return NULL; \
    }

/*if input point is nun, just return err num*/
#define NSFW_POINT_CHK_RET_ERR(p, desc) \
    if (NULL == (p))  \
    {  \
        NSCOMM_LOGDBG("point check fail] desc_para=%s", desc); \
        return NSFW_MEM_ERR; \
    }

/*if input point is nun, goto flag*/
#define NSFW_POINT_CHK_RET_GOTO(p, gotoflag, desc) \
    if (NULL == (p))  \
    {  \
        NSCOMM_LOGERR("point check fail] desc_para=%s", desc); \
        goto gotoflag; \
    }

/*init the msg head*/
#define NSFW_SHMEM_MSG_HEAD_INIT(pmsg, type, length) { \
        (pmsg)->usmsg_type = (type); \
        (pmsg)->uslength = (length); \
    }

/*rsp msg head check, and if err goto*/
#define NSFW_SHMEM_MSGHEAD_CHK_GOTO(pmsg, type, length, gotoflag) { \
        if (((type) != pmsg->usmsg_type) && ((length) != pmsg->uslength)) \
        {  \
            NSCOMM_LOGERR("check fail] msgtype=%d, type_para=%d, len=%d", (pmsg->usmsg_type), (type), (length)); \
            goto gotoflag;  \
        }  \
    }

/*rsp check the state*/
#define NSFW_SHMEM_ACKSTATE_CHK_GOTO(expret, ret, expseg, seg, gotoflag) { \
        if (((ret) != (expret)) || ((expseg) != (seg))) \
        {  \
            NSCOMM_LOGERR("ackstate check fail]msgack exp=%d, real=%d,eseg=%d, rseg=%d", (expret), (ret), (expseg), (seg)); \
            goto gotoflag; \
        }   \
    }

/*mzone msg init*/
#define NSFW_SHMEM_MZONE_DATA_INIT(pdata, slength, seg, socketid) { \
        (pdata)->isocket_id = (socketid); \
        (pdata)->length = (slength); \
        (pdata)->usseq = (seg); \
        (pdata)->ireserv = 0; \
    }

/*mbuf msg init*/
#define NSFW_SHMEM_MBUF_DATA_INIT(pdata, seg, num, cashsize, priv_size, data_room, flag, socketid) { \
        (pdata)->usseq = (seg); \
        (pdata)->usnum = (num); \
        (pdata)->uscash_size = (cashsize); \
        (pdata)->uspriv_size = (priv_size); \
        (pdata)->usdata_room = (data_room); \
        (pdata)->enmptype = (flag); \
        (pdata)->isocket_id = (socketid); \
        (pdata)->ireserv = 0; \
    }

/*mpool msg init*/
#define NSFW_SHMEM_MPOOL_DATA_INIT(pdata, seg, num, eltsize, flag, socketid) { \
        (pdata)->usseq = (seg); \
        (pdata)->usnum = (num); \
        (pdata)->useltsize = (eltsize);  \
        (pdata)->enmptype = (flag); \
        (pdata)->isocket_id = (socketid); \
        (pdata)->ireserv = 0; \
    }

/*mring msg init*/
#define NSFW_SHMEM_MRING_DATA_INIT(pdata, seg, num, flag, socketid) { \
        (pdata)->usseq = (seg);     \
        (pdata)->usnum = (num);  \
        (pdata)->enmptype = (flag);  \
        (pdata)->isocket_id = (socketid);  \
        (pdata)->ireserv = 0; \
    }

#define NSFW_SHMEM_MSG_FREE(pmsg, prsp_msg) {\
    if (pmsg) \
    { \
        nsfw_mgr_msg_free(pmsg); \
    } \
    if (prsp_msg) \
    { \
        nsfw_mgr_msg_free(prsp_msg); \
    }  \
}

/*
 * create a block memory by send a msg
 *
 */
mzone_handle
nsfw_memzone_remote_reserv (const i8 * name, size_t mlen, i32 socket_id)
{
  /*msg point define */
  nsfw_mgr_msg *pmsg = NULL;
  nsfw_mgr_msg *prsp_msg = NULL;

  /*msg head point define */
  nsfw_shmem_msg_head *pdata_head = NULL;

  /*msg data point define */
  nsfw_shmem_reserv_req *pdata = NULL;
  nsfw_shmem_msg_head *pack_head = NULL;

  /*ack msg define */
  nsfw_shmem_ack *pack_data = NULL;

  mzone_handle hhandle = NULL;
  u8 ucret = TRUE;
  i32 iretval = NSFW_MEM_OK;

  //pmsg = nsfw_mgr_msg_alloc(MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MASTER);
  pmsg = nsfw_mgr_msg_alloc (MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MAIN);
  NSFW_POINT_CHK_RET_NULL (pmsg, "remote reserv pmsg alloc");

  prsp_msg = nsfw_mgr_null_rspmsg_alloc ();
  NSFW_POINT_CHK_RET_GOTO (prsp_msg, release, "remote reserv rspmsg alloc");

  /*msg head init */
  pdata_head = GET_USER_MSG (nsfw_shmem_msg_head, pmsg);
  NSFW_SHMEM_MSG_HEAD_INIT (pdata_head, NSFW_RESERV_REQ_MSG,
                            sizeof (nsfw_shmem_reserv_req));

  /*msg data init */
  pdata = NSFW_SHMEM_GET_DATA (pdata_head, nsfw_shmem_reserv_req);
  iretval = STRCPY_S (pdata->aname, sizeof (pdata->aname), name);
  if (EOK != iretval)
    {
      NSCOMM_LOGERR ("reserv mem copy name fail] ret=%d", iretval);
      goto release;
    }

  /*fill msg data */
  NSFW_SHMEM_MZONE_DATA_INIT (pdata, mlen, (u16) 0, socket_id);

  ucret = nsfw_mgr_send_req_wait_rsp (pmsg, prsp_msg);

  if (FALSE == ucret)
    {
      NSCOMM_LOGERR ("reserv mem req rsp fail] ret=%u", ucret);
      goto release;
    }

  /*interrupt msg head */
  pack_head = GET_USER_MSG (nsfw_shmem_msg_head, prsp_msg);
  NSFW_SHMEM_MSGHEAD_CHK_GOTO (pack_head, NSFW_RESERV_ACK_MSG,
                               sizeof (nsfw_shmem_ack), release);

  pack_data = NSFW_SHMEM_GET_DATA (pack_head, nsfw_shmem_ack);
  NSFW_SHMEM_ACKSTATE_CHK_GOTO (NSFW_MEM_ALLOC_SUCC, pack_data->cstate, 0,
                                pack_data->usseq, release);

  hhandle = (mzone_handle) ADDR_SHTOL (pack_data->pbase_addr);
  NSCOMM_LOGDBG ("mem reserve] name=%s, handle=%p, seg=%u", name, hhandle,
                 pack_data->usseq);
release:
  NSFW_SHMEM_MSG_FREE (pmsg, prsp_msg);
  return hhandle;
}

/*
 *create some memories by send a msg
 */
i32
nsfw_memzone_remote_reserv_v (nsfw_mem_zone * pmeminfo,
                              mzone_handle * paddr_array, i32 inum, pid_t pid)
{
  /*msg point define */
  nsfw_mgr_msg *pmsg = NULL;
  nsfw_mgr_msg *prsp_msg = NULL;
  nsfw_shmem_msg_head *pdata_head = NULL;
  nsfw_shmem_reserv_req *pdata = NULL;
  nsfw_shmem_reserv_req *ptempdata = NULL;
  nsfw_shmem_msg_head *pack_head = NULL;

  nsfw_shmem_ack *pack_data = NULL;
  u8 ucret = TRUE;
  i32 iretval = NSFW_MEM_OK;
  i32 icount = 0;
  i32 itindex = 0;
  i32 iindex = 0;
  u16 ussegbase = 0;
  u16 ustempv = 0;
  i32 ieltnum = 0;
  i32 ieltnum_max =
    (NSFW_MGR_MSG_BODY_LEN -
     sizeof (nsfw_shmem_msg_head)) / sizeof (nsfw_shmem_reserv_req);

  //pmsg = nsfw_mgr_msg_alloc(MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MASTER);
  pmsg = nsfw_mgr_msg_alloc (MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MAIN);
  NSFW_POINT_CHK_RET_GOTO (pmsg, err, "remote reserv_v msg alloc");

  prsp_msg = nsfw_mgr_null_rspmsg_alloc ();
  NSFW_POINT_CHK_RET_GOTO (prsp_msg, err, "remote reserv_v rspmsg alloc");

  pdata_head = GET_USER_MSG (nsfw_shmem_msg_head, pmsg);

  ptempdata = pdata = NSFW_SHMEM_GET_DATA (pdata_head, nsfw_shmem_reserv_req);

  do
    {
      icount++;
      ieltnum++;

      if (((icount % ieltnum_max) == 0) || (icount >= inum))
        {
          NSFW_SHMEM_MSG_HEAD_INIT (pdata_head, NSFW_RESERV_REQ_MSG,
                                    ieltnum * sizeof (nsfw_shmem_reserv_req));

          itindex = icount - 1;
          int retVal =
            SPRINTF_S (ptempdata->aname, sizeof (ptempdata->aname), "%s_%x",
                       pmeminfo[itindex].stname.aname, pid);
          if (-1 == retVal)
            {
              NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
            }
          NSFW_SHMEM_MZONE_DATA_INIT (ptempdata, pmeminfo[itindex].length,
                                      (u16) itindex,
                                      pmeminfo[itindex].isocket_id);

          ucret = nsfw_mgr_send_req_wait_rsp (pmsg, prsp_msg);

          if (FALSE == ucret)
            {
              NSCOMM_LOGERR ("reserv v mem req rsp fail] ret=%u", ucret);
              goto err;
            }

          pack_head = GET_USER_MSG (nsfw_shmem_msg_head, prsp_msg);
          NSFW_SHMEM_MSGHEAD_CHK_GOTO (pack_head, NSFW_RESERV_ACK_MSG,
                                       ieltnum * sizeof (nsfw_shmem_ack),
                                       err);

          pack_data = NSFW_SHMEM_GET_DATA (pack_head, nsfw_shmem_ack);

          for (iindex = 0; iindex < ieltnum; iindex++)
            {
              ustempv = ussegbase + iindex;

              NSFW_SHMEM_ACKSTATE_CHK_GOTO (NSFW_MEM_ALLOC_SUCC,
                                            pack_data->cstate, ustempv,
                                            (u16) pack_data->usseq, err);

              paddr_array[ustempv] = ADDR_SHTOL (pack_data->pbase_addr);
              NSCOMM_LOGDBG ("remote reserve]index=%u, seg=%u, handle=%p",
                             ustempv, pack_data->usseq, paddr_array[ustempv]);
              pack_data++;
            }

          ussegbase = icount;
          ieltnum = 0;
          ptempdata = pdata;
        }
      else
        {
          itindex = icount - 1;
          int retVal =
            SPRINTF_S (ptempdata->aname, sizeof (ptempdata->aname), "%s_%x",
                       pmeminfo[itindex].stname.aname, pid);
          if (-1 == retVal)
            {
              NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
            }
          NSFW_SHMEM_MZONE_DATA_INIT (ptempdata, pmeminfo[itindex].length,
                                      (u16) itindex,
                                      pmeminfo[itindex].isocket_id);
          ptempdata++;
        }
    }
  while (icount < inum);

  iretval = NSFW_MEM_OK;
  goto free;

err:
  iretval = NSFW_MEM_ERR;
free:
  NSFW_SHMEM_MSG_FREE (pmsg, prsp_msg);
  return iretval;
}

/*
 *release a block memory with name by send msg
 */
i32
nsfw_remote_free (const i8 * name, nsfw_mem_struct_type entype)
{
  /*msg point define */
  nsfw_mgr_msg *pmsg = NULL;
  nsfw_mgr_msg *prsp_msg = NULL;

  nsfw_shmem_msg_head *pdata_head = NULL;

  nsfw_shmem_free_req *pdata = NULL;

  nsfw_shmem_msg_head *pack_head = NULL;
  nsfw_shmem_ack *pack_data = NULL;
  u8 ucret = TRUE;
  i32 iretval = NSFW_MEM_OK;

  pmsg = nsfw_mgr_msg_alloc (MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MAIN);
  NSFW_POINT_CHK_RET_ERR (pmsg, "remote free msg alloc");

  prsp_msg = nsfw_mgr_null_rspmsg_alloc ();
  NSFW_POINT_CHK_RET_GOTO (prsp_msg, terr, "remote free rspmsg alloc");

  pdata_head = GET_USER_MSG (nsfw_shmem_msg_head, pmsg);
  NSFW_SHMEM_MSG_HEAD_INIT (pdata_head, NSFW_RELEASE_REQ_MSG,
                            sizeof (nsfw_shmem_free_req));

  pdata = NSFW_SHMEM_GET_DATA (pdata_head, nsfw_shmem_free_req);
  if (EOK != STRCPY_S (pdata->aname, sizeof (pdata->aname), name))
    {
      NSCOMM_LOGERR ("STRCPY_S failed]name=%s", name);
    }
  pdata->usseq = 0;
  pdata->ustype = entype;
  pdata->ireserv = 0;

  ucret = nsfw_mgr_send_req_wait_rsp (pmsg, prsp_msg);

  if (FALSE == ucret)
    {
      NSCOMM_LOGERR ("free mem req rsp fail] ret=%u", ucret);
      goto release;
    }

  pack_head = GET_USER_MSG (nsfw_shmem_msg_head, prsp_msg);
  NSFW_SHMEM_MSGHEAD_CHK_GOTO (pack_head, NSFW_RELEASE_ACK_MSG,
                               sizeof (nsfw_shmem_ack), terr);

  pack_data = NSFW_SHMEM_GET_DATA (pack_head, nsfw_shmem_ack);
  NSFW_SHMEM_ACKSTATE_CHK_GOTO (NSFW_MEM_ALLOC_SUCC, pack_data->cstate, 0,
                                pack_data->usseq, terr);

  iretval = NSFW_MEM_OK;
  goto release;
terr:
  iretval = NSFW_MEM_ERR;
release:
  NSFW_SHMEM_MSG_FREE (pmsg, prsp_msg);
  return iretval;
}

/*
 *create a mbuf pool by send a msg
 */
mpool_handle
nsfw_remote_shmem_mbf_create (const i8 * name, unsigned n,
                              unsigned cache_size, unsigned priv_size,
                              unsigned data_room_size, i32 socket_id,
                              nsfw_mpool_type entype)
{
  /*msg point define */
  nsfw_mgr_msg *pmsg = NULL;
  nsfw_mgr_msg *prsp_msg = NULL;
  nsfw_shmem_msg_head *pdata_head = NULL;
  nsfw_shmem_mbuf_req *pdata = NULL;
  nsfw_shmem_msg_head *tpack_head = NULL;
  nsfw_shmem_ack *tpack_data = NULL;
  mpool_handle hhandle = NULL;
  u8 ucret = TRUE;
  i32 iretval = NSFW_MEM_OK;

  pmsg = nsfw_mgr_msg_alloc (MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MAIN);
  NSFW_POINT_CHK_RET_NULL (pmsg, "remote mbf create pmsg alloc");

  prsp_msg = nsfw_mgr_null_rspmsg_alloc ();
  NSFW_POINT_CHK_RET_GOTO (prsp_msg, release, "remote mbf create msg alloc");

  pdata_head = GET_USER_MSG (nsfw_shmem_msg_head, pmsg);
  NSFW_SHMEM_MSG_HEAD_INIT (pdata_head, NSFW_MBUF_REQ_MSG,
                            sizeof (nsfw_shmem_mbuf_req));

  pdata = NSFW_SHMEM_GET_DATA (pdata_head, nsfw_shmem_mbuf_req);
  iretval = STRCPY_S (pdata->aname, sizeof (pdata->aname), name);
  if (EOK != iretval)
    {
      NSCOMM_LOGERR ("mbf create name cpy fail] ret=%d", iretval);
      goto release;
    }

  NSFW_SHMEM_MBUF_DATA_INIT (pdata, 0, n, cache_size, priv_size,
                             data_room_size, (u16) entype, socket_id);

  ucret = nsfw_mgr_send_req_wait_rsp (pmsg, prsp_msg);

  if (FALSE == ucret)
    {
      NSCOMM_LOGERR ("mbf create mem req rsp fail] ret=%u", ucret);
      goto release;
    }

  tpack_head = GET_USER_MSG (nsfw_shmem_msg_head, prsp_msg);
  NSFW_SHMEM_MSGHEAD_CHK_GOTO (tpack_head, NSFW_MBUF_ACK_MSG,
                               sizeof (nsfw_shmem_ack), release);

  tpack_data = NSFW_SHMEM_GET_DATA (tpack_head, nsfw_shmem_ack);
  NSFW_SHMEM_ACKSTATE_CHK_GOTO (NSFW_MEM_ALLOC_SUCC, tpack_data->cstate, 0,
                                tpack_data->usseq, release);

  hhandle = ADDR_SHTOL (tpack_data->pbase_addr);
  NSCOMM_LOGDBG ("mbf create] name=%s, handle=%p, seg=%u", name, hhandle,
                 tpack_data->usseq);
release:
  NSFW_SHMEM_MSG_FREE (pmsg, prsp_msg);
  return hhandle;
}

/*
 *create some mbuf pools
 */
i32
nsfw_remote_shmem_mbf_createv (nsfw_mem_mbfpool * pmbfname,
                               mpool_handle * phandle_array, i32 inum,
                               pid_t pid)
{
  /*msg point define */
  nsfw_mgr_msg *mbpmsg = NULL;
  nsfw_mgr_msg *prsp_msg = NULL;
  nsfw_shmem_msg_head *pdata_head = NULL;

  nsfw_shmem_mbuf_req *pdata = NULL;
  nsfw_shmem_mbuf_req *ptempdata = NULL;

  nsfw_shmem_msg_head *pack_head = NULL;

  nsfw_shmem_ack *pack_data = NULL;
  mpool_handle hhandle = NULL;
  u8 ucret = TRUE;
  i32 iretval = NSFW_MEM_OK;
  i32 icount = 0;
  i32 itindex = 0;
  i32 iindex = 0;
  i32 isegbase = 0;
  i32 ieltnum = 0;
  i32 ieltnum_max =
    (NSFW_MGR_MSG_BODY_LEN -
     sizeof (nsfw_shmem_msg_head)) / sizeof (nsfw_shmem_mbuf_req);

  mbpmsg = nsfw_mgr_msg_alloc (MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MAIN);
  NSFW_POINT_CHK_RET_GOTO (mbpmsg, lerr, "remote mbf createv msg alloc");

  prsp_msg = nsfw_mgr_null_rspmsg_alloc ();
  NSFW_POINT_CHK_RET_GOTO (prsp_msg, lerr, "remote mbf createv rspmsg alloc");

  pdata_head = GET_USER_MSG (nsfw_shmem_msg_head, mbpmsg);

  ptempdata = pdata = NSFW_SHMEM_GET_DATA (pdata_head, nsfw_shmem_mbuf_req);

  do
    {
      icount++;
      ieltnum++;

      if (((icount % ieltnum_max) == 0) || (icount >= inum))
        {
          NSFW_SHMEM_MSG_HEAD_INIT (pdata_head, NSFW_MBUF_REQ_MSG,
                                    ieltnum * sizeof (nsfw_shmem_mbuf_req));

          /*fill msg data */
          itindex = icount - 1;
          if (-1 ==
              SPRINTF_S (ptempdata->aname, sizeof (ptempdata->aname), "%s_%x",
                         pmbfname[itindex].stname.aname, pid))
            {
              NSCOMM_LOGERR ("SPRINTF_S failed]");
              goto lerr;
            }
          NSFW_SHMEM_MBUF_DATA_INIT (ptempdata, (u16) itindex,
                                     pmbfname[itindex].usnum,
                                     pmbfname[itindex].uscash_size,
                                     pmbfname[itindex].uspriv_size,
                                     pmbfname[itindex].usdata_room,
                                     pmbfname[itindex].enmptype,
                                     pmbfname[itindex].isocket_id);

          ucret = nsfw_mgr_send_req_wait_rsp (mbpmsg, prsp_msg);

          if (FALSE == ucret)
            {
              NSCOMM_LOGERR ("mbf createv mem req rsp fail] ret=%d", ucret);
              goto lerr;
            }

          /*interrupt msg head */
          pack_head = GET_USER_MSG (nsfw_shmem_msg_head, prsp_msg);
          NSFW_SHMEM_MSGHEAD_CHK_GOTO (pack_head, NSFW_MBUF_ACK_MSG,
                                       ieltnum * sizeof (nsfw_shmem_ack),
                                       lerr);

          pack_data = NSFW_SHMEM_GET_DATA (pack_head, nsfw_shmem_ack);

          for (iindex = 0; iindex < ieltnum; iindex++)
            {
              NSFW_SHMEM_ACKSTATE_CHK_GOTO (NSFW_MEM_ALLOC_SUCC,
                                            pack_data->cstate,
                                            (isegbase + iindex),
                                            (u16) pack_data->usseq, lerr);
              phandle_array[isegbase + iindex] =
                ADDR_SHTOL (pack_data->pbase_addr);
              NSCOMM_LOGDBG ("mbf createv] seg=%d, handle=%p",
                             pack_data->usseq, hhandle);
              pack_data++;
            }

          isegbase = icount;
          ieltnum = 0;
          ptempdata = pdata;
        }
      else
        {
          itindex = icount - 1;
          if (-1 ==
              SPRINTF_S (ptempdata->aname, sizeof (ptempdata->aname), "%s_%x",
                         pmbfname[itindex].stname.aname, pid))
            {
              NSCOMM_LOGERR ("SPRINTF_S failed]");
              goto lerr;
            }
          NSFW_SHMEM_MBUF_DATA_INIT (ptempdata, (u16) itindex,
                                     pmbfname[itindex].usnum,
                                     pmbfname[itindex].uscash_size,
                                     pmbfname[itindex].uspriv_size,
                                     pmbfname[itindex].usdata_room,
                                     pmbfname[itindex].enmptype,
                                     pmbfname[itindex].isocket_id);
          ptempdata++;
        }
    }
  while (icount < inum);

  /*release memory */
  iretval = NSFW_MEM_OK;
  goto release;

lerr:
  iretval = NSFW_MEM_ERR;
release:
  NSFW_SHMEM_MSG_FREE (mbpmsg, prsp_msg);
  return iretval;
}

/*
 *create a simpile pool
 */
mring_handle
nsfw_remote_shmem_mpcreate (const char *name, unsigned int n,
                            unsigned int elt_size, i32 socket_id,
                            nsfw_mpool_type entype)
{
  /*msg point define */
  nsfw_mgr_msg *pmsg = NULL;
  nsfw_mgr_msg *prsp_msg = NULL;
  nsfw_shmem_msg_head *pdata_head = NULL;
  nsfw_shmem_sppool_req *pdata = NULL;
  nsfw_shmem_msg_head *mppack_head = NULL;
  nsfw_shmem_ack *mppack_data = NULL;
  mring_handle hhandle = NULL;
  u8 ucret = TRUE;
  i32 iretval = NSFW_MEM_OK;

  pmsg = nsfw_mgr_msg_alloc (MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MAIN);
  NSFW_POINT_CHK_RET_NULL (pmsg, "remote mbf mpcreate pmsg alloc");

  prsp_msg = nsfw_mgr_null_rspmsg_alloc ();
  NSFW_POINT_CHK_RET_GOTO (prsp_msg, release, "remote mpcreate rspmsg alloc");

  /*init msg head */
  pdata_head = GET_USER_MSG (nsfw_shmem_msg_head, pmsg);
  NSFW_SHMEM_MSG_HEAD_INIT (pdata_head, NSFW_SPPOOL_REQ_MSG,
                            sizeof (nsfw_shmem_sppool_req));

  /*fill msg data */
  pdata = NSFW_SHMEM_GET_DATA (pdata_head, nsfw_shmem_sppool_req);
  iretval = STRCPY_S (pdata->aname, sizeof (pdata->aname), name);
  if (EOK != iretval)
    {
      NSCOMM_LOGERR ("mp create copy name fail] ret=%d", iretval);
      goto release;
    }

  /*fill msg data */
  NSFW_SHMEM_MPOOL_DATA_INIT (pdata, 0, n, elt_size, entype, socket_id);

  ucret = nsfw_mgr_send_req_wait_rsp (pmsg, prsp_msg);

  if (FALSE == ucret)
    {
      NSCOMM_LOGERR ("mp create rsp fail] ret=%d", ucret);
      goto release;
    }

  /*get msg head */
  mppack_head = GET_USER_MSG (nsfw_shmem_msg_head, prsp_msg);
  NSFW_SHMEM_MSGHEAD_CHK_GOTO (mppack_head, NSFW_SPPOOL_ACK_MSG,
                               sizeof (nsfw_shmem_ack), release);

  mppack_data = NSFW_SHMEM_GET_DATA (mppack_head, nsfw_shmem_ack);
  NSFW_SHMEM_ACKSTATE_CHK_GOTO (NSFW_MEM_ALLOC_SUCC, mppack_data->cstate, 0,
                                mppack_data->usseq, release);

  hhandle = ADDR_SHTOL (mppack_data->pbase_addr);
  NSCOMM_LOGDBG ("mpcreate] name=%s, handle=%p, seg=%d", name, hhandle,
                 mppack_data->usseq);
release:
  NSFW_SHMEM_MSG_FREE (pmsg, prsp_msg);
  return hhandle;
}

/*
 *create some simpile pools by send a msg
 */
i32
nsfw_remote_shmem_mpcreatev (nsfw_mem_sppool * pmpinfo,
                             mring_handle * pringhandle_array, i32 inum,
                             pid_t pid)
{
  /*msg point define */
  nsfw_mgr_msg *pmsg = NULL;
  nsfw_mgr_msg *prsp_msg = NULL;

  /*msg head define */
  nsfw_shmem_msg_head *pdata_head = NULL;

  /*msg data define */
  nsfw_shmem_sppool_req *pdata = NULL;
  nsfw_shmem_sppool_req *ptempdata = NULL;

  /*ack msg define */
  nsfw_shmem_msg_head *pack_head = NULL;

  nsfw_shmem_ack *pack_data = NULL;
  mring_handle hhandle = NULL;
  u8 ucret = TRUE;
  i32 iretval = NSFW_MEM_OK;
  i32 icount = 0;
  i32 itindex = 0;
  i32 iindex = 0;
  i32 isegbase = 0;
  i32 ieltnum = 0;
  /*the max members that a msg can take */
  i32 ieltnum_max =
    (NSFW_MGR_MSG_BODY_LEN -
     sizeof (nsfw_shmem_msg_head)) / sizeof (nsfw_shmem_sppool_req);

  /*alloc a msg */
  //pmsg = nsfw_mgr_msg_alloc(MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MASTER);
  pmsg = nsfw_mgr_msg_alloc (MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MAIN);
  NSFW_POINT_CHK_RET_GOTO (pmsg, mperr, "remote mpcreatev pmsg alloc");

  /*alloc rsp msg */
  prsp_msg = nsfw_mgr_null_rspmsg_alloc ();
  NSFW_POINT_CHK_RET_GOTO (prsp_msg, mperr, "remote mpcreatev rspmsg alloc");

  pdata_head = GET_USER_MSG (nsfw_shmem_msg_head, pmsg);

  ptempdata = pdata = NSFW_SHMEM_GET_DATA (pdata_head, nsfw_shmem_sppool_req);

  do
    {
      icount++;
      ieltnum++;

      /*if the element num reach the bigest, or already send all, just deal */
      if (((icount % ieltnum_max) == 0) || (icount >= inum))
        {
          /*init msg header */
          NSFW_SHMEM_MSG_HEAD_INIT (pdata_head, NSFW_SPPOOL_REQ_MSG,
                                    ieltnum * sizeof (nsfw_shmem_sppool_req));

          /*fill the msg data */
          itindex = icount - 1;

          int retVal =
            SPRINTF_S (ptempdata->aname, sizeof (ptempdata->aname), "%s_%x",
                       pmpinfo[itindex].stname.aname, pid);
          if (-1 == retVal)
            {
              NSCOMM_LOGERR ("SPRINTF_S fail]ret=%d", retVal);
              goto mperr;
            }
          NSFW_SHMEM_MPOOL_DATA_INIT (ptempdata, itindex,
                                      pmpinfo[itindex].usnum,
                                      pmpinfo[itindex].useltsize,
                                      pmpinfo[itindex].enmptype,
                                      pmpinfo[itindex].isocket_id);

          ucret = nsfw_mgr_send_req_wait_rsp (pmsg, prsp_msg);

          if (FALSE == ucret)
            {
              NSCOMM_LOGERR ("mpcreatev create fail] ret=%u", ucret);
              goto mperr;
            }

          /*interrupt mgs head */
          pack_head = GET_USER_MSG (nsfw_shmem_msg_head, prsp_msg);
          NSFW_SHMEM_MSGHEAD_CHK_GOTO (pack_head, NSFW_SPPOOL_ACK_MSG,
                                       ieltnum * sizeof (nsfw_shmem_ack),
                                       mperr);

          pack_data = NSFW_SHMEM_GET_DATA (pack_head, nsfw_shmem_ack);

          for (iindex = 0; iindex < ieltnum; iindex++)
            {
              NSFW_SHMEM_ACKSTATE_CHK_GOTO (NSFW_MEM_ALLOC_SUCC,
                                            pack_data->cstate,
                                            (isegbase + iindex),
                                            (u16) pack_data->usseq, mperr);
              pringhandle_array[isegbase + iindex] =
                ADDR_SHTOL (pack_data->pbase_addr);
              NSCOMM_LOGDBG ("mpcreatev] seg=%u, handle=%p", pack_data->usseq,
                             hhandle);
              pack_data++;
            }

          isegbase = icount;
          ieltnum = 0;
          ptempdata = pdata;
        }
      else
        {
          itindex = icount - 1;
          int retVal =
            SPRINTF_S (ptempdata->aname, sizeof (ptempdata->aname), "%s_%x",
                       pmpinfo[itindex].stname.aname, pid);
          if (-1 == retVal)
            {
              NSCOMM_LOGERR ("SPRINTF_S fail]ret=%d", retVal);
              goto mperr;
            }
          NSFW_SHMEM_MPOOL_DATA_INIT (ptempdata, itindex,
                                      pmpinfo[itindex].usnum,
                                      pmpinfo[itindex].useltsize,
                                      pmpinfo[itindex].enmptype,
                                      pmpinfo[itindex].isocket_id);

          ptempdata++;
        }
    }
  while (icount < inum);

  /*release the memory */
  iretval = NSFW_MEM_OK;
  goto release;

mperr:
  iretval = NSFW_MEM_ERR;
release:
  NSFW_SHMEM_MSG_FREE (pmsg, prsp_msg);
  return iretval;
}

/*
 *create a ring
 */
mring_handle
nsfw_remote_shmem_ringcreate (const char *name, unsigned int n, i32 socket_id,
                              nsfw_mpool_type entype)
{
  /*msg point define */
  nsfw_mgr_msg *pmsg = NULL;
  nsfw_mgr_msg *prsp_msg = NULL;

  /*msg head define */
  nsfw_shmem_msg_head *pdata_head = NULL;

  /*msg data define */
  nsfw_shmem_ring_req *pdata = NULL;
  /*ack msg define */
  nsfw_shmem_msg_head *pack_head = NULL;
  nsfw_shmem_ack *ppack_data = NULL;
  mring_handle hhandle = NULL;
  u8 ucret = TRUE;
  i32 iretval = NSFW_MEM_OK;

  //pmsg = nsfw_mgr_msg_alloc(MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MASTER);
  pmsg = nsfw_mgr_msg_alloc (MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MAIN);
  NSFW_POINT_CHK_RET_NULL (pmsg, "remote ringcreate pmsg alloc");

  prsp_msg = nsfw_mgr_null_rspmsg_alloc ();
  NSFW_POINT_CHK_RET_GOTO (prsp_msg, release,
                           "remote ringcreate rspmsg alloc");

  /*fill msg head */
  pdata_head = GET_USER_MSG (nsfw_shmem_msg_head, pmsg);
  NSFW_SHMEM_MSG_HEAD_INIT (pdata_head, NSFW_RING_REQ_MSG,
                            sizeof (nsfw_shmem_ring_req));

  /*fill msg data */
  pdata = NSFW_SHMEM_GET_DATA (pdata_head, nsfw_shmem_ring_req);
  iretval = STRCPY_S (pdata->aname, sizeof (pdata->aname), name);
  if (EOK != iretval)
    {
      NSCOMM_LOGERR ("ring create cpy name fail] ret=%d", iretval);
      goto release;
    }

  /*fill msg data */
  NSFW_SHMEM_MRING_DATA_INIT (pdata, 0, n, entype, socket_id);

  ucret = nsfw_mgr_send_req_wait_rsp (pmsg, prsp_msg);

  if (FALSE == ucret)
    {
      NSCOMM_LOGERR ("ring create rsp fail] ret=%d", ucret);
      goto release;
    }

  /*interrupt mgs head */
  pack_head = GET_USER_MSG (nsfw_shmem_msg_head, prsp_msg);
  NSFW_SHMEM_MSGHEAD_CHK_GOTO (pack_head, NSFW_RING_ACK_MSG,
                               sizeof (nsfw_shmem_ack), release);

  ppack_data = NSFW_SHMEM_GET_DATA (pack_head, nsfw_shmem_ack);
  NSFW_SHMEM_ACKSTATE_CHK_GOTO (NSFW_MEM_ALLOC_SUCC, ppack_data->cstate, 0,
                                ppack_data->usseq, release);

  hhandle = ADDR_SHTOL (ppack_data->pbase_addr);
  NSCOMM_LOGDBG ("ring create] name=%s, handle=%p, seg=%u", name, hhandle,
                 ppack_data->usseq);
release:
  NSFW_SHMEM_MSG_FREE (pmsg, prsp_msg);
  return hhandle;
}

/*
 *create a mem pool that the members are rings by send a msg
 *ieltnum:the num of ring member
 *iringnum:the num of ring in simple mem pool
 *entype:the default the of ring
 */
i32
nsfw_remote_shmem_ringcreatev (const char *name, i32 ieltnum,
                               mring_handle * pringhandle_array, i32 iringnum,
                               i32 socket_id, nsfw_mpool_type entype)
{
  unsigned int useltsize = 0;
  mring_handle nhandle = NULL;
  i32 icount = 0;
  i32 n = 0;
  uint64_t baseaddr = 0;
  uint64_t endaddr = 0;
  /*the num of ring member must be power of 2 */
  unsigned int usnum = common_mem_align32pow2 (ieltnum + 1);

  useltsize =
    sizeof (struct nsfw_mem_ring) + usnum * sizeof (union RingData_U);
  nhandle =
    nsfw_remote_shmem_mpcreate (name, iringnum, useltsize, socket_id,
                                NSFW_MRING_SPSC);
  NSFW_POINT_CHK_RET_ERR (nhandle, "remote ringcreatev msg alloc");

  n =
    nsfw_shmem_ring_sc_dequeuev (nhandle, (void **) pringhandle_array,
                                 iringnum);

  if (n != iringnum)
    {
      NSCOMM_LOGERR ("ring dequeue fail] ringnum=%d, retnum=%d", iringnum, n);
      return NSFW_MEM_ERR;
    }

  nsfw_shmem_ring_baseaddr_query (&baseaddr, &endaddr);

  for (icount = 0; icount < iringnum; icount++)
    {
      nsfw_mem_ring_init (pringhandle_array[icount], usnum, (void *) baseaddr,
                          NSFW_SHMEM, entype);
    }

  return NSFW_MEM_OK;
}

/*
 *look up a msg by send a msg
 */
void *
nsfw_remote_shmem_lookup (const i8 * name, nsfw_mem_struct_type entype)
{
  /*msg point define */
  nsfw_mgr_msg *pmsg = NULL;
  nsfw_mgr_msg *prsp_msg = NULL;
  void *addr = NULL;
  /*msg head data define */
  nsfw_shmem_msg_head *pdata_head = NULL;

  /*msg data define */
  nsfw_shmem_lookup_req *pdata = NULL;

  /*ack msg define */
  nsfw_shmem_msg_head *pack_head = NULL;
  nsfw_shmem_ack *lpack_data = NULL;
  u8 ucret = TRUE;

  pmsg = nsfw_mgr_msg_alloc (MGR_MSG_MEM_ALLOC_REQ, NSFW_PROC_MAIN);
  NSFW_POINT_CHK_RET_NULL (pmsg, "remote lookup pmsg alloc");

  prsp_msg = nsfw_mgr_null_rspmsg_alloc ();
  NSFW_POINT_CHK_RET_GOTO (prsp_msg, perr, "remote lookup rspmsg alloc");

  /*msg head init */
  pdata_head = GET_USER_MSG (nsfw_shmem_msg_head, pmsg);
  NSFW_SHMEM_MSG_HEAD_INIT (pdata_head, NSFW_MEM_LOOKUP_REQ_MSG,
                            sizeof (nsfw_shmem_lookup_req));

  pdata = NSFW_SHMEM_GET_DATA (pdata_head, nsfw_shmem_lookup_req);
  if (EOK != STRCPY_S (pdata->aname, sizeof (pdata->aname), name))
    {
      NSCOMM_LOGERR ("STRCPY_S failed]name=%s", name);
    }
  pdata->usseq = 0;
  pdata->ustype = entype;
  pdata->ireserv = 0;

  ucret = nsfw_mgr_send_req_wait_rsp (pmsg, prsp_msg);

  if (FALSE == ucret)
    {
      NSCOMM_LOGERR ("mem lookup fail] ret=%u", ucret);
      goto release;
    }

  /*interrupt mgs head */
  pack_head = GET_USER_MSG (nsfw_shmem_msg_head, prsp_msg);
  NSFW_SHMEM_MSGHEAD_CHK_GOTO (pack_head, NSFW_MEM_LOOKUP_ACK_MSG,
                               sizeof (nsfw_shmem_ack), perr);

  lpack_data = NSFW_SHMEM_GET_DATA (pack_head, nsfw_shmem_ack);
  NSFW_SHMEM_ACKSTATE_CHK_GOTO (NSFW_MEM_ALLOC_SUCC, lpack_data->cstate, 0,
                                lpack_data->usseq, perr);

  addr = ADDR_SHTOL (lpack_data->pbase_addr);
  NSCOMM_LOGDBG ("shmem lookup] name=%s, handle=%p, seg=%u", name, addr,
                 lpack_data->usseq);
  goto release;
perr:
  addr = NULL;

release:
  NSFW_SHMEM_MSG_FREE (pmsg, prsp_msg);
  return addr;
}
