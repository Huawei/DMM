#ifndef __QUEUE_H__
#define __QUEUE_H__
#ifdef __cplusplus
extern "C"
{

#endif
#include "common_mem_base_type.h"
#ifdef HAL_LIB
#else
#include "rte_ring.h"
#endif
#include "nsfw_mem_api.h"
//#include "stackx_dfx_api.h"

#include "sys.h"

  typedef struct queue
  {
    PRIMARY_ADDR mring_handle llring;
  } queue_t;

  err_t queue_push (queue_t * q, void *msg, int isTrypush);
  void *queue_pop (queue_t * q, u32_t * timeout, int isTryPop);

#ifdef __cplusplus

}
#endif
#endif
