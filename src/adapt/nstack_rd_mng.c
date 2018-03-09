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

#include <stdlib.h>
#include "nstack_rd_data.h"
#include "nsfw_mem_api.h"
#include "nstack_log.h"
#include "nstack_securec.h"

#define RD_SHMEM_NAME "rd_table"

#define RD_AGE_MAX_TIME   3

rd_route_table *g_rd_table_handle = NULL;

#define RD_IP_ROUTE_CPY(pnode, name, data) {  \
     if (EOK != STRCPY_S((pnode)->data.stack_name, RD_PLANE_NAMELEN, (name))) \
     { \
        NSSOC_LOGERR("STRCPY_S failed]copy_name=%s", name); \
     } \
     (pnode)->data.type = RD_DATA_TYPE_IP; \
     (pnode)->agetime = 0; \
     (pnode)->data.ipdata.addr = (data)->addr; \
     (pnode)->data.ipdata.masklen = (data)->masklen; \
     (pnode)->data.ipdata.resev[0] = 0;  \
     (pnode)->data.ipdata.resev[1] = 0;  \
}

/*****************************************************************************
*   Prototype    : nstack_rd_mng_int
*   Description  : rd mng moudule init, create a block memory
*   Input        : int flag
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    : nStackMain
*****************************************************************************/
int
nstack_rd_mng_int (int flag)
{
  int ret;
  nsfw_mem_zone zname = {
    {NSFW_SHMEM, NSFW_PROC_MAIN, RD_SHMEM_NAME},
    sizeof (rd_route_table),
    NSFW_SOCKET_ANY,
    0
  };
  NSSOC_LOGINF ("nstack rd mng init begin]flag=%d", flag);
  /*nstack main create, app lookup */
  if (0 == flag)
    {
      g_rd_table_handle = (rd_route_table *) nsfw_mem_zone_create (&zname);
      if (!g_rd_table_handle)
        {
          NSSOC_LOGERR ("mem create fail]mem name=%s", RD_SHMEM_NAME);
          return -1;
        }
      ret =
        MEMSET_S (&(g_rd_table_handle->node[0]), sizeof (rd_route_node), 0,
                  sizeof (rd_route_node));
      if (EOK != ret)
        {
          NSSOC_LOGERR ("MEMSET_S failed]ret=%d", ret);
          return -1;
        }
      g_rd_table_handle->size = NSTACK_RD_DATA_MAX;
      g_rd_table_handle->icnt = 0;
    }
  else                          /* static data will not be erased in fault case */
    {
      g_rd_table_handle =
        (rd_route_table *) nsfw_mem_zone_lookup (&(zname.stname));
      if (!g_rd_table_handle)
        {
          NSSOC_LOGERR ("mem lookup fail]mem name=%s", RD_SHMEM_NAME);
          return -1;
        }
    }
  NSSOC_LOGINF ("nstack rd mng init end flag");
  return 0;
}

/*****************************************************************************
*   Prototype    : nstack_rd_ip_node_insert
*   Description  : insert a rd_ip_data into list
*   Input        : char *name
*                  rd_ip_data *data
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    : nStackMain
*****************************************************************************/
int
nstack_rd_ip_node_insert (char *name, rd_ip_data * data)
{
  if (!g_rd_table_handle)
    {
      NSSOC_LOGERR ("nstack rd mng not inited");
      return -1;
    }
  int iindex = 0;
  rd_route_node *pnode = NULL;
  int agetime = 0;
  int ageindex = -1;
  int freeindex = -1;
  int repeatflag = 0;

  for (iindex = 0; iindex < NSTACK_RD_DATA_MAX; iindex++)
    {
      pnode = &(g_rd_table_handle->node[iindex]);
      /*record the index of first free element */
      if (RD_NODE_USELESS == pnode->flag)
        {
          if (-1 == freeindex)
            {
              freeindex = iindex;
              NSSOC_LOGINF ("nstack rd ip free element index:%d was found",
                            iindex);
            }
          continue;
        }

      /*if is using, and repeat just set flag */
      if (RD_NODE_USING == pnode->flag)
        {
          if (MASK_V (pnode->data.ipdata.addr, pnode->data.ipdata.masklen) ==
              MASK_V (data->addr, data->masklen))
            {
              NSSOC_LOGWAR
                ("nstack:%s, old_addr:0x%x index:%d new_addr:0x%x, masklen:%u was repeat",
                 name, pnode->data.ipdata.addr, iindex, data->addr,
                 data->masklen);
              repeatflag = 1;
            }
          continue;
        }

      /*if flag is deleting, just update the age time, if agetime is on, just set flag to free */
      if (RD_NODE_DELETING == pnode->flag)
        {
          pnode->agetime++;
          if (pnode->agetime >= RD_AGE_MAX_TIME)
            {
              pnode->flag = RD_NODE_USELESS;
              NSSOC_LOGINF
                ("nstack rd ip element index:%d addr:0x%x, masklen:%u was delete and set to free",
                 iindex, pnode->data.ipdata.addr, pnode->data.ipdata.masklen);
            }
          /*record delete time */
          if (agetime < pnode->agetime)
            {
              agetime = pnode->agetime;
              ageindex = iindex;
            }
          continue;
        }
    }

  /*if repeat, just return */
  if (1 == repeatflag)
    {
      return 0;
    }
  if (-1 == freeindex)
    {
      if (-1 != ageindex)
        {
          freeindex = ageindex;
        }
      else
        {
          NSSOC_LOGERR
            ("the rd table is full,nstack:%s, rd addr:0x%x, masklen:%u can't be inserted",
             name, data->addr, data->masklen);
          return -1;
        }
    }
  pnode = &(g_rd_table_handle->node[freeindex]);
  /*if no free found, just reuse the big agetime */
  RD_IP_ROUTE_CPY (pnode, name, data);
  pnode->flag = RD_NODE_USING;  /*last set */
  g_rd_table_handle->icnt++;
  NSSOC_LOGINF ("nstack:%s, rd addr:0x%x, masklen:%u index was inserted",
                name, data->addr, data->masklen);
  return 0;
}

/*****************************************************************************
*   Prototype    : nstack_rd_ip_node_delete
*   Description  : rd data delete
*   Input        : rd_ip_data *data
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    : nStackMain
*                  just set delete flag, becuase
*****************************************************************************/
int
nstack_rd_ip_node_delete (rd_ip_data * data)
{
  int iindex = 0;
  rd_route_node *pnode = NULL;

  if (!g_rd_table_handle)
    {
      NSSOC_LOGERR ("nstack rd mng not inited");
      return -1;
    }

  for (iindex = 0; iindex < NSTACK_RD_DATA_MAX; iindex++)
    {
      pnode = &(g_rd_table_handle->node[iindex]);
      if ((RD_NODE_USING == pnode->flag)
          && (MASK_V (pnode->data.ipdata.addr, pnode->data.ipdata.masklen) ==
              MASK_V (data->addr, data->masklen)))
        {
          pnode->flag = RD_NODE_DELETING;       /*just set deleting state */
          pnode->agetime = 0;
          g_rd_table_handle->icnt--;
          NSSOC_LOGINF
            ("nstack rd delete:%s, addr:0x%x, masklen:%u index:%d was delete",
             pnode->data.stack_name, data->addr, data->masklen, iindex);
          return 0;
        }
    }
  NSSOC_LOGINF ("nstack rd delete, addr:0x%x, masklen:%u index was not found",
                data->addr, data->masklen);
  return 0;
}

/*****************************************************************************
*   Prototype    : nstack_rd_ip_get
*   Description  : get rd data from rd table
*   Input        : char *planename
*                  rd_route_data **data
*                  int *num
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*   Note         : a block memory was alloc and return by data,
*                  this need free by caller if return 0, otherwise no need
*****************************************************************************/
int
nstack_rd_ip_get (rd_route_data ** data, int *num)
{
  rd_route_data *pdata = NULL;
  rd_route_node *pnode = NULL;
  int size = 0;
  int icnt = 0;
  int idex = 0;
  int ret;

  if (!g_rd_table_handle || !data || !num)
    {
      NSSOC_LOGERR ("nstack rd mng not inited or input err");
      return -1;
    }
  size = sizeof (rd_route_data) * g_rd_table_handle->size;
  pdata = (rd_route_data *) malloc (size);
  if (!pdata)
    {
      NSSOC_LOGERR ("rd route data malloc fail");
      return -1;
    }
  ret = MEMSET_S (pdata, size, 0, size);
  if (EOK != ret)
    {
      NSSOC_LOGERR ("MEMSET_S failed]ret=%d", ret);
      free (pdata);
      return -1;
    }
  for (icnt = 0; icnt < g_rd_table_handle->size; icnt++)
    {
      pnode = &(g_rd_table_handle->node[icnt]);
      if (RD_NODE_USING == pnode->flag)
        {
          pdata[idex].type = pnode->data.type;
          pdata[idex].ipdata.addr = pnode->data.ipdata.addr;
          pdata[idex].ipdata.masklen = pnode->data.ipdata.masklen;
          pdata[idex].ipdata.resev[0] = pnode->data.ipdata.resev[0];
          pdata[idex].ipdata.resev[1] = pnode->data.ipdata.resev[1];
          ret =
            STRCPY_S (pdata[idex].stack_name, RD_PLANE_NAMELEN,
                      pnode->data.stack_name);
          if (EOK != ret)
            {
              NSSOC_LOGERR ("STRCPY_S failed]ret=%d", ret);
              free (pdata);
              return -1;
            }
          idex++;
        }
    }
  /*if no data fetched , just return fail */
  if (idex == 0)
    {
      free (pdata);
      return -1;
    }
  *data = pdata;
  *num = idex;
  return 0;
}
