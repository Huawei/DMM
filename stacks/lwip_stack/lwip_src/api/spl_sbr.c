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

#include "stackx_spl_share.h"
#include "spl_api.h"
#include "ip.h"
#include "spl_api_msg.h"
#include "stackx_spl_msg.h"
#include "internal_msg.h"
#include "spl_sbr.h"
#include "spl_pbuf.h"

/**
 * common operation for sbr message.
 *
 * @param msg the api_msg_msg describing the connection type
 */
int
do_sbrmsg (data_com_msg * m)
{
  return 0;
}

/**
 * Create a new pcb of a specific type inside a netconn.
 * Called from netconn_new_with_proto_and_callback.
 *
 * @param msg the api_msg_msg describing the connection type
 */
static int
_do_newconn (data_com_msg * m)
{
  m->param.err = ERR_OK;

  msg_new_netconn *_m = (msg_new_netconn *) m->buffer;
  m->param.err = spl_pcb_new (_m);

  if (ERR_OK != m->param.err)
    {
      NSPOL_LOGERR ("pcb_new err]conn=%p,pid=%u,err=%d", _m->conn,
                    m->param.recycle_pid, m->param.err);
      goto ERROR;
    }

  NSFW_LOGINF ("alloc pcb]conn=%p,pcb=%p,pid=%u", _m->conn,
               _m->conn->private_data, m->param.recycle_pid);

  /* sbr use it to set receiver after new connction */
  m->param.receiver = (i64) & _m->conn->private_data;
  m->param.comm_receiver = (i64) & _m->conn->comm_pcb_data;
  _m->conn->recv_obj = m->param.receiver;

  SYNC_MSG_ACK (m);
  return 0;

ERROR:
  SYNC_MSG_ACK (m);
  return 0;
}

int
_do_connect (data_com_msg * m)
{
  m->param.err = ERR_OK;

  struct common_pcb *cpcb = COMM_PRIVATE_PTR (m);
  if (cpcb == NULL)
    {
      m->param.err = ERR_CLSD;
      SYNC_MSG_ACK (m);
      return 0;
    }

  msg_connect *_m = (msg_connect *) m->buffer;
  do_connect (cpcb, _m);

    /**
     * err == ERR_OK only applies for blocking connction, so others mean
     * in progress for nonblocking connection or failed to connect
     * cpcb may be NULL, so don't change the order of the 2 ifs.
     */
  if (m->param.err != ERR_OK || cpcb->type != SPL_NETCONN_TCP)
    SYNC_MSG_ACK (m);

  return 0;
}

/**
 * Close a TCP pcb contained in a netconn
 * Called from netconn_close
 *
 * @param msg the api_msg_msg pointing to the connection
 */
int
_do_close (data_com_msg * m)
{
  m->param.err = ERR_OK;

  struct common_pcb *cpcb = COMM_PRIVATE_PTR (m);
  if (cpcb != NULL)
    {
      msg_close *_m = (msg_close *) m->buffer;
      do_close (cpcb, _m);
    }

  /* if cpcb == NULL, assuming the close is okay, err = ERR_OK */
  SYNC_MSG_ACK (m);

  return 0;
}

/**
 * Delete the pcb inside a netconn.
 * Called from netconn_delete.
 *
 * @param msg the data_com_msg to handle
 */
static int
_do_delconn (data_com_msg * m)
{
  m->param.err = ERR_OK;
  msg_delete_netconn *_m = (msg_delete_netconn *) m->buffer;

  if (0 == (--_m->msg_box_ref))
    {
      /* the aync msg is released inside */
      ss_recycle_conn ((void *) _m, do_try_delconn);
    }

  return 0;
}

/**
 * handle message to send UDP packets.
 *
 * @param msg the data_com_msg to handle
 */
static int
_do_send (data_com_msg * m)
{
  m->param.err = ERR_OK;

  struct common_pcb *cpcb = COMM_PRIVATE_PTR (m);
  msg_send_buf *_m = (msg_send_buf *) m->buffer;
  if (cpcb == NULL)
    {
      NS_LOG_CTRL (LOG_CTRL_SEND, STACKX, "NSPOL", NSLOG_ERR,
                   "failed to find target pcb, drop the message]"
                   "module=%u, major=%u, minor=%u",
                   m->param.module_type,
                   m->param.major_type, m->param.minor_type);

      spl_pbuf_free (_m->p);
      _m->p = NULL;
      ASYNC_MSG_FREE (m);
      return -1;
    }

  do_send (cpcb, _m);

  ASYNC_MSG_FREE (m);

  return 0;
}

/**
 * handle message to send TCP packets.
 *
 * @param msg the data_com_msg to handle
 */
static int
_do_write (data_com_msg * m)
{
  m->param.err = ERR_OK;

  void *tpcb = TCP_PRIVATE_PTR (m);
  struct common_pcb *cpcb = COMM_PRIVATE_PTR (m);

  msg_write_buf *_m = (msg_write_buf *) m->buffer;
  if ((tpcb == NULL) || (cpcb == NULL))
    {
      NS_LOG_CTRL (LOG_CTRL_WRITE, STACKX, "NSPOL", NSLOG_ERR,
                   "failed to find target pcb, drop the message]"
                   "module=%u, major=%u, minor=%u",
                   m->param.module_type,
                   m->param.major_type, m->param.minor_type);

      spl_pbuf_free (_m->p);
      _m->p = NULL;
      ASYNC_MSG_FREE (m);
      return -1;
    }

  do_write (cpcb, _m);

  return 0;
}

/**
 * handle message to receive.
 *
 * @param msg the data_com_msg to handle
 */
static int
_do_recv (data_com_msg * m)
{
  m->param.err = ERR_OK;

  msg_recv_buf *_m = (msg_recv_buf *) m->buffer;
  void *tpcb = TCP_PRIVATE_PTR (m);
  struct common_pcb *cpcb = COMM_PRIVATE_PTR (m);

  if ((tpcb == NULL) || (cpcb == NULL))
    {
      NS_LOG_CTRL (LOG_CTRL_RECV, STACKX, "NSPOL", NSLOG_ERR,
                   "failed to find target pcb, drop the message]"
                   "module=%u, major=%u, minor=%u",
                   m->param.module_type,
                   m->param.major_type, m->param.minor_type);

      spl_pbuf_free (_m->p);
      _m->p = NULL;
      ASYNC_MSG_FREE (m);
      return -1;
    }

  do_recv (cpcb, _m);
  ASYNC_MSG_FREE (m);
  return 0;
}

/**
 * handle message to bind local address.
 *
 * @param msg the data_com_msg to handle
 */
static int
_do_bind (data_com_msg * m)
{
  m->param.err = ERR_OK;

  struct common_pcb *cpcb = COMM_PRIVATE_PTR (m);
  if (cpcb == NULL)
    {
      NSPOL_LOGERR ("failed to find target pcb, drop the message]"
                    "module=%u, major=%u, minor=%u",
                    m->param.module_type,
                    m->param.major_type, m->param.minor_type);

      m->param.err = ERR_VAL;
      SYNC_MSG_ACK (m);
      return -1;
    }

  msg_bind *_m = (msg_bind *) m->buffer;
  do_bind (cpcb, _m);

  SYNC_MSG_ACK (m);

  return 0;
}

/**
 * handle message to listen for new connection.
 *
 * @param msg the data_com_msg to handle
 */
static int
_do_listen (data_com_msg * m)
{
  m->param.err = ERR_OK;

  struct common_pcb *cpcb = COMM_PRIVATE_PTR (m);
  if (cpcb == NULL)
    {
      NSPOL_LOGERR ("failed to find target pcb, drop the message]"
                    "module=%u, major=%u, minor=%u",
                    m->param.module_type,
                    m->param.major_type, m->param.minor_type);

      m->param.err = ERR_CONN;
      SYNC_MSG_ACK (m);
      return -1;
    }

  msg_listen *_m = (msg_listen *) m->buffer;
  do_listen (cpcb, _m);

  /* Update since pcb may have been changed */
  //m->param.receiver = (i64)&_m->conn->private_data;
  SYNC_MSG_ACK (m);
  return 0;
}

/**
 * handle message to set socket option.
 *
 * @param msg the data_com_msg to handle
 */
static int
_do_setsockopt (data_com_msg * m)
{
  m->param.err = ERR_OK;

  struct common_pcb *cpcb = COMM_PRIVATE_PTR (m);
  if (cpcb == NULL)
    {

      NSPOL_LOGERR ("failed to find target pcb, drop the message]"
                    "module=%u, major=%u, minor=%u",
                    m->param.module_type,
                    m->param.major_type, m->param.minor_type);

      m->param.err = ERR_VAL;
      SYNC_MSG_ACK (m);
      return -1;
    }

  msg_setgetsockopt *_m = (msg_setgetsockopt *) m->buffer;
  do_setsockopt_internal (cpcb, _m);

  SYNC_MSG_ACK (m);

  return 0;
}

/**
 * handle message to get socket option.
 *
 * @param msg the data_com_msg to handle
 */
static int
_do_getsockopt (data_com_msg * m)
{
  m->param.err = ERR_OK;

  struct common_pcb *cpcb = COMM_PRIVATE_PTR (m);
  if (cpcb == NULL)
    {
      NSPOL_LOGERR ("failed to find target pcb, drop the message]"
                    "module=%u, major=%u, minor=%u",
                    m->param.module_type,
                    m->param.major_type, m->param.minor_type);

      m->param.err = ERR_VAL;
      SYNC_MSG_ACK (m);
      return -1;
    }

  msg_setgetsockopt *_m = (msg_setgetsockopt *) m->buffer;
  do_getsockopt_internal (cpcb, _m);

  SYNC_MSG_ACK (m);

  return 0;
}

/**
 * handle message to free pbuf which never used afterwards by application.
 *
 * @param msg the data_com_msg to handle
 */

static int
_do_pbuf_free (data_com_msg * m)
{
  m->param.err = ERR_OK;

  msg_free_buf *_m = (msg_free_buf *) m->buffer;
  do_pbuf_free (_m->buf);

  ASYNC_MSG_FREE (m);

  return 0;
}

static int
_do_getsock_name (data_com_msg * m)
{
  m->param.err = ERR_OK;

  struct common_pcb *cpcb = COMM_PRIVATE_PTR (m);
  if (cpcb == NULL)
    {
      NSPOL_LOGERR ("failed to find target pcb, drop the message]"
                    "module=%u, major=%u, minor=%u",
                    m->param.module_type,
                    m->param.major_type, m->param.minor_type);

      m->param.err = ERR_VAL;
      SYNC_MSG_ACK (m);
      return -1;
    }

  msg_getaddrname *_m = (msg_getaddrname *) m->buffer;
  do_getsockname (cpcb, _m);

  SYNC_MSG_ACK (m);

  return 0;
}

/* app send its version info to nStackMain */
static int
_do_app_touch (data_com_msg * m)
{
  m->param.err = ERR_OK;

  msg_app_touch *_m = (msg_app_touch *) m->buffer;
  do_app_touch (_m);

  ASYNC_MSG_FREE (m);
  return 0;
}

/**
 * process message from sbr module, all the processing function
 * is registered when nstack is up.
 *
 * @param msg the api_msg_msg pointing to the connection
 */
int
spl_sbr_process (data_com_msg * m)
{
  if (m == NULL)
    {
      NSPOL_LOGERR ("message is NULL");
      return -1;
    }

  return call_msg_fun (m);
}

int
spl_unsupport_process (data_com_msg * m)
{
  NSPOL_LOGINF (TCPIP_DEBUG, "module_type=%u,major_type=%u,minor_type=%u",
                m->param.module_type, m->param.major_type,
                m->param.minor_type);
  if (MSG_SYN_POST == m->param.op_type)
    {
      m->param.err = ERR_EPROTONOSUPPORT;
      SYNC_MSG_ACK (m);
    }
  else
    {
      ASYNC_MSG_FREE (m);
    }

  return -1;
}

REGIST_MSG_UNSUPPORT_FUN (spl_unsupport_process);
REGIST_MSG_MODULE_MAJOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                             do_sbrmsg);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_NEWCONN, _do_newconn);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_CONNECT, _do_connect);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_CLOSE, _do_close);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_DELCON, _do_delconn);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_SEND, _do_send);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_WRITE, _do_write);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_RECV, _do_recv);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_BIND, _do_bind);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_LISTEN, _do_listen);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_GET_SOCK_OPT, _do_getsockopt);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_SET_SOCK_OPT, _do_setsockopt);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_PBUF_FREE, _do_pbuf_free);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API,
                                   SPL_API_DO_GETSOCK_NAME, _do_getsock_name);
REGIST_MSG_MODULE_MAJOR_MINOR_FUN (MSG_MODULE_SBR, SPL_TCPIP_NEW_MSG_API, SPL_API_DO_APP_TOUCH, _do_app_touch); /* app send its version info to nStackMain */
