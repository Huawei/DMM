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
#ifndef _NSFW_MGRCOM_API_H
#define _NSFW_MGRCOM_API_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSFW_MGR_COM_MODULE "nsfw_mgr_com"

#define MRG_RSP(_req_msg) (_req_msg + MGR_MSG_RSP_BASE)

typedef enum _mgr_msg_type
{
  MGR_MSG_NULL = 0,
  /*#############common msg type################# */
  MGR_MSG_CHK_INIT_REQ = 1,
  MGR_MSG_INIT_NTY_REQ,
  MGR_MSG_CHK_HBT_REQ,
  MGR_MSG_APP_EXIT_REQ,
  MGR_MSG_SRV_CTL_REQ,
  MGR_MSG_VER_MGR_REQ,
  MGR_MSG_SOF_PAR_REQ,

  /*############################################# */
  MGR_MSG_MEM_ALLOC_REQ = 64,   /* memory msg type */

  /*############################################# */
  MGR_MSG_DFX_QRY_REQ = 96,     /* nStackCtrl maintain msg */
  MGR_MSG_SET_LOG_REQ,

  /*############################################# */
  MGR_MSG_RCC_END_REQ = 128,    /* service msg type */

  /*############################################# */
  MGR_MSG_TOOL_TCPDUMP_REQ = 256,       /* for tools */
  MGR_MSG_TOOL_HEART_BEAT,

  /*###query message with large rsp message begin## */
  MGR_MSG_LARGE_QRY_REQ_BEGIN = 384,
  MGR_MSG_LARGE_STA_QRY_REQ = MGR_MSG_LARGE_QRY_REQ_BEGIN,
  MGR_MSG_LARGE_MT_QRY_REQ,     /* nStackCtrl maintain msg */

  /*############################################# */
  MGR_MSG_LARGE_ALARM_REQ = 500,        /* alarm msg type */

  MGR_MSG_RSP_BASE = 512,
  /*#############common msg type################# */
  MGR_MSG_CHK_INIT_RSP = MRG_RSP (MGR_MSG_CHK_INIT_REQ),
  MGR_MSG_INIT_NTY_RSP = MRG_RSP (MGR_MSG_INIT_NTY_REQ),
  MGR_MSG_CHK_HBT_RSP = MRG_RSP (MGR_MSG_CHK_HBT_REQ),
  MGR_MSG_APP_EXIT_RSP = MRG_RSP (MGR_MSG_APP_EXIT_REQ),
  MGR_MSG_SRV_CTL_RSP = MRG_RSP (MGR_MSG_SRV_CTL_REQ),
  MGR_MSG_VER_MGR_RSP = MRG_RSP (MGR_MSG_VER_MGR_REQ),
  MGR_MSG_SOF_PAR_RSP = MRG_RSP (MGR_MSG_SOF_PAR_REQ),
  /*############################################# */

  MGR_MSG_MEM_ALLOC_RSP = MRG_RSP (MGR_MSG_MEM_ALLOC_REQ),

  MGR_MSG_DFX_QRY_RSP = MRG_RSP (MGR_MSG_DFX_QRY_REQ),

  MGR_MSG_SET_LOG_RSP = MRG_RSP (MGR_MSG_SET_LOG_REQ),

  MGR_MSG_RCC_END_RSP = MRG_RSP (MGR_MSG_RCC_END_REQ),

  /*############################################# */
  MGR_MSG_TOOL_TCPDUMP_RSP = MRG_RSP (MGR_MSG_TOOL_TCPDUMP_REQ),
  MGR_MSG_TOOL_HEART_BEAT_RSP = MRG_RSP (MGR_MSG_TOOL_HEART_BEAT),

  /*##############LARGE RSP MESSAGE################## */
  MGR_MSG_LAG_QRY_RSP_BEGIN = MRG_RSP (MGR_MSG_LARGE_QRY_REQ_BEGIN),
  MGR_MSG_LAG_STA_QRY_RSP = MRG_RSP (MGR_MSG_LARGE_STA_QRY_REQ),
  MGR_MSG_LAG_MT_QRY_RSP = MRG_RSP (MGR_MSG_LARGE_MT_QRY_REQ),
  MGR_MSG_LARGE_ALARM_RSP = MRG_RSP (MGR_MSG_LARGE_ALARM_REQ),
  MGR_MSG_MAX = 1024
} mgr_msg_type;

typedef enum _fw_poc_type
{
  NSFW_PROC_NULL = 0,
  NSFW_PROC_MAIN,
  NSFW_PROC_MASTER,
  NSFW_PROC_APP,
  NSFW_PROC_CTRL,
  NSFW_PROC_TOOLS,
  NSFW_PROC_ALARM,
  NSFW_PROC_MAX = 16
} fw_poc_type;

#define NSFW_DOMAIN_DIR "/var/run/"
#define NSTACK_MAX_PROC_NAME_LEN 20

typedef enum _nsfw_mgr_msg_rsp_code
{
  NSFW_MGR_SUCCESS,
  NSFW_MGR_MSG_TYPE_ERROR,
} mgr_msg_rsp_code;

extern char *nsfw_get_proc_name (u8 proc_type);

#define GET_USER_MSG(_stu, _msg) ((_stu *)(&(_msg)->msg_body[0]))

/*for log print*/
#define MSGINFO    "msg=%p,len=%d,t=%d,sq=%d,st=%d,sp=%d,dt=%d,dp=%d"
#define PRTMSG(msg) (msg), (msg)->msg_len,(msg)->msg_type,(msg)->seq, (msg)->src_proc_type,(msg)->src_pid,(msg)->dst_proc_type,(msg)->dst_pid

#define NSFW_MGR_MSG_LEN 512
#define NSFW_MGR_MSG_HDR_LEN 32
#define NSFW_MGR_MSG_BODY_LEN (NSFW_MGR_MSG_LEN - NSFW_MGR_MSG_HDR_LEN)

#define NSFW_MGR_LARGE_MSG_LEN (256*1024)
#define NSFW_MGR_LARGE_MSG_BODY_LEN (NSFW_MGR_LARGE_MSG_LEN - NSFW_MGR_MSG_HDR_LEN)

typedef struct _nsfw_mgr_msg
{
  u16 msg_type;                 /* mgr_msg_type */
  u16 u16Reserve;
  u32 msg_len;

  u8 alloc_flag:1;
  u8 fw_flag:1;
  u8 from_mem:1;
  u8 more_msg_flag:1;
  u8 reserve_flag:4;
  u8 resp_code;
  u8 src_proc_type;             /* fw_poc_type */
  u8 dst_proc_type;
  i32 seq;

  u32 src_pid;
  u32 dst_pid;

  u8 msg_body[NSFW_MGR_MSG_BODY_LEN];
} nsfw_mgr_msg;

extern nsfw_mgr_msg *nsfw_mgr_msg_alloc (u16 msg_type, u8 dst_proc_type);
extern void nsfw_mgr_msg_free (nsfw_mgr_msg * msg);

/* for rsp msg alloc*/
extern nsfw_mgr_msg *nsfw_mgr_null_rspmsg_alloc ();
extern nsfw_mgr_msg *nsfw_mgr_rsp_msg_alloc (nsfw_mgr_msg * req_msg);

/* for msg proc fun reg*/
typedef int (*nsfw_mgr_msg_fun) (nsfw_mgr_msg * msg);
extern u8 nsfw_mgr_reg_msg_fun (u16 msg_type, nsfw_mgr_msg_fun fun);

extern u8 nsfw_mgr_send_msg (nsfw_mgr_msg * msg);
extern u8 nsfw_mgr_send_req_wait_rsp (nsfw_mgr_msg * req_msg,
                                      nsfw_mgr_msg * rsp_msg);

/* for fork clear parent resource*/
extern void nsfw_mgr_close_dst_proc (u8 proc_type, u32 dst_pid);
extern u8 nsfw_mgr_clr_fd_lock ();

/* for epoll thread reg other sock proc fun*/
typedef int (*nsfw_mgr_sock_fun) (i32 epfd, i32 socket, u32 events);
extern u8 nsfw_mgr_reg_sock_fun (i32 socket, nsfw_mgr_sock_fun fun);
extern void nsfw_mgr_unreg_sock_fun (i32 socket);
extern int nsfw_mgr_com_socket_error (i32 fd, nsfw_mgr_sock_fun fun,
                                      i32 timer);
extern u8 nsfw_mgr_ep_start ();
extern int nsfw_mgr_com_module_init (void *param);
extern int nsfw_mgr_run_script (const char *cmd, char *result,
                                int result_buf_len);

extern int nsfw_mgr_com_chk_hbt (int v_add);
extern i32 nsfw_set_close_on_exec (i32 sock);
extern int nsfw_mgr_comm_fd_init (u32 proc_type);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* _NSFW_MGRCOM_API_H  */
