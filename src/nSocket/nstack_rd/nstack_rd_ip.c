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
#include "nstack_rd_ip.h"
#include "nstack_log.h"
#include "nstack_securec.h"

#include "nstack_ip_addr.h"

#define NSTACK_IP_MLSTACKID  RD_LWIP

#define PP_HTONL(x) ((((x) & 0xff) << 24) | \
                     (((x) & 0xff00) << 8) | \
                     (((x) & 0xff0000UL) >> 8) | \
                     (((x) & 0xff000000UL) >> 24))

#define rd_ismulticast(addr)(((unsigned int)(addr) & 0xf0000000UL) == 0xe0000000UL)

int g_multi_stackid = -1;

/*copy rd data*/
int
nstack_rd_ipdata_cpy (void *destdata, void *srcdata)
{
  rd_data_item *pitem = (rd_data_item *) destdata;
  rd_route_data *pdata = (rd_route_data *) srcdata;

  pitem->type = pdata->type;
  pitem->ipdata.addr = pdata->ipdata.addr;
  pitem->ipdata.masklen = pdata->ipdata.masklen;
  pitem->ipdata.resev[0] = pdata->ipdata.resev[0];
  pitem->ipdata.resev[1] = pdata->ipdata.resev[1];
  return NSTACK_RD_SUCCESS;
}

/*
 * Add an ip segment to the list and sort it in descending order of ip mask length
 * If the list already exists in the same list of ip side, then stack_id update
 *ip is local byteorder
 */
int
nstack_rd_ip_item_insert (nstack_rd_list * hlist, void *rditem)
{
  nstack_rd_node *pdatanode = NULL;
  nstack_rd_node *tempdata = NULL;
  struct hlist_node *tempnode = NULL;
  struct hlist_node *tem = NULL;
  unsigned int ip_addr = 0;
  unsigned int ip_masklen = 0;
  unsigned int ip_maskv = MASK_V (ip_addr, ip_masklen);
  unsigned int tempip_addr = 0;
  unsigned int tempip_masklen = 0;
  rd_data_item *pitem = (rd_data_item *) rditem;

  ip_masklen = pitem->ipdata.masklen;
  NSSOC_LOGDBG ("stackid:%d, ipaddr:%u.%u.%u.%u masklen:0x%x was inserted",
                pitem->stack_id, ip4_addr4_16 (&pitem->ipdata.addr),
                ip4_addr3_16 (&pitem->ipdata.addr),
                ip4_addr2_16 (&pitem->ipdata.addr),
                ip4_addr1_16 (&pitem->ipdata.addr), pitem->ipdata.masklen);

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
  NSTACK_RD_IP_ITEM_COPY (&(pdatanode->item), pitem);

  if (hlist_empty (&(hlist->headlist)))
    {
      hlist_add_head (&(pdatanode->rdnode), &(hlist->headlist));
      return NSTACK_RD_SUCCESS;
    }
  hlist_for_each_entry (tempdata, tempnode, &(hlist->headlist), rdnode)
  {
    tem = tempnode;
    tempip_addr = tempdata->item.ipdata.addr;
    tempip_masklen = tempdata->item.ipdata.masklen;
    if (ip_masklen < tempip_masklen)
      {
        continue;
      }

    /*if already exist, just return success */
    if (ip_maskv == MASK_V (tempip_addr, tempip_masklen))
      {
        NSSOC_LOGDBG
          ("insert ip:%u.%u.%u.%u, mask:0x%x, stack_id:%d, exist orgid:%d",
           ip4_addr4_16 (&pitem->ipdata.addr),
           ip4_addr3_16 (&pitem->ipdata.addr),
           ip4_addr2_16 (&pitem->ipdata.addr),
           ip4_addr1_16 (&pitem->ipdata.addr), pitem->ipdata.masklen,
           pitem->stack_id, tempdata->item.stack_id);

        tempdata->item.stack_id = pitem->stack_id;
        tempdata->item.agetime = NSTACK_RD_AGETIME_MAX;
        free (pdatanode);
        return NSTACK_RD_SUCCESS;
      }
    hlist_add_before (&(pdatanode->rdnode), tempnode);
    return NSTACK_RD_SUCCESS;
  }
  hlist_add_after (tem, &(pdatanode->rdnode));
  return NSTACK_RD_SUCCESS;
}

/*
 *find stackid by ip
 *input ip must netorder
 */
int
nstack_rd_ip_item_find (nstack_rd_list * hlist, void *rdkey, void *outitem)
{
  struct hlist_node *tempnode = NULL;
  nstack_rd_node *tempdata = NULL;
  unsigned int tempip_addr = 0;
  unsigned int tempip_masklen = 0;
  nstack_rd_key *key = (nstack_rd_key *) rdkey;
  rd_data_item *pitem = (rd_data_item *) outitem;
  unsigned int ip_addr = 0;
  /*need to convert to local order */
  ip_addr = ntohl (key->ip_addr);

  hlist_for_each_entry (tempdata, tempnode, &(hlist->headlist), rdnode)
  {
    tempip_addr = tempdata->item.ipdata.addr;
    tempip_masklen = tempdata->item.ipdata.masklen;
    /*if already exist, just return success */
    if (MASK_V (ip_addr, tempip_masklen) ==
        MASK_V (tempip_addr, tempip_masklen))
      {
        NSTACK_RD_IP_ITEM_COPY (pitem, &(tempdata->item));
        return NSTACK_RD_SUCCESS;
      }
  }

  NSSOC_LOGDBG ("ip=%u.%u.%u.%u item not found", ip4_addr4_16 (&ip_addr),
                ip4_addr3_16 (&ip_addr),
                ip4_addr2_16 (&ip_addr), ip4_addr1_16 (&ip_addr));

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
nstack_rd_ip_item_age (nstack_rd_list * hlist)
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
            NSSOC_LOGDBG
              ("stackid:%d, addrip:%u.%u.%u.%u, masklen:0x%x was aged",
               tempdata->item.stack_id,
               ip4_addr4_16 (&tempdata->item.ipdata.addr),
               ip4_addr3_16 (&tempdata->item.ipdata.addr),
               ip4_addr2_16 (&tempdata->item.ipdata.addr),
               ip4_addr1_16 (&tempdata->item.ipdata.addr),
               tempdata->item.ipdata.masklen);

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
          NSSOC_LOGDBG
            ("stackid:%d, addrip:%u.%u.%u.%u, masklen:0x%x was last aged",
             tempdata->item.stack_id,
             ip4_addr4_16 (&tempdata->item.ipdata.addr),
             ip4_addr3_16 (&tempdata->item.ipdata.addr),
             ip4_addr2_16 (&tempdata->item.ipdata.addr),
             ip4_addr1_16 (&tempdata->item.ipdata.addr),
             tempdata->item.ipdata.masklen);
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
nstack_rd_ip_spec (void *rdkey)
{
  nstack_rd_key *key = (nstack_rd_key *) rdkey;
  unsigned int ip_addr = 0;

  ip_addr = ntohl (key->ip_addr);

  if (rd_ismulticast (ip_addr))
    {
      if (-1 == g_multi_stackid)
        {
          g_multi_stackid = nstack_get_stackid_byname (NSTACK_IP_MLSTACKID);
        }
      return g_multi_stackid;
    }
  return -1;
}

int
nstack_rd_ip_default (void *rdkey)
{
  return NSTACK_GET_STACK (0);
}
