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
#include <arpa/inet.h>
#include "nstack_rd_data.h"
#include "nstack_rd.h"
#include "nstack_rd_init.h"
#include "nstack_rd_priv.h"
#include "nstack_rd_proto.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "common_mem_common.h"

/*copy rd data*/
int
nstack_rd_proto_cpy (void *destdata, void *srcdata)
{
  rd_data_item *pitem = (rd_data_item *) destdata;
  rd_route_data *pdata = (rd_route_data *) srcdata;
  pitem->type = pdata->type;
  pitem->proto_type = pdata->proto_type;
  return NSTACK_RD_SUCCESS;
}

/*
 * Add an ip segment to the list and sort it in descending order of ip mask length
 * If the list already exists in the same list of ip side, then stack_id update
 *ip is local byteorder
 */
int
nstack_rd_proto_item_insert (nstack_rd_list * hlist, void *rditem)
{
  nstack_rd_node *pdatanode = NULL;
  nstack_rd_node *tempdata = NULL;
  struct hlist_node *tempnode = NULL;
  rd_data_item *pitem = (rd_data_item *) rditem;

  NSSOC_LOGDBG ("stackid:%d, protocol type:%d was inserted", pitem->stack_id,
                pitem->proto_type);

  pdatanode = (nstack_rd_node *) malloc (sizeof (nstack_rd_node));
  if (!pdatanode)
    {
      NSSOC_LOGERR ("nstack rd item malloc fail");
      return NSTACK_RD_FAIL;
    }
  int retVal =
    MEMSET_S (pdatanode, sizeof (nstack_rd_node), 0, sizeof (nstack_rd_node));
  if (EOK != retVal)
    {
      NSSOC_LOGERR ("MEMSET_S failed]retVal=%d", retVal);
      free (pdatanode);
      return NSTACK_RD_FAIL;
    }
  INIT_HLIST_NODE (&pdatanode->rdnode);
  NSTACK_RD_PROTO_ITEM_COPY (&(pdatanode->item), pitem);
  if (hlist_empty (&(hlist->headlist)))
    {
      hlist_add_head (&(pdatanode->rdnode), &(hlist->headlist));

      return NSTACK_RD_SUCCESS;

    }

  hlist_for_each_entry (tempdata, tempnode, &(hlist->headlist), rdnode)
  {
    if (tempdata->item.proto_type == pitem->proto_type)

      {
        tempdata->item.stack_id = pitem->stack_id;
        tempdata->item.agetime = NSTACK_RD_AGETIME_MAX;
        free (pdatanode);
        return NSTACK_RD_SUCCESS;
      }
  }
  hlist_add_head (&(pdatanode->rdnode), &(hlist->headlist));

  return NSTACK_RD_SUCCESS;

}

/*
 *find stackid by ip
 *input ip must netorder
 */
int
nstack_rd_proto_item_find (nstack_rd_list * hlist, void *rdkey, void *outitem)
{
  struct hlist_node *tempnode = NULL;
  nstack_rd_node *tempdata = NULL;
  nstack_rd_key *key = (nstack_rd_key *) rdkey;
  rd_data_item *pitem = (rd_data_item *) outitem;
  hlist_for_each_entry (tempdata, tempnode, &(hlist->headlist), rdnode)
  {

    /*if already exist, just return success */
    if (tempdata->item.proto_type == key->proto_type)

      {
        NSTACK_RD_PROTO_ITEM_COPY (pitem, &(tempdata->item));
        return NSTACK_RD_SUCCESS;
      }
  }

  NSSOC_LOGDBG ("protocol type item not found", key->proto_type);

  return NSTACK_RD_FAIL;
}

/*****************************************************************************
*   Prototype    : nstack_rd_ip_item_age
*   Description  : delete the ip item that have not been add again for one time
*   Input        : nstack_rd_list *hlist
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int
nstack_rd_proto_item_age (nstack_rd_list * hlist)
{
  struct hlist_node *tempnode = NULL;
  nstack_rd_node *tempdata = NULL;
  nstack_rd_node *prevdata = NULL;
  struct hlist_node *prevnode = NULL;
  NSSOC_LOGINF ("nstack rd ip age begin");
  hlist_for_each_entry (tempdata, tempnode, &(hlist->headlist), rdnode)
  {
    /*if agetime equal 0, remove it */
    if (tempdata->item.agetime <= 0)
      {
        if (prevdata)
          {
            NSSOC_LOGDBG ("stackid:%d, protocol type was aged",
                          tempdata->item.stack_id, tempdata->item.proto_type);

            hlist_del_init (prevnode);
            free (prevdata);
          }
        prevdata = tempdata;
        prevnode = tempnode;
      }
    else
      {
        tempdata->item.agetime--;
      }
  }
  if (prevdata)
    {
      if (tempdata)
        {
          NSSOC_LOGDBG ("stackid:%d, protocol type was aged",
                        tempdata->item.stack_id, tempdata->item.proto_type);
        }
      hlist_del_init (prevnode);
      free (prevdata);
    }
  NSSOC_LOGINF ("nstack rd ip age end");
  return NSTACK_RD_SUCCESS;
}

/*
 *find stackid by spec ip(multicast ip)
 *input ip must netorder
 */
int
nstack_rd_proto_spec (void *rdkey)
{
  return -1;
}

int
nstack_rd_proto_default (void *rdkey)
{
  return -1;
}
