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

/*==============================================*
 *      include header files                    *
 *----------------------------------------------*/

#include "types.h"
#include "nstack_securec.h"
#include "nsfw_init.h"
#include "common_mem_api.h"

#include "nsfw_mgr_com_api.h"
#include "mgr_com.h"
#include "nstack_log.h"
#include "nsfw_base_linux_api.h"

#include <sys/un.h>
#include <stddef.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "nsfw_maintain_api.h"
#include "nsfw_ps_api.h"
#include "nsfw_fd_timer_api.h"

#include "common_func.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

/* *INDENT-OFF* */

/* *INDENT-OFF* */
nsfw_mgr_msg_fun g_mgr_fun[MGR_MSG_MAX][NSFW_MGRCOM_MAX_PROC_FUN];
nsfw_mgr_init_cfg g_mgr_com_cfg;
nsfw_mgr_sock_map g_mgr_socket_map = {{0}, NULL};
nsfw_mgrcom_stat g_mgr_stat;
nsfw_mgrcom_proc g_ep_proc = { 0 };
/* *INDENT-ON* */

u32 g_mgr_sockfdmax = NSFW_MGRCOM_MAX_SOCKET;

char g_proc_info[NSFW_PROC_MAX][NSTACK_MAX_PROC_NAME_LEN] = {
  "", "nStackMain", "nStackMaster", "nStackLib", "nStackTools", "nStackCtrl",
  "", "", "", "", "", "", "", "", "", ""
};

/* *INDENT-ON* */

int g_thread_policy = 0;
int g_thread_pri = 0;

void
nsfw_com_attr_set (int policy, int pri)
{
  g_thread_policy = policy;
  g_thread_pri = pri;
}

char *
nsfw_get_proc_name (u8 proc_type)
{
  if (proc_type >= NSFW_PROC_MAX || NSFW_PROC_NULL == proc_type)
    {
      return NULL;
    }

  return g_proc_info[proc_type];
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_reg_msg_fun
*   Description  : reg the callback function when receive new message
*   Input        : u16 msg_type
*                  nsfw_mgr_msg_fun fun
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_reg_msg_fun (u16 msg_type, nsfw_mgr_msg_fun fun)
{
  u32 i;
  if (MGR_MSG_MAX <= msg_type)
    {
      NSFW_LOGERR ("reg mgr_msg]msg_type=%u,fun=%p", msg_type, fun);
      return FALSE;
    }

  for (i = 0; i < NSFW_MGRCOM_MAX_PROC_FUN; i++)
    {
      if (NULL == g_mgr_fun[msg_type][i])
        {
          g_mgr_fun[msg_type][i] = fun;
          NSFW_LOGINF ("reg mgr_msg fun suc]msg_type=%u,fun=%p", msg_type,
                       fun);
          return TRUE;
        }
    }

  NSFW_LOGERR ("reg mgr_msg type full]msg_type=%u,fun=%p", msg_type, fun);
  return FALSE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_null_rspmsg_alloc
*   Description  : alloc null response msg for receive message buffer
*   Input        : None
*   Output       : None
*   Return Value : nsfw_mgr_msg*
*   Calls        :
*   Called By    :
*****************************************************************************/
nsfw_mgr_msg *
nsfw_mgr_null_rspmsg_alloc ()
{
  return nsfw_mgr_msg_alloc (MGR_MSG_NULL, NSFW_PROC_NULL);
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_msg_alloc
*   Description  : alloc new request message to send
*   Input        : u16 msg_type
*                  u8 dst_proc_type
*   Output       : None
*   Return Value : nsfw_mgr_msg*
*   Calls        :
*   Called By    :
*****************************************************************************/
nsfw_mgr_msg *
nsfw_mgr_msg_alloc (u16 msg_type, u8 dst_proc_type)
{
  nsfw_mgr_msg *p_msg = NULL;
  u8 from_mem_flag = FALSE;
  u32 alloc_len = sizeof (nsfw_mgr_msg);

  if (MGR_MSG_LAG_QRY_RSP_BEGIN <= msg_type)
    {
      from_mem_flag = TRUE;
      alloc_len = NSFW_MGR_LARGE_MSG_LEN;
    }

  if ((NULL == g_mgr_com_cfg.msg_pool)
      && (MGR_MSG_INIT_NTY_REQ == msg_type
          || MGR_MSG_INIT_NTY_RSP == msg_type))
    {
      from_mem_flag = TRUE;
    }

  if (FALSE == from_mem_flag)
    {
      if (0 ==
          nsfw_mem_ring_dequeue (g_mgr_com_cfg.msg_pool, (void *) &p_msg))
        {
          NSFW_LOGERR ("alloc msg full]type=%u,dst=%u", msg_type,
                       dst_proc_type);
          return NULL;
        }
      alloc_len = sizeof (nsfw_mgr_msg);
    }
  else
    {
      p_msg = (nsfw_mgr_msg *) malloc (alloc_len);
    }

  if (NULL == p_msg)
    {
      NSFW_LOGERR ("alloc msg nul]type=%u,dst=%u", msg_type, dst_proc_type);
      return NULL;
    }

  if (EOK != MEMSET_S (p_msg, alloc_len, 0, alloc_len))
    {
      p_msg->from_mem = from_mem_flag;
      nsfw_mgr_msg_free (p_msg);
      NSFW_LOGERR ("alloc msg MEMSET_S failed]type=%u,dst=%u", msg_type,
                   dst_proc_type);
      return NULL;
    }
  p_msg->from_mem = from_mem_flag;

  if (msg_type < MGR_MSG_RSP_BASE && msg_type > 0)
    {
      p_msg->seq = common_mem_atomic32_add_return (&g_mgr_com_cfg.cur_idx, 1);
    }

  p_msg->from_mem = from_mem_flag;
  p_msg->msg_type = msg_type;
  p_msg->src_pid = get_sys_pid ();
  p_msg->src_proc_type = g_mgr_com_cfg.proc_type;
  p_msg->msg_len = alloc_len;
  p_msg->dst_proc_type = dst_proc_type;
  p_msg->alloc_flag = TRUE;
  p_msg->more_msg_flag = 0;

  g_mgr_stat.msg_alloc++;
  return p_msg;
}

static inline void
lint_lock_1 ()
{
  return;
}

static inline void
lint_unlock_1 ()
{
  return;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_rsp_msg_alloc
*   Description  : alloc response message from request message
*   Input        : nsfw_mgr_msg* req_msg
*   Output       : None
*   Return Value : nsfw_mgr_msg*
*   Calls        :
*   Called By    :
*****************************************************************************/
nsfw_mgr_msg *
nsfw_mgr_rsp_msg_alloc (nsfw_mgr_msg * req_msg)
{
  nsfw_mgr_msg *p_msg = NULL;
  if (NULL == req_msg)
    {
      NSFW_LOGERR ("req msg nul!");
      return NULL;
    }

  p_msg =
    nsfw_mgr_msg_alloc (req_msg->msg_type + MGR_MSG_RSP_BASE,
                        req_msg->src_proc_type);
  if (NULL == p_msg)
    {
      return NULL;
    }

  p_msg->dst_pid = req_msg->src_pid;
  p_msg->seq = req_msg->seq;
  return p_msg;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_msg_free
*   Description  : message free
*   Input        : nsfw_mgr_msg *msg
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
nsfw_mgr_msg_free (nsfw_mgr_msg * msg)
{
  if (NULL == msg)
    {
      return;
    }

  if (FALSE == msg->alloc_flag)
    {
      NSFW_LOGERR ("msg refree]msg=%p, type=%u", msg, msg->msg_type);
      return;
    }

  msg->alloc_flag = FALSE;

  if (TRUE == msg->from_mem)
    {
      if ((MGR_MSG_INIT_NTY_REQ == msg->msg_type
           || MGR_MSG_INIT_NTY_RSP == msg->msg_type)
          || (MGR_MSG_LAG_QRY_RSP_BEGIN <= msg->msg_type))
        {
          free (msg);
          g_mgr_stat.msg_free++;
          return;
        }
      NSFW_LOGERR ("msg err free]type=%u", msg->msg_type);
    }

  if (0 == nsfw_mem_ring_enqueue (g_mgr_com_cfg.msg_pool, msg))
    {
      NSFW_LOGERR ("msg free failed pool full]msg=%p, type=%u", msg,
                   msg->msg_type);
      return;
    }

  g_mgr_stat.msg_free++;
  return;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_get_listen_socket
*   Description  : get domain socket listen fd
*   Input        : None
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_mgr_get_listen_socket ()
{
  i32 fd, len;
  struct sockaddr_un un;

  if ((fd = nsfw_base_socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
      NSFW_LOGERR ("create sock failed!");
      return -1;
    }

  if (-1 == unlink ((char *) g_mgr_com_cfg.domain_path))
    {
      NSFW_LOGWAR ("unlink failed]error=%d", errno);
    }
  if (EOK != MEMSET_S (&un, sizeof (un), 0, sizeof (un)))
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("create sock MEMSET_S failed!] error=%d", errno);
      return -1;
    }

  un.sun_family = AF_UNIX;
  int retVal = STRCPY_S ((char *) un.sun_path, sizeof (un.sun_path),
                         (char *) g_mgr_com_cfg.domain_path);
  if (EOK != retVal)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("create sock STRCPY_S failed!] error=%d", errno);
      return -1;
    }

  int rc = nsfw_set_close_on_exec (fd);
  if (rc == -1)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("set exec err]fd=%d, errno=%d", fd, errno);
      return -1;
    }

  len =
    offsetof (struct sockaddr_un,
              sun_path) +strlen ((char *) g_mgr_com_cfg.domain_path);

  if (nsfw_base_bind (fd, (struct sockaddr *) &un, len) < 0)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("bind failed!]mgr_fd=%d,error=%d", fd, errno);
      return -1;
    }

  if (nsfw_base_listen (fd, 10) < 0)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("listen failed!]mgr_fd=%d,error=%d", fd, errno);
      return -1;
    }

  NSFW_LOGINF ("mgr com start with]mgr_fd=%d", fd);
  return fd;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_get_connect_socket
*   Description  : get new connect fd to destination procedure
*   Input        : u8 proc_type
*                  u32 host_pid
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_mgr_get_connect_socket (u8 proc_type, u32 host_pid)
{
  i32 fd, len;
  char *name;
  struct sockaddr_un un;
  const char *directory = NSFW_DOMAIN_DIR;
  const char *home_dir = getenv ("HOME");

  if (getuid () != 0 && home_dir != NULL)
    directory = home_dir;

  switch (proc_type)
    {
    case NSFW_PROC_MAIN:
      name = NSFW_MAIN_FILE;
      break;
    case NSFW_PROC_MASTER:
      name = NSFW_MASTER_FILE;
      break;
    case NSFW_PROC_ALARM:
      directory = "/tmp";
      name = NSFW_ALARM_FILE;
      break;
    default:
      NSFW_LOGERR ("get dst socket err]type=%u,pid=%u", proc_type, host_pid);
      return -1;
    }

  if ((fd = nsfw_base_socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
      NSFW_LOGERR ("create socket err]type=%u,pid=%u,errno=%d", proc_type,
                   host_pid, errno);
      return -1;
    }

  if (EOK != MEMSET_S (&un, sizeof (un), 0, sizeof (un)))
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("get dst socket err]mgr_fd=%d,type=%u,pid=%u", fd,
                   proc_type, host_pid);
      return -1;
    }

  struct timeval tv;
  tv.tv_sec = MGR_COM_RECV_TIMEOUT;
  tv.tv_usec = 0;
  if (nsfw_base_setsockopt
      (fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof tv))
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("setsockopt socket err]mgr_fd=%d,type=%u,pid=%u", fd,
                   proc_type, host_pid);
      return -1;
    }

  int rc = nsfw_set_close_on_exec (fd);
  if (rc == -1)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("set exec err]fd=%d, errno=%d", fd, errno);
      return -1;
    }

  int size, size_len;
  size = MAX_RECV_BUF_DEF;
  size_len = sizeof (size);
  if (0 >
      nsfw_base_setsockopt (fd, SOL_SOCKET, SO_RCVBUF, (void *) &size,
                            (socklen_t) size_len))
    {
      NSFW_LOGERR ("set socket opt err!]error=%d", errno);
    }

  if (0 >
      nsfw_base_setsockopt (fd, SOL_SOCKET, SO_SNDBUF, (void *) &size,
                            (socklen_t) size_len))
    {
      NSFW_LOGERR ("set socket opt err!]error=%d", errno);
    }

  un.sun_family = AF_UNIX;;
  int retVal = STRCPY_S ((char *) un.sun_path, sizeof (un.sun_path),
                         (char *) directory);
  if (EOK != retVal)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("create sock STRCPY_S failed!] error=%d", errno);
      return -1;
    }

  retVal = STRCAT_S (un.sun_path, sizeof (un.sun_path), name);
  if (EOK != retVal)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("create sock STRCAT_S failed!] error=%d", errno);
      return -1;
    }

  len = offsetof (struct sockaddr_un, sun_path) +strlen (un.sun_path);
  if (nsfw_base_connect (fd, (struct sockaddr *) &un, len) < 0)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR
        ("create socket err]mgr_fd=%d,type=%u,pid=%u,errno=%d,path=%s", fd,
         proc_type, host_pid, errno, un.sun_path);
      return -1;
    }

  NSFW_LOGINF ("get dst socket]mgr_fd=%d,type=%u,pid=%u", fd, proc_type,
               host_pid);
  return (fd);
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_new_socket
*   Description  : management the new fd to cache
*   Input        : u32 fd
*                  u8 proc_type
*                  u32 host_pid
*   Output       : None
*   Return Value : static inline u8
*   Calls        :
*   Called By    :
*****************************************************************************/
NSTACK_STATIC inline u8
nsfw_mgr_new_socket (i32 fd, u8 proc_type, u32 host_pid)
{
  nsfw_mgr_sock_info *sock_info = NULL;
  if (((i32) NSFW_MGR_FD_MAX <= fd) || (fd < 0) || (!g_mgr_socket_map.sock))
    {
      NSFW_LOGERR ("fd err]mgr_fd=%d, sock=%p", fd, g_mgr_socket_map.sock);
      return FALSE;
    }

  sock_info = &g_mgr_socket_map.sock[fd];
  if (host_pid != sock_info->host_pid)
    {
      NSFW_LOGDBG
        ("update sock info]mgr_fd=%d,old_pid=%u,new_pid=%u,type=%u", fd,
         sock_info->host_pid, host_pid, proc_type);
    }

  sock_info->host_pid = host_pid;
  sock_info->proc_type = proc_type;

  if (proc_type < NSFW_PROC_MAX)
    {
      g_mgr_socket_map.proc_cache[proc_type] = fd;
    }

  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_del_socket
*   Description  : delete the fd from cache when fd close
*   Input        : u32 fd
*   Output       : None
*   Return Value : static inline u8
*   Calls        :
*   Called By    :
*****************************************************************************/
NSTACK_STATIC inline u8
nsfw_mgr_del_socket (u32 fd)
{
  nsfw_mgr_sock_info *sock_info = NULL;
  if ((NSFW_MGR_FD_MAX <= fd) || (!g_mgr_socket_map.sock))
    {
      NSFW_LOGERR ("fd err]mgr_fd=%u, sock=%p", fd, g_mgr_socket_map.sock);
      return FALSE;
    }

  sock_info = &g_mgr_socket_map.sock[fd];

  if (sock_info->proc_type < NSFW_PROC_MAX
      && fd == g_mgr_socket_map.proc_cache[sock_info->proc_type])
    {
      g_mgr_socket_map.proc_cache[sock_info->proc_type] = 0;
    }

  NSFW_LOGDBG ("del sock]mgr_fd=%u,type=%u,pid=%u", fd,
               sock_info->proc_type, sock_info->host_pid);
  sock_info->host_pid = 0;
  sock_info->proc_type = 0;
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_get_dst_socket
*   Description  : get the fd to send message to destination procedure
*   Input        : u8 proc_type
*                  u32 dst_pid
*   Output       : None
*   Return Value : static inline i32
*   Calls        :
*   Called By    :
*****************************************************************************/
NSTACK_STATIC inline i32
nsfw_mgr_get_dst_socket (u8 proc_type, u32 dst_pid)
{
  i32 fd = -1;

  nsfw_mgr_sock_info *sock_info = NULL;

  if (proc_type < NSFW_PROC_MAX)
    {
      fd = g_mgr_socket_map.proc_cache[proc_type];
    }

  if (!g_mgr_socket_map.sock)
    {
      return -1;
    }

  if (fd > 0 && fd < (i32) NSFW_MGR_FD_MAX)
    {
      sock_info = &g_mgr_socket_map.sock[fd];
      if (sock_info->host_pid != 0)
        {
          if (0 == dst_pid || dst_pid == sock_info->host_pid)
            {
              return fd;
            }
        }
      else if (proc_type == sock_info->proc_type)
        {
          return fd;
        }
    }

  i32 i;
  for (i = 0; i < (i32) NSFW_MGR_FD_MAX; i++)
    {
      sock_info = &g_mgr_socket_map.sock[i];
      if (sock_info->host_pid != 0 && proc_type == sock_info->proc_type)
        {
          if (0 == dst_pid || dst_pid == sock_info->host_pid)
            {
              return i;
            }
        }
    }

  return -1;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_get_new_socket
*   Description  : get new connect
*   Input        : u8 proc_type
*                  u32 dst_pid
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_mgr_get_new_socket (u8 proc_type, u32 dst_pid)
{
  i32 fd = 0;
  fd = nsfw_mgr_get_connect_socket (proc_type, dst_pid);
  if (fd > 0)
    {
      (void) nsfw_mgr_new_socket (fd, proc_type, dst_pid);
      (void) nsfw_mgr_reg_sock_fun (fd, nsfw_mgr_new_msg);
    }
  return fd;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_clr_fd_lock
*   Description  : clear the fd lock when fork in child proc
*   Input        : None
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_clr_fd_lock ()
{
  i32 i;
  if (!g_mgr_socket_map.sock)
    {
      NSFW_LOGERR ("clr fd lock fail, sock is null");
      return FALSE;
    }
  for (i = 0; i < (i32) NSFW_MGR_FD_MAX; i++)
    {
      common_mem_spinlock_init (&(g_mgr_socket_map.sock[i].opr_lock));
    }
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_close_dst_proc
*   Description  : close the remote connect
*   Input        : u8 proc_type
*                  u32 dst_pid
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
void
nsfw_mgr_close_dst_proc (u8 proc_type, u32 dst_pid)
{
  i32 fd = nsfw_mgr_get_dst_socket (proc_type, dst_pid);
  if (fd > 0)
    {
      (void) nsfw_mgr_del_socket (fd);
      (void) nsfw_mgr_unreg_sock_fun (fd);
      (void) nsfw_base_close (fd);
    }
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_send_msg_socket
*   Description  : send message to dst fd
*   Input        : u32 fd
*                  nsfw_mgr_msg* msg
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_send_msg_socket (u32 fd, nsfw_mgr_msg * msg)
{
  i32 send_len = 0;
  i32 off_set = 0;

  if (NULL == msg)
    {
      NSFW_LOGERR ("msg nul]mgr_fd=%u", fd);
      return FALSE;
    }

  if (msg->msg_len < sizeof (nsfw_mgr_msg))
    {
      msg->msg_len = sizeof (nsfw_mgr_msg);
    }

  if (msg->msg_type == MGR_MSG_LARGE_ALARM_RSP)
    {
      off_set = NSFW_MGR_MSG_HDR_LEN;
    }

  do
    {
      off_set += send_len;
      send_len =
        nsfw_base_send (fd, (char *) msg + off_set, msg->msg_len - off_set,
                        MSG_NOSIGNAL);
      if (send_len <= 0)
        {
          NSFW_LOGERR
            ("send error]mgr_fd=%u,send_len=%d,off_set=%d,errno=%d" MSGINFO,
             fd, send_len, off_set, errno, PRTMSG (msg));
          return FALSE;
        }
    }
  while ((send_len + off_set) < (i32) msg->msg_len);
  NSFW_LOGDBG ("send mgr_msg suc]mgr_fd=%u," MSGINFO, fd, PRTMSG (msg));
  g_mgr_stat.msg_send[msg->msg_type]++;
  return TRUE;
}

#define MAX_RECV_COUNT 100

/*****************************************************************************
*   Prototype    : nsfw_mgr_recv_msg_socket
*   Description  : receive new message from fd
*   Input        : u32 fd
*                  nsfw_mgr_msg* msg
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_recv_msg_socket (u32 fd, nsfw_mgr_msg * msg,
                          nsfw_mgr_msg ** large_msg)
{
  i32 recv_len = 0;
  i32 off_set = 0;
  u32 msg_len = 0;
  i32 max_count = 0;
  if (NULL == msg)
    {
      return FALSE;
    }

  u8 from_flag = msg->from_mem;
  msg_len = msg->msg_len;
  do
    {
      off_set += recv_len;
      recv_len =
        nsfw_base_recv (fd, (char *) msg + off_set, msg_len - off_set, 0);
      if (recv_len <= 0)
        {
          if ((EINTR == errno || EAGAIN == errno)
              && (max_count < MAX_RECV_COUNT))
            {
              recv_len = 0;
              max_count++;
              continue;
            }

          NSFW_LOGERR
            ("recv error]mgr_fd=%u,recv_len=%d,off_set=%d,errno=%d,"
             MSGINFO, fd, recv_len, off_set, errno, PRTMSG (msg));
          msg->from_mem = from_flag;
          return FALSE;
        }
    }
  while (recv_len + off_set < (i32) msg_len);

  msg->from_mem = from_flag;

  g_mgr_stat.msg_recv[msg->msg_type]++;

  if (msg->msg_len <= msg_len)
    {
      NSFW_LOGDBG ("recv mgr_msg suc]mgr_fd=%u," MSGINFO, fd, PRTMSG (msg));
      return TRUE;
    }

  if (large_msg == NULL)
    {
      return TRUE;
    }

  nsfw_mgr_msg *l_msg =
    nsfw_mgr_msg_alloc (msg->msg_type, msg->dst_proc_type);
  if (NULL == l_msg)
    {
      return TRUE;
    }

  if (l_msg->msg_len <= msg_len)
    {
      NSFW_LOGWAR ("alloc new msg error!]len=%u,org_len=%u,type=%u",
                   l_msg->msg_len, msg->msg_len, msg->msg_type);
      nsfw_mgr_msg_free (l_msg);
      return TRUE;
    }

  max_count = 0;
  (void) nsfw_set_sock_block (fd, FALSE);
  from_flag = l_msg->from_mem;
  u32 l_msg_len = l_msg->msg_len;
  do
    {
      off_set += recv_len;
      recv_len =
        nsfw_base_recv (fd, (char *) l_msg + off_set, l_msg_len - off_set, 0);
      if (recv_len <= 0)
        {
          if ((EINTR == errno || EAGAIN == errno)
              && (max_count < MAX_RECV_COUNT))
            {
              recv_len = 0;
              max_count++;
              continue;
            }

          NSFW_LOGERR
            ("recv error]mgr_fd=%u,recv_len=%d,off_set=%d,errno=%d,"
             MSGINFO, fd, recv_len, off_set, errno, PRTMSG (msg));
          l_msg->from_mem = from_flag;
          nsfw_mgr_msg_free (l_msg);
          return FALSE;
        }
    }
  while (recv_len + off_set < (i32) l_msg_len);
  (void) nsfw_set_sock_block (fd, TRUE);
  int retVal = MEMCPY_S (l_msg, msg_len, msg, msg_len);
  if (EOK != retVal)
    {
      NSFW_LOGERR ("MEMCPY_S failed] ret=%d", retVal);
      l_msg->from_mem = from_flag;
      nsfw_mgr_msg_free (l_msg);
      return TRUE;
    }
  l_msg->from_mem = from_flag;
  l_msg->msg_len = l_msg_len;

  *large_msg = l_msg;
  NSFW_LOGDBG ("recv large mgr_msg suc]mgr_fd=%u," MSGINFO, fd,
               PRTMSG (l_msg));
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_send_req_wait_rsp
*   Description  : send request message and block waiting for it's response
                   within MGR_COM_RECV_TIMEOUT
*   Input        : nsfw_mgr_msg* req_msg
*                  nsfw_mgr_msg* rsp_msg
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_send_req_wait_rsp (nsfw_mgr_msg * req_msg, nsfw_mgr_msg * rsp_msg)
{
  if (NULL == req_msg)
    {
      NSFW_LOGERR ("req msg nul!");
      return FALSE;
    }

  i32 dst_socket =
    nsfw_mgr_get_dst_socket (req_msg->dst_proc_type, req_msg->dst_pid);
  if (dst_socket <= 0)
    {
      dst_socket =
        nsfw_mgr_get_new_socket (req_msg->dst_proc_type, req_msg->dst_pid);
      if (dst_socket <= 0)
        {
          NSFW_LOGERR ("send msg get dst_socket_error]" MSGINFO,
                       PRTMSG (req_msg));
          return FALSE;
        }
    }

  if ((NULL == rsp_msg) && (req_msg->msg_len == sizeof (nsfw_mgr_msg)))
    {
      LOCK_MGR_FD (dst_socket)
        if (FALSE == nsfw_mgr_send_msg_socket (dst_socket, req_msg))
        {
          NSFW_LOGERR ("send msg error]" MSGINFO, PRTMSG (req_msg));
          g_mgr_stat.msg_send_failed++;
          UNLOCK_MGR_FD (dst_socket) return FALSE;
        }
      UNLOCK_MGR_FD (dst_socket) return TRUE;
    }

  LOCK_MGR_FD (dst_socket);
  (void) nsfw_mgr_unreg_sock_fun (dst_socket);
  if (FALSE == nsfw_mgr_send_msg_socket (dst_socket, req_msg))
    {
      NSFW_LOGERR ("send msg error]" MSGINFO, PRTMSG (req_msg));
      g_mgr_stat.msg_send_failed++;
      (void) nsfw_mgr_reg_sock_fun (dst_socket, nsfw_mgr_new_msg);
      UNLOCK_MGR_FD (dst_socket);
      return FALSE;
    }

  if (NULL == rsp_msg)
    {
      (void) nsfw_mgr_reg_sock_fun (dst_socket, nsfw_mgr_new_msg);
      UNLOCK_MGR_FD (dst_socket) return TRUE;
    }

  u16 i;
  for (i = 0; i < MGR_COM_MAX_DROP_MSG; i++)
    {
      if (FALSE == nsfw_mgr_recv_msg_socket (dst_socket, rsp_msg, NULL))
        {
          NSFW_LOGERR ("recv msg error]" MSGINFO, PRTMSG (req_msg));
          (void) nsfw_mgr_reg_sock_fun (dst_socket, nsfw_mgr_new_msg);
          UNLOCK_MGR_FD (dst_socket) return FALSE;
        }

      if ((rsp_msg->seq == req_msg->seq)
          && (rsp_msg->msg_type == req_msg->msg_type + MGR_MSG_RSP_BASE))
        {
          break;
        }

      NSFW_LOGINF ("recv msg forward]" MSGINFO, PRTMSG (rsp_msg));
      rsp_msg->fw_flag = TRUE;
      (void) nsfw_mgr_send_msg (rsp_msg);
    }

  (void) nsfw_mgr_reg_sock_fun (dst_socket, nsfw_mgr_new_msg);
  if (0 == req_msg->dst_pid)
    {
      (void) nsfw_mgr_new_socket (dst_socket, rsp_msg->src_proc_type,
                                  rsp_msg->src_pid);
    }
  UNLOCK_MGR_FD (dst_socket) return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_send_msg
*   Description  : send message to peer
*   Input        : nsfw_mgr_msg* msg
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_send_msg (nsfw_mgr_msg * msg)
{
  return nsfw_mgr_send_req_wait_rsp (msg, NULL);
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_msg_in
*   Description  : when new domain socket mgr message receive, this function will be call
*   Input        : i32 fd
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_msg_in (i32 fd)
{
  u32 i = 0;
  u8 ret = FALSE;
  u8 msg_match = FALSE;
  nsfw_mgr_msg *msg = nsfw_mgr_null_rspmsg_alloc ();
  nsfw_mgr_msg *large_msg = NULL;

  LOCK_MGR_FD (fd) ret = nsfw_mgr_recv_msg_socket (fd, msg, &large_msg);
  UNLOCK_MGR_FD (fd) if (large_msg != NULL)
    {
      nsfw_mgr_msg_free (msg);
      msg = large_msg;
    }

  if (FALSE == ret)
    {
      nsfw_mgr_msg_free (msg);
      return FALSE;
    }

  if (msg->fw_flag != TRUE)
    {
      (void) nsfw_mgr_new_socket (fd, msg->src_proc_type, msg->src_pid);
    }

  if (msg->msg_type < MGR_MSG_MAX)
    {
      for (i = 0; i < NSFW_MGRCOM_MAX_PROC_FUN; i++)
        {
          if (NULL == g_mgr_fun[msg->msg_type][i])
            {
              break;
            }

          (void) g_mgr_fun[msg->msg_type][i] (msg);
          msg_match = TRUE;
        }
    }

  if (FALSE != msg_match)
    {
      nsfw_mgr_msg_free (msg);
      return TRUE;
    }

  if (msg->msg_type < MGR_MSG_RSP_BASE)
    {
      NSFW_LOGERR ("msg match failed! auto rsp]" MSGINFO, PRTMSG (msg));
      nsfw_mgr_msg *rsp_msg = nsfw_mgr_rsp_msg_alloc (msg);
      if (NULL != rsp_msg)
        {
          rsp_msg->resp_code = NSFW_MGR_MSG_TYPE_ERROR;
          (void) nsfw_mgr_send_msg (rsp_msg);
          nsfw_mgr_msg_free (rsp_msg);
        }
    }

  NSFW_LOGERR ("drop msg]" MSGINFO, PRTMSG (msg));
  /* fix "Out-of-bounds write" type codex issue */
  if (msg->msg_type < MGR_MSG_MAX)
    {
      g_mgr_stat.recv_drop[msg->msg_type]++;
    }
  nsfw_mgr_msg_free (msg);
  return FALSE;

}

/*****************************************************************************
*   Prototype    : nsfw_mgr_new_msg
*   Description  : when new mgr message receive from socket, this function
                   will call back
*   Input        : i32 epfd
*                  i32 fd
*                  u32 events
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_mgr_new_msg (i32 epfd, i32 fd, u32 events)
{
  lint_lock_1 ();
  if ((events & EPOLLERR) || (events & EPOLLHUP) || (!(events & EPOLLIN)))
    {
      (void) nsfw_mgr_del_socket (fd);
      (void) nsfw_mgr_unreg_sock_fun (fd);
      (void) nsfw_base_close (fd);
      lint_unlock_1 ();
      return TRUE;
    }

  (void) nsfw_mgr_msg_in (fd);
  lint_unlock_1 ();
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_com_rereg_fun
*   Description  : rereg the error socket
*   Input        : u32 timer_type
*                  void* data
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_mgr_com_rereg_fun (u32 timer_type, void *data)
{
  (void) nsfw_mgr_reg_sock_fun (timer_type, (nsfw_mgr_sock_fun) data);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_com_socket_error
*   Description  : remove the error fd from ep
*   Input        : i32 fd
*                  nsfw_mgr_sock_fun fun
*                  i32 timer
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_mgr_com_socket_error (i32 fd, nsfw_mgr_sock_fun fun, i32 timer)
{
  struct timespec time_left = { timer, 0 };
  nsfw_mgr_unreg_sock_fun (fd);
  nsfw_timer_reg_timer (fd, (void *) fun, nsfw_mgr_com_rereg_fun, time_left);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_new_connection
*   Description  : when new mgr connection in, this function will call back
*   Input        : i32 epfd
*                  i32 fd
*                  u32 events
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_mgr_new_connection (i32 epfd, i32 fd, u32 events)
{
  if ((events & EPOLLERR) || (events & EPOLLHUP) || (!(events & EPOLLIN)))
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGWAR ("listen disconnect!]epfd=%d,listen=%d,event=0x%x", epfd,
                   fd, events);
      (void) nsfw_mgr_unreg_sock_fun (fd);
      i32 listen_fd = nsfw_mgr_get_listen_socket ();
      if (listen_fd < 0)
        {
          NSFW_LOGERR
            ("get listen_fd failed!]epfd=%d,listen_fd=%d,event=0x%x", epfd,
             fd, events);
          return FALSE;
        }

      (void) nsfw_mgr_reg_sock_fun (listen_fd, nsfw_mgr_new_connection);
      return TRUE;
    }

  struct sockaddr in_addr;
  socklen_t in_len;
  int infd;
  in_len = sizeof in_addr;

  int size, size_len;
  u8 accept_flag = FALSE;
  while (1)
    {
      infd = nsfw_base_accept (fd, &in_addr, &in_len);
      if (infd == -1)
        {
          if (FALSE == accept_flag)
            {
              nsfw_mgr_com_socket_error (fd, nsfw_mgr_new_connection, 1);
            }
          break;
        }

      if (-1 == nsfw_set_close_on_exec (infd))
        {
          (void) nsfw_base_close (infd);
          NSFW_LOGERR ("set exec err]fd=%d, errno=%d", infd, errno);
          break;
        }

      size = MAX_RECV_BUF_DEF;
      size_len = sizeof (size);
      if (0 >
          nsfw_base_setsockopt (infd, SOL_SOCKET, SO_RCVBUF, (void *) &size,
                                (socklen_t) size_len))
        {
          NSFW_LOGERR ("set socket opt err!]error=%d", errno);
        }

      if (0 >
          nsfw_base_setsockopt (infd, SOL_SOCKET, SO_SNDBUF, (void *) &size,
                                (socklen_t) size_len))
        {
          NSFW_LOGERR ("set socket opt err!]error=%d", errno);
        }

      (void) nsfw_mgr_reg_sock_fun (infd, nsfw_mgr_new_msg);
      NSFW_LOGDBG ("accept_flag new fd]new_mgr_fd=%d", infd);
      accept_flag = TRUE;
    }

  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_set_sock_block
*   Description  : set fd block or not for epoll thread
*   Input        : i32 sock
*                  u8 flag
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_set_sock_block (i32 sock, u8 flag)
{
  i32 flags;
  flags = nsfw_base_fcntl (sock, F_GETFL, 0);
  if (flags < 0)
    {
      NSFW_LOGERR ("fcntl err]new_mgr_fd=%d,errno=%d", sock, errno);
      return -1;
    }

  if (TRUE == flag)
    {
      flags = flags | O_NONBLOCK;
    }
  else
    {
      flags = flags & (~O_NONBLOCK);
      struct timeval tv;
      tv.tv_sec = MGR_COM_RECV_TIMEOUT;
      tv.tv_usec = 0;
      if (nsfw_base_setsockopt
          (sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof tv))
        {
          NSFW_LOGERR ("setsockopt socket err]mgr_fd=%d", sock);
          return -1;
        }
    }

  if (nsfw_base_fcntl (sock, F_SETFL, flags) < 0)
    {
      NSFW_LOGERR ("fcntl err]new_mgr_fd=%d,errno=%d,flags=%d", sock, errno,
                   flags);
      return -1;
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : nsfw_set_close_on_exec
*   Description  : close on exec set
*   Input        : i32 sock
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_set_close_on_exec (i32 sock)
{
  i32 flags;
  flags = nsfw_base_fcntl (sock, F_GETFD, 0);
  if (flags < 0)
    {
      NSFW_LOGERR ("fcntl err]fd=%d,errno=%d", sock, errno);
      return -1;
    }

  flags |= FD_CLOEXEC;

  if (nsfw_base_fcntl (sock, F_SETFD, flags) < 0)
    {
      NSFW_LOGERR ("fcntl err]fd=%d,errno=%d,flags=%d", sock, errno, flags);
      return -1;
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : nsfw_add_sock_to_ep
*   Description  : add fd to epoll wait thread
*   Input        : i32 fd
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_add_sock_to_ep (i32 fd)
{
  struct epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN;
  if (g_ep_proc.epfd == 0)
    {
      return TRUE;
    }

  (void) nsfw_set_sock_block (fd, TRUE);

  if (0 > nsfw_base_epoll_ctl (g_ep_proc.epfd, EPOLL_CTL_ADD, fd, &event))
    {
      NSFW_LOGINF
        ("add sock to ep thread failed]mgr_fd=%d,errno=%d,epfd=%d", fd,
         errno, g_ep_proc.epfd);
      return FALSE;
    }

  NSFW_LOGDBG ("add sock to ep thread]mgr_fd=%d,epfd=%d", fd,
               g_ep_proc.epfd) return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_rmv_sock_from_ep
*   Description  : remove fd from epoll thread
*   Input        : i32 fd
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_rmv_sock_from_ep (i32 fd)
{
  struct epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN;
  if (g_ep_proc.epfd == 0)
    {
      return TRUE;
    }

  (void) nsfw_set_sock_block (fd, FALSE);

  if (0 > nsfw_base_epoll_ctl (g_ep_proc.epfd, EPOLL_CTL_DEL, fd, &event))
    {
      NSFW_LOGINF
        ("rmv sock to ep thread failed] mgr_fd=%d,errno=%d,epfd=%d", fd,
         errno, g_ep_proc.epfd);
      return FALSE;
    }

  NSFW_LOGDBG ("rmv sock to ep thread] mgr_fd=%d,epfd=%d", fd,
               g_ep_proc.epfd) return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_reg_sock_fun
*   Description  : reg fd process function to call back when epoll thread
                   recvice new event of the reg fd
*   Input        : i32 fd
*                  nsfw_mgr_sock_fun fun
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_reg_sock_fun (i32 fd, nsfw_mgr_sock_fun fun)
{
  lint_lock_1 ();
  if ((fd >= (i32) NSFW_MGR_FD_MAX) || (fd < 0) || NULL == fun)
    {
      NSFW_LOGINF ("reg sock fun error!] mgr_fd=%d,fun=%p", fd, fun);
      lint_unlock_1 ();
      return FALSE;
    }

  if ((g_ep_proc.ep_fun) && (NULL == g_ep_proc.ep_fun[fd]))
    {
      g_ep_proc.ep_fun[fd] = fun;
      if (FALSE == nsfw_add_sock_to_ep (fd))
        {
          g_ep_proc.ep_fun[fd] = NULL;
          lint_unlock_1 ();
          return FALSE;
        }

      NSFW_LOGDBG ("reg sock fun] mgr_fd=%d,fun=%p", fd, fun);
      lint_unlock_1 ();
      return TRUE;
    }
  lint_unlock_1 ();
  return FALSE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_unreg_sock_fun
*   Description  : unreg the fd event function
*   Input        : i32 fd
*   Output       : None
*   Return Value : void
*   Calls        :
*   Called By    :
*****************************************************************************/
void
nsfw_mgr_unreg_sock_fun (i32 fd)
{
  lint_lock_1 ();
  if (fd >= (i32) NSFW_MGR_FD_MAX)
    {
      NSFW_LOGINF ("unreg sock fun failed!] mgr_fd=%d", fd);
      lint_unlock_1 ();
      return;
    }

  if ((g_ep_proc.ep_fun) && (NULL != g_ep_proc.ep_fun[fd]))
    {
      g_ep_proc.ep_fun[fd] = NULL;
      (void) nsfw_rmv_sock_from_ep (fd);
      NSFW_LOGDBG ("unreg sock fun] mgr_fd=%d", fd);
      lint_unlock_1 ();
      return;
    }

  lint_unlock_1 ();
  return;
}

/*****************************************************************************
*   Prototype    : nsfw_sock_fun_callback
*   Description  : call back the event process function
*   Input        : i32 epfd
*                  i32 fd
*                  u32 events
*   Output       : None
*   Return Value : static inline u8
*   Calls        :
*   Called By    :
*****************************************************************************/
NSTACK_STATIC inline u8
nsfw_sock_fun_callback (i32 epfd, i32 fd, u32 events)
{
  if ((fd < (i32) NSFW_MGR_FD_MAX)
      && (g_ep_proc.ep_fun) && (NULL != g_ep_proc.ep_fun[fd]))
    {
      (void) g_ep_proc.ep_fun[fd] (epfd, fd, events);
      return TRUE;
    }

  return FALSE;
}

/*****************************************************************************
*   Prototype    : nsfw_sock_add_to_ep
*   Description  : add all event process function has ben reg to the epoll
                   thread when thread start
*   Input        : i32 epfd
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_sock_add_to_ep (i32 epfd)
{
  u32 i;

  for (i = 0; i < NSFW_MGR_FD_MAX; i++)
    {
      if ((g_ep_proc.ep_fun) && (NULL == g_ep_proc.ep_fun[i]))
        {
          continue;
        }
      (void) nsfw_add_sock_to_ep (i);
    }

  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_com_start
*   Description  : start mgr com module
*   Input        : None
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_com_start ()
{
  i32 listen_fd = nsfw_mgr_get_listen_socket ();
  if (listen_fd < 0)
    {
      NSFW_LOGERR ("get listen_fd failed!");
      return FALSE;
    }

  NSFW_LOGINF ("start mgr_com module!] listen_fd=%d", listen_fd);
  (void) nsfw_mgr_reg_sock_fun (listen_fd, nsfw_mgr_new_connection);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_com_start_local
*   Description  : start_local_msg_com
*   Input        : None
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_com_start_local (u8 proc_type)
{
  int fd[2];
  if ((socketpair (AF_UNIX, SOCK_STREAM, 0, fd)) < 0)
    {
      NSFW_LOGERR ("create socket err] type=%u,errno=%d", proc_type, errno);
      return FALSE;
    }

  (void) nsfw_mgr_new_socket (fd[0], proc_type, get_sys_pid ());
  (void) nsfw_mgr_new_socket (fd[1], proc_type, get_sys_pid ());
  (void) nsfw_mgr_reg_sock_fun (fd[0], nsfw_mgr_new_msg);
  (void) nsfw_mgr_reg_sock_fun (fd[1], nsfw_mgr_new_msg);
  NSFW_LOGINF ("create local socket] fd0=%d,fd1=%d", fd[0], fd[1]);
  return TRUE;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_listen_thread
*   Description  : epoll thread function
*   Input        : void* arg
*   Output       : None
*   Return Value : void *
*   Calls        :
*   Called By    :
*****************************************************************************/
void *
nsfw_mgr_listen_thread (void *arg)
{
  i32 epfd = 0;
  //i32 listen_socket = 0;

  lint_lock_1 ();
#define MAXEVENTS 10
  epfd = nsfw_base_epoll_create (10);

  struct epoll_event events[MAXEVENTS];
  if (EOK != MEMSET_S (events, sizeof (events), 0, sizeof (events)))
    {
      NSFW_LOGERR ("MEMSET_S failed!]epfd=%d", epfd);
      lint_unlock_1 ();
      return NULL;
    }

  g_ep_proc.epfd = epfd;
  g_ep_proc.hbt_count = 0;
  (void) nsfw_sock_add_to_ep (epfd);
  lint_unlock_1 ();
  while (1)
    {
      lint_lock_1 ();
      int n, i;
      n = nsfw_base_epoll_wait (epfd, events, MAXEVENTS, -1);
      for (i = 0; i < n; i++)
        {
          if (TRUE ==
              nsfw_sock_fun_callback (epfd, events[i].data.fd,
                                      events[i].events))
            {
              g_ep_proc.hbt_count = 0;
              continue;
            }

          NSFW_LOGERR ("error event recv] fd=%d,event=%d",
                       events[i].data.fd, events[i].events);
        }
      lint_unlock_1 ();
    }

}

NSTACK_STATIC inline void
get_thread_policy (pthread_attr_t * attr)
{
  int policy;
  int rs = pthread_attr_getschedpolicy (attr, &policy);
  if (rs != 0)
    {
      NSFW_LOGERR ("pthread_attr_getschedpolicy failed");
      return;
    }
  switch (policy)
    {
    case SCHED_FIFO:
      NSFW_LOGINF ("policy= SCHED_FIFO");
      break;
    case SCHED_RR:
      NSFW_LOGINF ("policy= SCHED_RR");
      break;
    case SCHED_OTHER:
      NSFW_LOGINF ("policy=SCHED_OTHER");
      break;
    default:
      NSFW_LOGINF ("policy=UNKNOWN");
      break;
    }

  return;
}

NSTACK_STATIC inline void
get_thread_priority (pthread_attr_t * attr)
{
  struct sched_param param;
  int rs = pthread_attr_getschedparam (attr, &param);
  if (rs != 0)
    {
      NSFW_LOGERR ("pthread_attr_getschedparam failed");
      return;
    }

  NSFW_LOGINF ("get thread priority] pri=%d", param.sched_priority);
}

/* support thread priority configuration */
void
set_thread_attr (pthread_attr_t * pattr, int stacksize, int pri, int policy)
{
  struct sched_param param;
  (void) pthread_attr_init (pattr);

  if (stacksize > 0)
    {
      (void) pthread_attr_setstacksize (pattr, stacksize);
    }

  param.sched_priority = pri;
  if (SCHED_OTHER != policy)
    {
      (void) pthread_attr_setschedpolicy (pattr, policy);
      (void) pthread_attr_setschedparam (pattr, &param);
      (void) pthread_attr_setinheritsched (pattr, PTHREAD_EXPLICIT_SCHED);
    }
  get_thread_policy (pattr);
  get_thread_priority (pattr);
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_ep_start
*   Description  : start epoll thread
*   Input        : None
*   Output       : None
*   Return Value : u8
*   Calls        :
*   Called By    :
*****************************************************************************/
u8
nsfw_mgr_ep_start ()
{
  /* heart beat thread should have the same priority with the tcpip thread */
  pthread_attr_t attr;
  pthread_attr_t *pattr = NULL;

  if (g_thread_policy != SCHED_OTHER)
    {
      set_thread_attr (&attr, 0, g_thread_pri, g_thread_policy);
      pattr = &attr;
    }

  if (pthread_create
      (&g_ep_proc.ep_thread, pattr, nsfw_mgr_listen_thread, NULL))
    {
      return FALSE;
    }

  NSFW_LOGINF ("start thread] id=%d", g_ep_proc.ep_thread);

  if (pthread_setname_np (g_ep_proc.ep_thread, NSFW_MGRCOM_THREAD))
    {
      return TRUE;
    }
  (void) nsfw_reg_trace_thread (g_ep_proc.ep_thread);
  return TRUE;
}

int
nsfw_mgr_com_chk_hbt (int v_add)
{
  int ret = g_ep_proc.hbt_count;
  g_ep_proc.hbt_count += v_add;
  return ret;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_comm_fd_destroy
*   Description  : free the memory
*   Input        :
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
void
nsfw_mgr_comm_fd_destroy ()
{
  if (g_ep_proc.ep_fun)
    {
      free (g_ep_proc.ep_fun);
      g_ep_proc.ep_fun = NULL;
    }
  if (g_mgr_socket_map.sock)
    {
      free (g_mgr_socket_map.sock);
      g_mgr_socket_map.sock = NULL;
    }
  return;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_comm_fd_init
*   Description  : fd map and socket info init
*   Input        : u32 proc_type
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nsfw_mgr_comm_fd_init (u32 proc_type)
{
  /*only app need to do this */
  if ((g_mgr_socket_map.sock) && (g_ep_proc.ep_fun))
    {
      return 0;
    }
  if (NSFW_PROC_APP == proc_type)
    {
      long sysfdmax = 0;
      sysfdmax = sysconf (_SC_OPEN_MAX);
      NSFW_LOGINF ("] sys max open files=%d", sysfdmax);
      if (sysfdmax > 0)
        {
          NSFW_MGR_FD_MAX =
            (int) ((sysfdmax <=
                    NSFW_MGRCOM_MAX_SOCKET *
                    60) ? sysfdmax : NSFW_MGRCOM_MAX_SOCKET * 60);
        }
      else
        {
          NSFW_LOGERR ("get sys max open file fail");
          NSFW_MGR_FD_MAX = NSFW_MGRCOM_MAX_SOCKET;
        }
    }
  NSFW_LOGINF ("] final max fd=%d", NSFW_MGR_FD_MAX);
  if (!g_mgr_socket_map.sock)
    {
      g_mgr_socket_map.sock =
        (nsfw_mgr_sock_info *) malloc (sizeof (nsfw_mgr_sock_info) *
                                       NSFW_MGR_FD_MAX);
      if (NULL == g_mgr_socket_map.sock)
        {
          NSFW_LOGERR ("malloc fail] length=%d",
                       sizeof (nsfw_mgr_sock_info) * NSFW_MGR_FD_MAX);
          return -1;
        }
      (void) MEMSET_S (g_mgr_socket_map.sock,
                       sizeof (nsfw_mgr_sock_info) * NSFW_MGR_FD_MAX, 0,
                       sizeof (nsfw_mgr_sock_info) * NSFW_MGR_FD_MAX);
    }
  if (!g_ep_proc.ep_fun)
    {
      g_ep_proc.ep_fun =
        (nsfw_mgr_sock_fun *) malloc (sizeof (nsfw_mgr_sock_fun) *
                                      NSFW_MGR_FD_MAX);
      if (NULL == g_ep_proc.ep_fun)
        {
          NSFW_LOGERR ("malloc fail] length=%d ",
                       sizeof (nsfw_mgr_sock_fun) * NSFW_MGR_FD_MAX);
          return -1;
        }
      (void) MEMSET_S (g_ep_proc.ep_fun,
                       sizeof (nsfw_mgr_sock_fun) * NSFW_MGR_FD_MAX, 0,
                       sizeof (nsfw_mgr_sock_fun) * NSFW_MGR_FD_MAX);
    }
  return 0;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_com_module_init
*   Description  : module init
*   Input        : void* param
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int nsfw_mgr_com_module_init (void *param);
int
nsfw_mgr_com_module_init (void *param)
{
  lint_lock_1 ();
  u32 proc_type = (u32) ((long long) param);
  nsfw_mgr_init_cfg *mgr_cfg = &g_mgr_com_cfg;
  const char *directory = NSFW_DOMAIN_DIR;
  const char *home_dir = getenv ("HOME");

  NSFW_LOGINF ("module mgr init] type=%u", proc_type);

  if (getuid () != 0 && home_dir != NULL)
    directory = home_dir;

  if (0 != nsfw_mgr_comm_fd_init (proc_type))
    {
      NSFW_LOGERR ("fd init fail] proc_type=%u", proc_type);
      lint_unlock_1 ();
      return -1;
    }

  switch (proc_type)
    {
    case NSFW_PROC_MAIN:
      /* modify destMax, remove "-1" */
      if (EOK !=
          STRCPY_S (mgr_cfg->domain_path, NSFW_MGRCOM_PATH_LEN, directory))
        {
          NSFW_LOGERR ("module mgr init STRCPY_S failed!");
          lint_unlock_1 ();
          return -1;
        }

      /* modify destMax, remove "-1" */
      if (EOK !=
          STRCAT_S (mgr_cfg->domain_path, NSFW_MGRCOM_PATH_LEN,
                    NSFW_MAIN_FILE))
        {
          NSFW_LOGERR ("module mgr init STRCAT_S failed!");
          lint_unlock_1 ();
          return -1;
        }

      //TODO: check the path exist or not

      NSFW_LOGINF ("module mgr init]NSFW_PROC_MAIN domain_path=%s",
                   mgr_cfg->domain_path);

      if (TRUE != nsfw_mgr_com_start ())
        {
          NSFW_LOGERR ("module mgr nsfw_mgr_com_start failed!");
          lint_unlock_1 ();
          return -1;
        }

      break;
    case NSFW_PROC_MASTER:
      /* modify destMax, remove "-1" */
      if (EOK !=
          STRCPY_S (mgr_cfg->domain_path, NSFW_MGRCOM_PATH_LEN, directory))
        {
          NSFW_LOGERR ("module mgr init STRCPY_S failed!");
          lint_unlock_1 ();
          return -1;
        }

      /* modify destMax, remove "-1" */
      if (EOK !=
          STRCAT_S (mgr_cfg->domain_path, NSFW_MGRCOM_PATH_LEN,
                    NSFW_MASTER_FILE))
        {
          NSFW_LOGERR ("module mgr init STRCAT_S failed!");
          lint_unlock_1 ();
          return -1;
        }

      NSFW_LOGINF ("module mgr init]NSFW_PROC_MASTER domain_path=%s",
                   mgr_cfg->domain_path);

      if (TRUE != nsfw_mgr_com_start ())
        {
          NSFW_LOGERR ("module mgr nsfw_mgr_com_start failed!");
          lint_unlock_1 ();
          return -1;
        }

      break;
    case NSFW_PROC_TOOLS:
      break;
    case NSFW_PROC_CTRL:
      if (TRUE != nsfw_mgr_com_start_local (proc_type))
        {
          NSFW_LOGERR ("module mgr nsfw_mgr_com_start_local failed!");
          lint_unlock_1 ();
          return -1;
        }
      break;
    default:
      if (proc_type < NSFW_PROC_MAX)
        {
          break;
        }
      lint_unlock_1 ();
      return -1;
    }

  mgr_cfg->msg_size = MGR_COM_MSG_COUNT_DEF;
  mgr_cfg->max_recv_timeout = MGR_COM_RECV_TIMEOUT_DEF;
  mgr_cfg->max_recv_drop_msg = MGR_COM_MAX_DROP_MSG_DEF;

  mgr_cfg->proc_type = proc_type;

  nsfw_mem_sppool pmpinfo;
  if (EOK != MEMSET_S (&pmpinfo, sizeof (pmpinfo), 0, sizeof (pmpinfo)))
    {
      NSFW_LOGERR ("Error to memset!!!");
      nsfw_mgr_comm_fd_destroy ();
      lint_unlock_1 ();
      return -1;
    }

  pmpinfo.enmptype = NSFW_MRING_MPMC;
  pmpinfo.usnum = mgr_cfg->msg_size;
  pmpinfo.useltsize = sizeof (nsfw_mgr_msg);
  pmpinfo.isocket_id = NSFW_SOCKET_ANY;
  pmpinfo.stname.entype = NSFW_NSHMEM;
  if (-1 ==
      SPRINTF_S (pmpinfo.stname.aname, sizeof (pmpinfo.stname.aname), "%s",
                 "MS_MGR_MSGPOOL"))
    {
      NSFW_LOGERR ("Error to SPRINTF_S!!!");
      nsfw_mgr_comm_fd_destroy ();
      lint_unlock_1 ();
      return -1;
    }

  mgr_cfg->msg_pool = nsfw_mem_sp_create (&pmpinfo);

  if (!mgr_cfg->msg_pool)
    {
      NSFW_LOGERR ("module mgr init msg_pool alloc failed!");
      nsfw_mgr_comm_fd_destroy ();
      lint_unlock_1 ();
      return -1;
    }

  (void) MEM_STAT (NSFW_MGR_COM_MODULE, pmpinfo.stname.aname, NSFW_NSHMEM,
                   nsfw_mem_get_len (mgr_cfg->msg_pool, NSFW_MEM_SPOOL));

  if ((NSFW_PROC_TOOLS == proc_type)
      || (NSFW_PROC_CTRL == proc_type) || (NSFW_PROC_MAIN == proc_type))
    {
      if (TRUE != nsfw_mgr_ep_start ())
        {
          NSFW_LOGERR ("module mgr nsfw_mgr_ep_start failed!");
          nsfw_mgr_comm_fd_destroy ();
          lint_unlock_1 ();
          return -1;
        }
    }
  lint_unlock_1 ();
  return 0;
}

/*****************************************************************************
*   Prototype    : nsfw_mgr_run_script
*   Description  : run a linux shell script
*   Input        : const char *cmd, char *result, int result_buf_len
*   Output       : result length
*   Return Value : int
*   Calls        :
*   Called By    :
 *****************************************************************************/
int
nsfw_mgr_run_script (const char *cmd, char *result, int result_buf_len)
{
  if (!cmd || !result || result_buf_len <= 1)
    {
      return -1;
    }

  FILE *fp = popen (cmd, "r");
  if (fp != NULL)
    {
      size_t n = fread (result, sizeof (char), result_buf_len - 1, fp);
      if (n == 0)
        {
          result[0] = '\0';
        }
      else if ('\n' == result[n - 1])
        {
          result[n - 1] = '\0';
        }
      else
        {
          result[n] = '\0';
        }

      pclose (fp);
      return n;
    }

  return -1;
}

/* *INDENT-OFF* */
NSFW_MODULE_NAME(NSFW_MGR_COM_MODULE)
NSFW_MODULE_PRIORITY(10)
NSFW_MODULE_DEPENDS(NSFW_MEM_MGR_MODULE)
NSFW_MODULE_INIT(nsfw_mgr_com_module_init)
/* *INDENT-ON* */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
