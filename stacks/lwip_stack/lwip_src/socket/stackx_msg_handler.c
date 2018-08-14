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

#include "stackx_msg_handler.h"
#include "stackx_spl_share.h"
#include "stackx_spl_msg.h"
#include "nsfw_msg_api.h"
#include "common_pal_bitwide_adjust.h"
#include "stackx_res_mgr.h"
#include "stackx_prot_com.h"
#include "nstack_securec.h"
//#include "stackx_dfx_api.h"

#define SBR_MSG_MALLOC(sk) msg_malloc(ss_get_msg_pool(sbr_get_conn(sk)))
#define SBR_MSG_FREE(msg) msg_free(msg)

#define SBR_MSG_POST(msg, ring) \
    do { \
        if (MSG_ASYN_POST == msg->param.op_type)\
        {\
            if (msg_post(msg, ring) < 0)\
            {\
                SBR_MSG_FREE(msg); \
                NSSBR_LOGERR("msg_post failed]major=%u,minor=%u,type=%u", \
                     msg->param.major_type, msg->param.minor_type, msg->param.op_type); \
            }\
        } \
        else \
        {\
            if (msg_post(msg, ring) < 0)\
            {\
                 msg->param.err = ECONNABORTED; \
                 NSSBR_LOGERR("msg_post_with_ref failed]major=%u,minor=%u,type=%u", \
                     msg->param.major_type, msg->param.minor_type, msg->param.op_type); \
            }\
        } \
    } while (0)

#define SBR_MSG_POST_RET(msg, ring, ret) \
    do { \
        if (MSG_ASYN_POST == msg->param.op_type)\
        {\
            if ((ret = msg_post(msg, ring)) < 0)\
            {\
                SBR_MSG_FREE(msg); \
                NSSBR_LOGERR("msg_post failed]major=%u,minor=%u,type=%u", \
                     msg->param.major_type, msg->param.minor_type, msg->param.op_type); \
            }\
        } \
        else \
        {\
            if ((ret = msg_post(msg, ring)) < 0)\
            {\
                 msg->param.err = ECONNABORTED; \
                 NSSBR_LOGERR("msg_post_with_ref failed]major=%u,minor=%u,type=%u", \
                     msg->param.major_type, msg->param.minor_type, msg->param.op_type); \
            }\
        } \
    } while (0)

NSTACK_STATIC inline void
_sbr_construct_msg (data_com_msg * m, u16 major_type, u16 minor_type,
                    u16 type, sbr_socket_t * sk)
{
  m->param.module_type = MSG_MODULE_SBR;
  m->param.major_type = major_type;
  m->param.minor_type = minor_type;
  m->param.err = 0;
  m->param.op_type = type;
  sys_sem_init (&m->param.op_completed);
  m->param.receiver = ss_get_recv_obj (sbr_get_conn (sk));
  m->param.comm_receiver = ss_get_comm_private_data (sbr_get_conn (sk));
  m->param.extend_member_bit = 0;
}

#define sbr_construct_msg(m, major_type, minor_type, type, sk) { \
        _sbr_construct_msg(m, major_type, minor_type, type, sk); \
        NSSBR_LOGINF("fd=%d,conn=%p,private_data=%p", sk->fd, sbr_get_conn(sk), ss_get_private_data(sbr_get_conn(sk))); \
    }

#define sbr_construct_msg_dbg(m, major_type, minor_type, type, sk) { \
        _sbr_construct_msg(m, major_type, minor_type, type, sk); \
        NSSBR_LOGDBG("fd=%d,conn=%p,private_data=%p", sk->fd, sbr_get_conn(sk), ss_get_private_data(sbr_get_conn(sk))); \
    }

/*****************************************************************************
*   Prototype    : sbr_attach_msg
*   Description  : use buf's msg first
*   Input        : sbr_socket_t * sk
*                  struct pbuf* buf
*   Output       : None
*   Return Value : static inline data_com_msg*
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static inline data_com_msg *
sbr_attach_msg (sbr_socket_t * sk, struct spl_pbuf *buf)
{
  data_com_msg *m = NULL;
  if (!sk)
    {
      return m;
    }

  if (buf && buf->msg)
    {
      m = (data_com_msg *) ADDR_SHTOL (buf->msg);
    }
  else
    {
      m = SBR_MSG_MALLOC (sk);
    }

  return m;
}

/*****************************************************************************
*   Prototype    : sbr_handle_socket
*   Description  : create socket
*   Input        : sbr_socket_t * sk
*                  netconn_type_t type
*                  u8 proto
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_handle_socket (sbr_socket_t * sk, spl_netconn_type_t type, u8 proto)
{
  data_com_msg *m = SBR_MSG_MALLOC (sk);

  if (!m)
    {
      NSSBR_LOGERR ("malloc msg failed]fd=%d", sk->fd);
      sbr_set_sk_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_NEWCONN,
                     MSG_SYN_POST, sk);
  msg_new_netconn *p = (msg_new_netconn *) m->buffer;
  p->conn = (spl_netconn_t *) ADDR_LTOSH (sbr_get_conn (sk));
  p->type = type;
  p->proto = proto;
  p->socket = sk->fd;
  p->extend_member_bit = 0;
  SBR_MSG_POST (m, sbr_get_msg_box (sk));
  int err = sbr_spl_err_to_errno (m->param.err);
  SBR_MSG_FREE (m);
  if (err != 0)
    {
      NSSBR_LOGERR ("handle socket failed]fd=%d,type=%d,proto=%u,err=%d",
                    sk->fd, type, proto, err);
      sbr_set_sk_errno (sk, err);
      return -1;
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_handle_bind
*   Description  : bind
*   Input        : sbr_socket_t * sk
*                  spl_ip_addr_t * addr
*                  u16 port
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_handle_bind (sbr_socket_t * sk, spl_ip_addr_t * addr, u16 port)
{
  data_com_msg *m = SBR_MSG_MALLOC (sk);

  if (!m)
    {
      NSSBR_LOGERR ("malloc msg failed]fd=%d", sk->fd);
      sbr_set_sk_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_BIND, MSG_SYN_POST,
                     sk);
  msg_bind *p = (msg_bind *) m->buffer;
  p->ipaddr = *addr;
  p->port = port;
  p->extend_member_bit = 0;
  SBR_MSG_POST (m, sbr_get_msg_box (sk));
  int err = sbr_spl_err_to_errno (m->param.err);
  SBR_MSG_FREE (m);
  if (err != 0)
    {
      NSSBR_LOGERR ("handle bind failed]fd=%d,err=%d", sk->fd, err);
      sbr_set_sk_errno (sk, err);
      return -1;
    }

  return 0;
}

int
sbr_handle_listen (sbr_socket_t * sk, int backlog)
{
  data_com_msg *m = SBR_MSG_MALLOC (sk);

  if (!m)
    {
      sbr_set_sk_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_LISTEN,
                     MSG_SYN_POST, sk);
  msg_listen *p = (msg_listen *) m->buffer;
  p->conn_pool = sbr_get_conn_pool ();
  p->backlog = backlog;
  p->extend_member_bit = 0;
  SBR_MSG_POST (m, sbr_get_msg_box (sk));
  int err = sbr_spl_err_to_errno (m->param.err);
  SBR_MSG_FREE (m);
  if (err != 0)
    {
      NSSBR_LOGERR ("handle listen failed]fd=%d,err=%d", sk->fd, err);
      sbr_set_sk_errno (sk, err);
      return -1;
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_handle_connect
*   Description  : connect
*   Input        : sbr_socket_t * sk
*                  spl_ip_addr_t * addr
*                  u16 port
*                  spl_ip_addr_t* local_ip
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_handle_connect (sbr_socket_t * sk, spl_ip_addr_t * addr, u16 port,
                    spl_ip_addr_t * local_ip)
{
  data_com_msg *m = SBR_MSG_MALLOC (sk);

  if (!m)
    {
      NSSBR_LOGERR ("malloc msg failed]fd=%d", sk->fd);
      sbr_set_sk_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_CONNECT,
                     MSG_SYN_POST, sk);
  msg_connect *p = (msg_connect *) m->buffer;
  p->local_ip = *local_ip;
  p->ipaddr = *addr;
  p->port = port;
  p->extend_member_bit = 0;
  SBR_MSG_POST (m, sbr_get_msg_box (sk));
  int err = sbr_spl_err_to_errno (m->param.err);
  SBR_MSG_FREE (m);
  if (err != 0)
    {
      NSSBR_LOGERR ("handle connect failed]fd=%d,err=%d", sk->fd, err);
      sbr_set_sk_errno (sk, err);
      return -1;
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_handle_get_name
*   Description  : get name
*   Input        : sbr_socket_t * sk
*                  struct sockaddr * name
*                  socklen_t * namelen
*                  u8 cmd
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_handle_get_name (sbr_socket_t * sk, struct sockaddr *name,
                     socklen_t * namelen, u8 cmd)
{
  data_com_msg *m = SBR_MSG_MALLOC (sk);

  if (!m)
    {
      NSSBR_LOGERR ("malloc msg failed]fd=%d", sk->fd);
      sbr_set_sk_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_GETSOCK_NAME,
                     MSG_SYN_POST, sk);
  msg_getaddrname *p = (msg_getaddrname *) m->buffer;
  p->cmd = cmd;
  p->extend_member_bit = 0;
  SBR_MSG_POST (m, sbr_get_msg_box (sk));
  int err = sbr_spl_err_to_errno (m->param.err);
  if (err != 0)
    {
      NSSBR_LOGERR ("handle get name failed]fd=%d,err=%d", sk->fd, err);
      goto error;
    }
  else
    {
      if (*namelen > sizeof (p->sock_addr))
        {
          *namelen = sizeof (p->sock_addr);
        }

      int ret = MEMCPY_S (name, *namelen, &p->sock_addr, *namelen);
      if (0 != ret)
        {
          NSSBR_LOGERR ("MEMCPY_S failed]fd=%d,ret=%d", sk->fd, ret);
          err = EINVAL;
          goto error;
        }

      *namelen = sizeof (p->sock_addr);
    }

  SBR_MSG_FREE (m);
  return 0;

error:
  sbr_set_sk_errno (sk, err);
  SBR_MSG_FREE (m);
  return -1;
}

/*****************************************************************************
*   Prototype    : sbr_handle_setsockopt
*   Description  : msg box will changed in IP_TOS
*   Input        : sbr_socket_t * sk
*                  int level
*                  int optname
*                  const void *optval
*                  socklen_t optlen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_handle_setsockopt (sbr_socket_t * sk, int level, int optname,
                       const void *optval, socklen_t optlen)
{
  data_com_msg *m = SBR_MSG_MALLOC (sk);

  if (!m)
    {
      NSSBR_LOGERR ("malloc msg failed]fd=%d", sk->fd);
      sbr_set_sk_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_SET_SOCK_OPT,
                     MSG_SYN_POST, sk);
  msg_setgetsockopt *p = (msg_setgetsockopt *) m->buffer;
  p->extend_member_bit = 0;
  p->level = level;
  p->optname = optname;
  p->msg_box = NULL;            /* init the value to avoid unexpected consequences */

  if (optlen > sizeof (p->optval))
    {
      optlen = sizeof (p->optval);
    }

  p->optlen = optlen;
  int err;
  int ret = MEMCPY_S (&p->optval, sizeof (p->optval), optval, optlen);
  if (0 != ret)
    {
      NSSBR_LOGERR ("MEMCPY_S failed]fd=%d,ret=%d", sk->fd, ret);
      err = EINVAL;
      goto error;
    }

  SBR_MSG_POST (m, sbr_get_msg_box (sk));
  err = sbr_spl_err_to_errno (m->param.err);
  if (err != 0)
    {
      NSSBR_LOGERR ("handle setsockopt failed]fd=%d,err=%d", sk->fd, err);
      goto error;
    }

  if (IPPROTO_IP == level && IP_TOS == optname && p->msg_box)
    {
      ss_set_msg_box (sbr_get_conn (sk),
                      (mring_handle) ADDR_SHTOL (p->msg_box));
    }

  SBR_MSG_FREE (m);
  return 0;

error:
  sbr_set_sk_errno (sk, err);
  SBR_MSG_FREE (m);
  return -1;
}

/*****************************************************************************
*   Prototype    : sbr_handle_getsockopt
*   Description  : get sockopt
*   Input        : sbr_socket_t * sk
*                  int level
*                  int optname
*                  void *optval
*                  socklen_t* optlen
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_handle_getsockopt (sbr_socket_t * sk, int level, int optname,
                       void *optval, socklen_t * optlen)
{
  data_com_msg *m = SBR_MSG_MALLOC (sk);

  if (!m)
    {
      NSSBR_LOGERR ("malloc msg failed]fd=%d", sk->fd);
      sbr_set_sk_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_GET_SOCK_OPT,
                     MSG_SYN_POST, sk);
  msg_setgetsockopt *p = (msg_setgetsockopt *) m->buffer;
  p->extend_member_bit = 0;
  p->level = level;
  p->optname = optname;

  if (*optlen > sizeof (p->optval))
    {
      *optlen = sizeof (p->optval);
    }

  p->optlen = *optlen;
  int err;
  int ret = MEMCPY_S (&p->optval, sizeof (p->optval), optval, *optlen);
  if (0 != ret)
    {
      NSSBR_LOGERR ("MEMCPY_S failed]fd=%d,ret=%d", sk->fd, ret);
      err = EINVAL;
      goto error;
    }

  SBR_MSG_POST (m, sbr_get_msg_box (sk));

  err = sbr_spl_err_to_errno (m->param.err);
  if (err != 0)
    {
      NSSBR_LOGERR ("handle getsockopt failed]fd=%d,err=%d", sk->fd, err);
      goto error;
    }

  ret = MEMCPY_S (optval, *optlen, &p->optval.int_optval, *optlen);
  if (0 != ret)
    {
      NSSBR_LOGERR ("MEMCPY_S failed]fd=%d,ret=%d", sk->fd, ret);
      err = EINVAL;
      goto error;
    }

  SBR_MSG_FREE (m);
  return 0;

error:
  sbr_set_sk_errno (sk, err);
  SBR_MSG_FREE (m);
  return -1;
}

/*****************************************************************************
*   Prototype    : sbr_handle_close
*   Description  : need care msg_box_ref,make sure to finalize this message
*   Input        : sbr_socket_t * sk
*                  u8 how
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_handle_close (sbr_socket_t * sk, u8 how)
{
  data_com_msg *m = sbr_attach_msg (sk,
                                    (struct spl_pbuf *)
                                    ADDR_SHTOL (sbr_get_fd_share
                                                (sk)->recoder.head));

  if (!m)
    {
      NSSBR_LOGERR ("attach msg failed]fd=%d", sk->fd);
      sbr_set_sk_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_DELCON,
                     MSG_ASYN_POST, sk);
  msg_delete_netconn *p = (msg_delete_netconn *) m->buffer;
  p->extend_member_bit = 0;
  p->buf = NULL;
  p->time_started = sys_now ();
  p->shut = how;
  p->pid = get_sys_pid ();
  p->conn = (spl_netconn_t *) ADDR_LTOSH (sbr_get_conn (sk));
  p->notify_omc = FALSE;
  p->msg_box_ref = SPL_MSG_BOX_NUM;

  /* to ensure that the last deal with SPL_API_DO_DELCON message */
  int i;
  for (i = 0; i < SPL_MSG_BOX_NUM; ++i)
    {
      SBR_MSG_POST (m,
                    ss_get_instance_msg_box (ss_get_bind_thread_index
                                             (sbr_get_conn (sk)), i));
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : sbr_handle_udp_send
*   Description  : udp send
*   Input        : sbr_socket_t * sk
*                  struct netbuf *buf
*                  spl_ip_addr_t* local_ip
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_handle_udp_send (sbr_socket_t * sk, struct spl_netbuf *buf,
                     spl_ip_addr_t * local_ip)
{
  data_com_msg *m =
    sbr_attach_msg (sk, (struct spl_pbuf *) ADDR_SHTOL (buf->p));

  if (!m)
    {
      NSSBR_LOGERR ("attach msg failed]fd=%d", sk->fd);
      sbr_set_sk_io_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg_dbg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_SEND,
                         MSG_ASYN_POST, sk);
  msg_send_buf *p = (msg_send_buf *) m->buffer;
  p->local_ip.addr = local_ip->addr;
  int ret = MEMCPY_S (&p->addr, sizeof (spl_ip_addr_t), &buf->addr,
                      sizeof (spl_ip_addr_t));
  if (ret != 0)
    {
      NSSBR_LOGERR ("MEMCPY_S failed]fd=%d,ret=%d", sk->fd, ret);
      sbr_set_sk_io_errno (sk, EINVAL);
      SBR_MSG_FREE (m);
      return -1;
    }

  p->p = buf->p;
  p->port = buf->port;
  p->extend_member_bit = 0;
  SBR_MSG_POST_RET (m, sbr_get_msg_box (sk), ret);

  if (0 == ret)
    {
      return 0;
    }
  else
    {
      NSSBR_LOGERR ("post msg failed]fd=%d", sk->fd);
      sbr_set_sk_io_errno (sk, EAGAIN);
      return -1;
    }
}

/*****************************************************************************
*   Prototype    : sbr_handle_free_recv_buf
*   Description  : free recv buf,can't free buf in app
*   Input        : sbr_socket_t * sk
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
sbr_handle_free_recv_buf (sbr_socket_t * sk)
{
  data_com_msg *m = sbr_attach_msg (sk,
                                    (struct spl_pbuf *)
                                    ADDR_SHTOL (sbr_get_fd_share
                                                (sk)->recoder.head));

  if (!m)
    {
      NSSBR_LOGERR ("attach msg failed]fd=%d", sk->fd);
      return;
    }

  sbr_construct_msg_dbg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_PBUF_FREE,
                         MSG_ASYN_POST, sk);
  msg_free_buf *p = (msg_free_buf *) m->buffer;
  p->extend_member_bit = 0;
  p->buf = sbr_get_fd_share (sk)->recoder.head;
  sbr_get_fd_share (sk)->recoder.head = NULL;
  sbr_get_fd_share (sk)->recoder.tail = NULL;
  sbr_get_fd_share (sk)->recoder.totalLen = 0;
  SBR_MSG_POST (m, sbr_get_msg_box (sk));
}

void
sbr_handle_free_send_buf (sbr_socket_t * sk, struct spl_pbuf *buf)
{
  if (buf != NULL)
    {
      data_com_msg *m = sbr_attach_msg (sk, buf);

      if (!m)
        {
          NSSBR_LOGERR ("attach msg failed]fd=%d", sk->fd);
          return;
        }

      sbr_construct_msg_dbg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_PBUF_FREE,
                             MSG_ASYN_POST, sk);
      msg_free_buf *p = (msg_free_buf *) m->buffer;
      p->extend_member_bit = 0;
      p->buf = (struct spl_pbuf *) ADDR_LTOSH (buf);
      SBR_MSG_POST (m, sbr_get_msg_box (sk));
    }
}

/*****************************************************************************
*   Prototype    : sbr_handle_shutdown
*   Description  : shut down
*   Input        : sbr_socket_t * sk
*                  u8 how
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_handle_shutdown (sbr_socket_t * sk, u8 how)
{
  int err;
  data_com_msg *m = SBR_MSG_MALLOC (sk);

  if (!m)
    {
      NSSBR_LOGERR ("malloc msg failed]fd=%d", sk->fd);
      sbr_set_sk_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_CLOSE, MSG_SYN_POST,
                     sk);
  msg_close *p = (msg_close *) m->buffer;
  p->extend_member_bit = 0;
  p->time_started = sys_now ();
  p->shut = how;
  SBR_MSG_POST (m, sbr_get_msg_box (sk));
  err = sbr_spl_err_to_errno (m->param.err);
  SBR_MSG_FREE (m);
  if (err != 0)
    {
      NSSBR_LOGERR ("handle getsockopt failed]fd=%d,err=%d", sk->fd, err);
      sbr_set_sk_errno (sk, err);
      return -1;
    }

  return 0;
}

void
sbr_handle_tcp_recv (sbr_socket_t * sk, u32 len, struct spl_pbuf *buf)
{
  data_com_msg *m = sbr_attach_msg (sk, buf);

  if (!m)
    {
      NSSBR_LOGERR ("attach msg failed]fd=%d", sk->fd);
      return;
    }

  sbr_construct_msg_dbg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_RECV,
                         MSG_ASYN_POST, sk);
  msg_recv_buf *p = (msg_recv_buf *) m->buffer;
  p->extend_member_bit = 0;
  p->len = len;
  p->p = (struct spl_pbuf *) ADDR_LTOSH (buf);
  SBR_MSG_POST (m, sbr_get_msg_box (sk));
}

int
sbr_handle_tcp_send (sbr_socket_t * sk, size_t size, struct spl_pbuf *buf,
                     u8 api_flag)
{
  int ret;
  data_com_msg *m = sbr_attach_msg (sk, buf);

  if (!m)
    {
      NSSBR_LOGERR ("attach msg failed]fd=%d", sk->fd);
      sbr_set_sk_io_errno (sk, ENOMEM);
      return -1;
    }

  sbr_construct_msg_dbg (m, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_WRITE,
                         MSG_ASYN_POST, sk);
  msg_write_buf *p = (msg_write_buf *) m->buffer;
  p->extend_member_bit = 0;
  p->len = size;
  p->p = (struct spl_pbuf *) ADDR_LTOSH (buf);
  p->apiflags = api_flag;
  SBR_MSG_POST_RET (m, sbr_get_msg_box (sk), ret);

  if (0 == ret)
    {
      return 0;
    }
  else
    {
      NSSBR_LOGERR ("post msg failed]fd=%d", sk->fd);
      sbr_set_sk_io_errno (sk, EAGAIN);
      return -1;
    }
}

/* need delete sbr_handle_app_touch */
void
sbr_handle_app_touch (void)
{
}
