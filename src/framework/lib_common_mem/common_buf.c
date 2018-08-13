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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>
#include <sys/queue.h>

#include "common_mem_base_type.h"

#include "common_mem_common.h"

#include "common_mem_memzone.h"

#include "common_mem_pal.h"

#include "common_mem_mempool.h"
#include "common_mem_buf.h"

#include "nstack_log.h"
#include "nstack_securec.h"

#include "common_func.h"
#include "common_pal_bitwide_adjust.h"

#define LOG_ERR   0
#define LOG_WARN  1
#define LOG_INFO  2
#define LOG_DEBUG 3
#define LOG_MAX   4

#define COMMON_LOG_PRINT(level, fmt, args...) \
        if (level <= log_level) NSCOMM_LOGERR("===>[COMMON]"fmt, ##args); \

#define COMMON_PANIC(fmt) \
        NSCOMM_LOGERR("==>[COMMON_PANIC]"fmt); \
        common_dump_stack();  \

#define PARA1_SET(argv, tempargv, Index, para1) do {\
    retVal = STRCPY_S(tempargv[Index], PATA_STRLENT, para1);\
    if (retVal != EOK)\
    {\
        NSCOMM_LOGERR("STRCPY_S failed]ret=%d", retVal);\
        return DMM_MBUF_RET_ERR;\
    }\
    argv[Index] = tempargv[Index]; \
    Index ++; } while (0)

#define PARA2_SET(argv, tempargv, Index, para1, para2) do {\
    retVal = STRCPY_S(tempargv[Index], PATA_STRLENT, para1); \
    if (retVal != EOK)\
    {\
        NSCOMM_LOGERR("STRCPY_S failed]ret=%d", retVal);\
        return DMM_MBUF_RET_ERR;\
    }\
    argv[Index] = tempargv[Index]; \
    Index++; \
    retVal = STRCPY_S(tempargv[Index], PATA_STRLENT, para2); \
    if (retVal != EOK)\
    {\
        NSCOMM_LOGERR("STRCPY_S failed]ret=%d", retVal);\
        return DMM_MBUF_RET_ERR;\
    }\
    argv[Index] = tempargv[Index]; \
    Index ++; } while (0)

#define PATA_STRLENT     64
#define PATA_NUM_MAX     12

int log_level = LOG_INFO;

int
nscomm_pal_module_init (common_mem_pal_module_info * pinfo)
{
  char tempargv[PATA_NUM_MAX][PATA_STRLENT];
  char *argv[PATA_NUM_MAX];
  char tempbuf[PATA_STRLENT];
  unsigned int Index = 0;
  int ioffset = 0;
  int agindex = 0;
  int intmask = 0;
  int retVal;
  retVal = MEMSET_S (tempargv, sizeof (tempargv), '\0', sizeof (tempargv));
  if (EOK != retVal)
    {
      NSCOMM_LOGERR ("MEMSET_S failed]ret=%d", retVal);
      return DMM_MBUF_RET_ERR;
    }
  retVal = MEMSET_S (argv, sizeof (argv), 0, sizeof (argv));
  if (EOK != retVal)
    {
      NSCOMM_LOGERR ("MEMSET_S failed]ret=%d", retVal);
      return DMM_MBUF_RET_ERR;
    }
  if (NULL == pinfo)
    {
      PARA1_SET (argv, tempargv, agindex, "nStackMain");
      PARA2_SET (argv, tempargv, agindex, "-c", "0x1");
      PARA2_SET (argv, tempargv, agindex, "-n", "4");
      PARA2_SET (argv, tempargv, agindex, "-m", "2048");
      PARA1_SET (argv, tempargv, agindex, "--huge-dir=/mnt/nstackhuge");
      PARA1_SET (argv, tempargv, agindex, "--proc-type=primary");
    }
  else
    {
      PARA1_SET (argv, tempargv, agindex, "nStackMain");

      retVal = SPRINTF_S (tempbuf, PATA_STRLENT, "0x");
      if (-1 == retVal)
        {
          NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", ioffset);
          return DMM_MBUF_RET_ERR;
        }
      ioffset = retVal;
      for (Index = 0; Index < LCORE_MASK_MAX; Index++)
        {
          if (ioffset >= PATA_STRLENT)
            {
              NSCOMM_LOGERR ("SPRINTF_S tempbuf overflow]ioffset=%d",
                             ioffset);
              return DMM_MBUF_RET_ERR;
            }
          retVal =
            SPRINTF_S (&(tempbuf[ioffset]), PATA_STRLENT - ioffset, "%8u",
                       pinfo->ilcoremask[Index]);
          if (-1 == retVal)
            {
              NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", ioffset);
              return DMM_MBUF_RET_ERR;
            }
          ioffset = ioffset + retVal;
          intmask |= pinfo->ilcoremask[Index];
        }
      if (0 == intmask)
        {
          PARA2_SET (argv, tempargv, agindex, "-c", "0x1");
        }
      else
        {
          PARA2_SET (argv, tempargv, agindex, "-c", tempbuf);
        }
      if (pinfo->ishare_mem_size > 0)
        {
          retVal = MEMSET_S (tempbuf, PATA_STRLENT, 0, PATA_NUM_MAX);
          if (EOK != retVal)
            {
              NSCOMM_LOGERR ("MEMSET_S failed]ret=%d", retVal);
              return DMM_MBUF_RET_ERR;
            }
          retVal =
            SPRINTF_S (tempbuf, PATA_STRLENT, "%d", pinfo->ishare_mem_size);
          if (-1 == retVal)
            {
              NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
              return DMM_MBUF_RET_ERR;
            }
          PARA2_SET (argv, tempargv, agindex, "-m", tempbuf);
        }

      retVal = MEMSET_S (tempbuf, PATA_STRLENT, 0, PATA_NUM_MAX);
      if (EOK != retVal)
        {
          NSCOMM_LOGERR ("MEMSET_S failed]ret=%d", retVal);
          return DMM_MBUF_RET_ERR;
        }

      switch (pinfo->ucproctype)
        {
        case DMM_PROC_T_PRIMARY:
          retVal = SPRINTF_S (tempbuf, PATA_STRLENT, "--proc-type=primary");
          if (-1 == retVal)
            {
              NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
              return DMM_MBUF_RET_ERR;
            }
          break;
        case DMM_PROC_T_SECONDARY:
          retVal = SPRINTF_S (tempbuf, PATA_STRLENT, "--proc-type=secondary");
          if (-1 == retVal)
            {
              NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
              return DMM_MBUF_RET_ERR;
            }
          break;
        case DMM_PROC_T_AUTO:
        default:
          retVal = SPRINTF_S (tempbuf, PATA_STRLENT, "--proc-type=auto");
          if (-1 == retVal)
            {
              NSCOMM_LOGERR ("SPRINTF_S failed]ret=%d", retVal);
              return DMM_MBUF_RET_ERR;
            }
          break;
        }
      PARA1_SET (argv, tempargv, agindex, tempbuf);

      if (DMM_HUGTBL_DISABLE == pinfo->uchugeflag)
        {
          PARA1_SET (argv, tempargv, agindex, "--no-huge");
        }
    }
  if (common_mem_pal_init (agindex, argv) < 0)
    {
      COMMON_LOG_PRINT (LOG_ERR, "Cannot init pal\r\n");
      return DMM_MBUF_RET_ERR;
    }
  return DMM_MBUF_RET_OK;
}

void *
nscomm_memzone_data_reserve_name (const char *name, size_t len, int socket_id)
{
  const struct common_mem_memzone *mz = NULL;
  /*
     rte_memzone_reserve must Call first,  cause rte_memzone_reserve has a globe lock.
     while proc race happen, who(calls A) got lock first will create memzone success.
     others create same memzone proc will got lock after A, and rte_memzone_reserve return NULL;
     so while rte_memzone_reserve return NULL we need do rte_memzone_lookup;
   */
  mz = common_mem_memzone_reserve (name, len, socket_id, 0);
  if (mz == NULL)
    {
      mz = common_mem_memzone_lookup (name);
    }

  return mz ? (void *) ADDR_SHTOL (mz->addr_64) : NULL;
}

void *
nscomm_memzone_data_lookup_name (const char *name)
{
  void *addr = NULL;
  const struct common_mem_memzone *mz = NULL;
  mz = common_mem_memzone_lookup (name);
  if (mz == NULL)
    {
      return NULL;
    }
  addr = (void *) ADDR_SHTOL (mz->addr_64);
  return addr;
}
