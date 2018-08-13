#include <pthread.h>
#include "lwip/memp.h"
#include "lwip/mem.h"

#include "arch/queue.h"
#include "lwip/sys.h"
#include "nstack_log.h"
#include "lwip/sockets.h"
#include "stackx_spl_share.h"
#include "lwip/api.h"
#include "lwip/ip.h"
#include <netif/sc_dpdk.h>

err_t
queue_push (queue_t * q, void *pmsg, int isTrypush)
{
  int pushRet;
  // struct tcpip_msg* _msg = pmsg;

  //enum tcpip_msg_type tcpip_type = TCPIP_MSG_MAX;
  //struct new_api_msg* lo_newapimsg = NULL;
  // enum api_msg_type api_type = NEW_MSG_API_MAX;
  // struct netconn *lo_conn = NULL;
  //u64_t tcp_tx_bytes = 0;
  while (1)
    {
      pushRet = nsfw_mem_ring_enqueue (q->llring, pmsg);

      switch (pushRet)
        {

        case -1:
          NSSOC_LOGERR ("Box range check has failed]llring_a=%p, msg=%p",
                        q->llring, pmsg);
          return ERR_MEM;

        case 1:
          return ERR_OK;

        default:
          if (isTrypush)
            {
              return ERR_MEM;
            }
          else
            {
              continue;
            }
        }
    }
}

void *
queue_pop (queue_t * q, u32_t * timeout, int isTryPop)
{
  void *pmsg;
  int popRet;
  int retVal;
  u32_t ellapsetime = 0;
//    struct tcpip_msg* _msg;
  struct timespec starttm = { 0 };
  struct timespec endtm;
  u32_t timeDiff;
  if (*timeout != 0)
    {
      retVal = clock_gettime (CLOCK_MONOTONIC, &starttm);
      if (0 != retVal)
        {
          NSPOL_LOGERR ("clock_gettime() failed]");
          *timeout = SYS_ARCH_TIMEOUT;
          return NULL;
        }
    }

  while (1)
    {
      //TODO:May have to consider, take out the data sharing problem
      popRet = nsfw_mem_ring_dequeue (q->llring, &pmsg);

      switch (popRet)
        {
        case 1:
          *timeout = ellapsetime;
          return pmsg;

        default:

          if (isTryPop)
            {
              *timeout = SYS_ARCH_TIMEOUT;
              return NULL;
            }
          else
            {
              if (*timeout != 0)
                {
                  retVal = clock_gettime (CLOCK_MONOTONIC, &endtm);
                  if (0 != retVal)
                    {
                      NSPOL_LOGERR ("clock_gettime() failed]");
                      *timeout = SYS_ARCH_TIMEOUT;
                      return NULL;
                    }

                  timeDiff = endtm.tv_sec - starttm.tv_sec;
                  if (timeDiff > *timeout)
                    {
                      *timeout = SYS_ARCH_TIMEOUT;
                      return NULL;
                    }
                  sys_sleep_ns (0, 100000);
                }

              continue;
            }

        }
    }

}
