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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "nstack_rd.h"
#include "nstack_rd_init.h"
#include "nstack_rd_priv.h"
#include "nstack_rd_ip.h"
#include "nstack_rd_proto.h"
#include "nstack_log.h"
#include "nstack_info_parse.h"
#include "nstack_securec.h"

typedef struct __rd_data_defaut_ip
{
  char ip[RD_IP_STR_MAX_LEN];
  char stackname[RD_PLANE_NAMELEN];
  int masklent;
} rd_data_defaut_ip;

typedef struct __rd_data_defaut_protocol
{
  unsigned int proto_type;
  char stackname[RD_PLANE_NAMELEN];
} rd_data_defaut_protocol;

rd_data_proc g_rd_cpy[RD_DATA_TYPE_MAX] = {
  {
   nstack_rd_ipdata_cpy,
   nstack_rd_ip_item_insert,
   nstack_rd_ip_item_age,
   nstack_rd_ip_item_find,
   nstack_rd_ip_spec,
   nstack_rd_ip_default,
   },
  {
   nstack_rd_proto_cpy,
   nstack_rd_proto_item_insert,
   nstack_rd_proto_item_age,
   nstack_rd_proto_item_find,
   nstack_rd_proto_spec,
   nstack_rd_proto_default,
   },
};

rd_data_defaut_ip g_default_ip_config[] = {
  {{"127.0.0.1"}, {RD_KERNEL}, 32},
  {{"0.0.0.0"}, {RD_KERNEL}, 32},
};

rd_data_defaut_protocol g_default_protcol[] = {
  {0xf001, {RD_LWIP}},
};

/*****************************************************************************
*   Prototype    : nstack_rd_get_stackid
*   Description  : choose the stack by key, type is the most important
*   Input        : nstack_rd_key* pkey
*                  int *stackid
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nstack_rd_get_stackid (nstack_rd_key * pkey, int *stackid)
{
  int type = 0;
  int ret = NSTACK_RD_SUCCESS;
  rd_data_item item;
  if ((!pkey) || (!stackid) || (pkey->type >= RD_DATA_TYPE_MAX))
    {
      NSSOC_LOGERR ("input get stackid fail]addr=%p,stackid=%p,addr->type=%d",
                    pkey, stackid, !pkey ? RD_DATA_TYPE_MAX : pkey->type);
      return NSTACK_RD_FAIL;
    }
  int retVal = MEMSET_S (&item, sizeof (item), 0, sizeof (item));
  if (EOK != retVal)
    {
      NSSOC_LOGERR ("MEMSET_S failed]retVal=%d", retVal);
      return NSTACK_RD_FAIL;
    }
  type = pkey->type;

  /*specfic key find, for ip example: stack-x was chose if the key is multicast ip */
  if (g_rd_cpy[type].rd_item_spec)
    {
      ret = g_rd_cpy[type].rd_item_spec ((void *) pkey);
      if (ret >= 0)
        {
          *stackid = ret;
          return NSTACK_RD_SUCCESS;
        }
    }

  /*search the list */
  ret =
    g_rd_cpy[type].rd_item_find (NSTACK_RD_LIST (type), (void *) pkey, &item);
  if (NSTACK_RD_SUCCESS == ret)
    {
      NSSOC_LOGDBG ("item type=%d stackid=%d was found", pkey->type,
                    item.stack_id);
      *stackid = item.stack_id;
      return NSTACK_RD_SUCCESS;
    }
  if (g_rd_cpy[type].rd_item_default)
    {
      *stackid = g_rd_cpy[type].rd_item_default ((void *) pkey);
    }
  else
    {
      *stackid = -1;
    }
  NSSOC_LOGINF ("item type=%d was not found, return default=%d", pkey->type,
                *stackid);
  return NSTACK_RD_SUCCESS;
}

/*****************************************************************************
*   Prototype    : nstack_rd_sys_default
*   Description  : sys default rd info,
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
******************************************************************************/
void
nstack_rd_sys_default ()
{
  rd_data_item item;
  rd_data_defaut_ip *pdata = NULL;
  rd_data_defaut_protocol *pprotodata = NULL;
  int icnt = 0, iindex = 0;
  int stack_num = 0;

  stack_num = g_rd_local_data->stack_num;

  /*get the ip default route */
  for (icnt = 0;
       icnt < sizeof (g_default_ip_config) / sizeof (rd_data_defaut_ip);
       icnt++)
    {
      pdata = &g_default_ip_config[icnt];
      for (iindex = 0; iindex < stack_num; iindex++)
        {
          if (0 ==
              strcmp (g_rd_local_data->pstack_info[iindex].name,
                      pdata->stackname))
            {
              item.stack_id = g_rd_local_data->pstack_info[iindex].stack_id;
              break;
            }
        }
      if (iindex >= stack_num)
        {
          NSSOC_LOGINF
            ("default stack name:%s was not fount, ip:%s msklen:%d was dropped",
             pdata->stackname, pdata->ip, pdata->masklent);
          continue;
        }
      item.type = RD_DATA_TYPE_IP;
      item.ipdata.addr = ntohl (inet_addr (pdata->ip));
      item.ipdata.masklen = pdata->masklent;
      item.ipdata.resev[0] = 0;
      item.ipdata.resev[1] = 0;
      item.agetime = NSTACK_RD_AGETIME_MAX;
      /*insert to the list */
      g_rd_cpy[RD_DATA_TYPE_IP].rd_item_inset (NSTACK_RD_LIST
                                               (RD_DATA_TYPE_IP), &item);
    }

  /*get the protocol default route */
  (void) MEMSET_S (&item, sizeof (item), 0, sizeof (item));
  for (icnt = 0;
       icnt < sizeof (g_default_protcol) / sizeof (rd_data_defaut_protocol);
       icnt++)
    {
      pprotodata = &g_default_protcol[icnt];
      for (iindex = 0; iindex < stack_num; iindex++)
        {
          if (0 ==
              strcmp (g_rd_local_data->pstack_info[iindex].name,
                      pprotodata->stackname))
            {
              item.stack_id = g_rd_local_data->pstack_info[iindex].stack_id;
              break;
            }
        }
      if (iindex >= stack_num)
        {
          NSSOC_LOGINF
            ("default stack name:%s was not fount, protocoltype:%d was dropped",
             pprotodata->stackname, pprotodata->proto_type);
          continue;
        }
      item.type = RD_DATA_TYPE_PROTO;
      item.proto_type = pprotodata->proto_type;
      /*insert to the list */
      g_rd_cpy[RD_DATA_TYPE_PROTO].rd_item_inset (NSTACK_RD_LIST
                                                  (RD_DATA_TYPE_PROTO),
                                                  &item);
    }
  return;
}

/*****************************************************************************
*   Prototype    : nstack_rd_save
*   Description  : save the rd data
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
void
nstack_rd_save (rd_route_data * rd_data, int num)
{
  int icnt = 0;
  int iindex = 0;
  int stack_num = 0;
  rd_data_item item;
  rd_data_type type = RD_DATA_TYPE_MAX;

  int retVal = MEMSET_S (&item, sizeof (item), 0, sizeof (item));
  if (EOK != retVal)
    {
      NSSOC_LOGERR ("MEMSET_S failed]retVal=%d", retVal);
      return;
    }

  stack_num = g_rd_local_data->stack_num;

  for (iindex = 0; iindex < num; iindex++)
    {
      if (rd_data[iindex].type >= RD_DATA_TYPE_MAX)
        {
          NSSOC_LOGERR ("rd data type=%d unkown", rd_data[iindex].type);
          continue;
        }

      type = rd_data[iindex].type;

      if (NSTACK_RD_SUCCESS ==
          g_rd_cpy[type].rd_item_cpy ((void *) &item,
                                      (void *) &rd_data[iindex]))
        {
          item.agetime = NSTACK_RD_AGETIME_MAX;
          for (icnt = 0; icnt < stack_num; icnt++)
            {
              if (0 ==
                  strcmp (g_rd_local_data->pstack_info[icnt].name,
                          rd_data[iindex].stack_name))
                {
                  item.stack_id = g_rd_local_data->pstack_info[icnt].stack_id;
                  break;
                }
            }
          if (icnt >= stack_num)
            {
              NSSOC_LOGINF
                ("plane name:%s was not fount, protocoltype:%d was dropped",
                 rd_data[iindex].stack_name);
              continue;
            }
          /*insert to the list */
          g_rd_cpy[type].rd_item_inset (NSTACK_RD_LIST (type), &item);
          continue;
        }

      NSSOC_LOGERR ("rd data type=%d cpy fail", rd_data[iindex].type);
    }
  return;
}

/*****************************************************************************
*   Prototype    : nstack_rd_data_get
*   Description  : rd data get,
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nstack_rd_data_get (nstack_get_route_data pfun)
{
  rd_route_data *rd_data = NULL;
  int iret = NSTACK_RD_FAIL;
  int inum = 0;

  /*get rd config */
  if (pfun && (NSTACK_RD_SUCCESS == pfun (&rd_data, &inum)))
    {
      if (inum > 0)
        {
          nstack_rd_save (rd_data, inum);
          iret = NSTACK_RD_SUCCESS;
        }
      else
        {
          NSSOC_LOGDBG ("no rd data got");
        }
      if (rd_data)
        {
          free (rd_data);
          rd_data = NULL;
        }
    }
  else
    {
      NSSOC_LOGERR ("nstack rd sys rd info fail");
    }
  return iret;
}

/*****************************************************************************
*   Prototype    : nstack_rd_sys
*   Description  : sys rd data from rd table,
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nstack_rd_sys ()
{
  int iret = NSTACK_RD_FAIL;
  int icnt = 0;
  if (!g_rd_local_data)
    {
      NSSOC_LOGERR ("rd have not been inited");
      return NSTACK_RD_FAIL;
    }

  /*insert default rd info */
  nstack_rd_sys_default ();

  /*get from config file */
  for (icnt = 0; icnt < g_rd_local_data->fun_num; icnt++)
    {
      if (NSTACK_RD_SUCCESS ==
          nstack_rd_data_get (g_rd_local_data->sys_fun[icnt]))
        {
          iret = NSTACK_RD_SUCCESS;
        }
    }

  /*age after sys */
  nstack_rd_age ();
  return iret;
}

/*****************************************************************************
*   Prototype    : nstack_rd_age
*   Description  : delete all rd item from the list that not been add again
                   for at least one time
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nstack_rd_age ()
{
  int icnt = 0;
  for (icnt = 0; icnt < RD_DATA_TYPE_MAX; icnt++)
    {
      (void) g_rd_cpy[icnt].rd_item_age (NSTACK_RD_LIST (icnt));
    }
  return NSTACK_RD_SUCCESS;
}
