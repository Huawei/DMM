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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "lwip/inet.h"
#include "trp_rb_tree.h"
#include "container_ip.h"
#include "network.h"
#include "netif.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "nstack_rd_mng.h"
#include "config_common.h"
#include "igmp.h"
#include "spl_def.h"
#include "stackx_ip_addr.h"
#include "hal_api.h"
#include "spl_hal.h"

struct container_list g_container_list = { 0 };
static trp_rb_root_t g_container_ip_root = { 0 };       //only handled in tcpip thread, no need protect it with a lock
static trp_rb_root_t g_container_multicast_root = { 0 };        //only handled in tcpip thread, no need protect it with a lock

static void free_container_port (struct container_port *port,
                                 bool_t only_free);

/*unsigned int value is typecasted into void * pointer and passed as argument to
this function. so the value can never be > 0xFFFFFFFF. so can suppress the warning*/

static int
ip_compare (trp_key_t left, trp_key_t right)
{
  //return (int)((unsigned long)left - (unsigned long)right);

  if (left > right)
    {
      return 1;
    }
  else if (left < right)
    {
      return -1;
    }
  else
    {
      return 0;
    }
}

NSTACK_STATIC bool_t
is_container_ok (struct container_ip * container)
{
  if (!container->ports_list)
    {
      return 0;
    }

  return 1;
}

NSTACK_STATIC void
add_port (struct container_ip *container, struct container_port *port)
{
  if (port->ip_cidr_list)
    {
      port->next = container->ports_list;
      container->ports_list = port;
    }
  else
    {
      free_container_port (port, IP_MODULE_TRUE);
    }
}

static void
add_ip_cidr (struct container_port *port,
             struct container_port_ip_cidr *ip_cidr)
{
  if (!ip_cidr)
    {
      return;
    }

  ip_cidr->next = port->ip_cidr_list;
  port->ip_cidr_list = ip_cidr;
  return;
}

NSTACK_STATIC void
add_multilcast_ip (struct container_port *port,
                   struct container_multicast_id *muticastIP)
{
  if (!muticastIP)
    {
      return;
    }

  muticastIP->next = port->multicast_list;
  port->multicast_list = muticastIP;
  return;
}

NSTACK_STATIC void
free_container_port_ip_cidr (struct container_port_ip_cidr *ip_cidr,
                             bool_t only_free)
{
  output_api *api = get_output_api ();
  struct container_port_ip_cidr *ip_cidr_tmp = NULL;

  while (ip_cidr)
    {
      ip_cidr_tmp = ip_cidr;
      ip_cidr = ip_cidr_tmp->next;
      if (!only_free)
        {
          if (api->del_netif_ip)
            {
              struct network_configuration *network =
                get_network_by_ip_with_tree (ip_cidr_tmp->ip);
              if (network)
                {
                  if (network->phy_net->bond_name[0] != 0)
                    {
                      (void) api->del_netif_ip (network->phy_net->bond_name, ip_cidr_tmp->ip);  //fails only when netif_name not exist, no side effect so don't check return value.
                    }
                  else
                    {
                      (void) api->del_netif_ip (network->phy_net->
                                                header->nic_name,
                                                ip_cidr_tmp->ip);
                    }
                }
              else
                {
                  NSOPR_LOGERR ("can't find network by]IP=%u",
                                ip_cidr_tmp->ip);
                }
            }

          trp_rb_erase ((void *) (u64_t) ip_cidr_tmp->ip,
                        &g_container_ip_root, ip_compare);
        }

      free (ip_cidr_tmp);
      ip_cidr_tmp = NULL;
    }
}

/*note:::the ip must be local order*/
void
container_multicast_rd (unsigned int ip, int op)
{
  rd_ip_data rd_ip = { 0 };
  int ret = 0;

  rd_ip.addr = ip;
  rd_ip.masklen = 32;
  if (0 == op)
    {
      ret = nstack_rd_ip_node_insert ("nstack-dpdk", &rd_ip);
    }
  else
    {
      ret = nstack_rd_ip_node_delete (&rd_ip);
    }

  if (0 != ret)
    {
      NSOPR_LOGERR ("nstack rd multicast ip:0x%x %s fail", ip,
                    (0 == op ? "insert" : "delete"));
    }
  else
    {
      NSOPR_LOGDBG ("nstack rd multicast ip:0x%x %s success", ip,
                    (0 == op ? "insert" : "delete"));
    }

  return;
}

static void
free_container_multicast (struct container_multicast_id *multicast,
                          bool_t only_free)
{
  struct container_multicast_id *tmp = NULL;

  while (multicast)
    {
      tmp = multicast;
      multicast = multicast->next;
      if (!only_free)
        {
          /*note:::multicast ip is network, need to change to local order, delete multicast ip from rd. */
          container_multicast_rd (spl_ntohl (tmp->ip), 1);

          trp_rb_erase ((void *) (u64_t) tmp->ip, &g_container_multicast_root,
                        ip_compare);
        }

      free (tmp);
      tmp = NULL;
    }
}

static void
free_container_port (struct container_port *port, bool_t only_free)
{
  struct container_port *port_tmp = NULL;
  struct container_port *port_curr = port;

  while (port_curr)
    {
      port_tmp = port_curr;
      port_curr = port_tmp->next;

      free_container_multicast (port_tmp->multicast_list, only_free);
      free_container_port_ip_cidr (port_tmp->ip_cidr_list, only_free);

      if (port_tmp->buffer)
        {
          free_port_buffer (port_tmp->buffer);
          port_tmp->buffer = NULL;
        }

      free (port_tmp);
      port_tmp = NULL;
    }
}

void
free_container (struct container_ip *container, bool_t only_free)
{
  struct container_ip *container_tmp = NULL;
  struct container_ip *container_curr = container;

  while (container_curr)
    {
      container_tmp = container_curr;
      container_curr = container_tmp->next;
      if (container_tmp->ports_list)
        {
          free_container_port (container_tmp->ports_list, only_free);
        }

      free (container_tmp);
      container_tmp = NULL;
    }
}

struct container_port *
parse_port_obj (struct json_object *port_obj)
{
  int retval;
  struct json_object *port_name_obj = NULL;
  struct json_object *ip_cidr_list_obj = NULL;
  struct json_object *mcIDObj = NULL;

  if (!port_obj)
    {
      NSOPR_LOGERR ("port_obj is null");
      return NULL;
    }

  struct container_port *port = malloc (sizeof (struct container_port));
  if (!port)
    {
      NSOPR_LOGERR ("malloc failed");
      return NULL;
    }

  retval =
    MEMSET_S (port, sizeof (struct container_port), 0,
              sizeof (struct container_port));
  if (EOK != retval)
    {
      NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retval);
      free (port);
      return NULL;
    }

  json_object_object_get_ex (port_obj, "port_name", &port_name_obj);
  if (port_name_obj)
    {
      const char *port_name = json_object_get_string (port_name_obj);
      if ((NULL == port_name)
          || (strlen (port_name) >= IP_MODULE_MAX_NAME_LEN))
        {
          NSOPR_LOGERR ("port name is not ok");
          goto RETURN_ERROR;
        }

      retval =
        STRCPY_S (port->port_name, sizeof (port->port_name), port_name);

      if (EOK != retval)
        {
          NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
          goto RETURN_ERROR;
        }
    }

  json_object_object_get_ex (port_obj, "ip_cidr", &ip_cidr_list_obj);
  if (ip_cidr_list_obj)
    {
      int j;
      int ip_cidr_num = json_object_array_length (ip_cidr_list_obj);
      for (j = 0; j < ip_cidr_num; ++j)
        {
          struct json_object *ip_cidr_obj =
            json_object_array_get_idx (ip_cidr_list_obj, j);
          if (ip_cidr_obj)
            {
              char tmp[IP_MODULE_LENGTH_32] = { 0 };
              struct container_port_ip_cidr *port_ip_cidr =
                malloc (sizeof (struct container_port_ip_cidr));
              if (NULL == port_ip_cidr)
                {
                  NSOPR_LOGERR ("malloc failed");
                  goto RETURN_ERROR;
                }

              retval =
                MEMSET_S (port_ip_cidr,
                          sizeof (struct container_port_ip_cidr), 0,
                          sizeof (struct container_port_ip_cidr));
              if (EOK != retval)
                {
                  NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retval);
                  free (port_ip_cidr);
                  port_ip_cidr = NULL;
                  goto RETURN_ERROR;
                }

              const char *ip_cidr = json_object_get_string (ip_cidr_obj);
              if ((NULL == ip_cidr) || (ip_cidr[0] == 0))
                {
                  NSOPR_LOGERR ("ip_cidr is not ok");
                  free (port_ip_cidr);
                  port_ip_cidr = NULL;
                  goto RETURN_ERROR;
                }

              const char *sub = strstr (ip_cidr, "/");
              if ((NULL == sub)
                  || (sizeof (tmp) - 1 < (unsigned int) (sub - ip_cidr))
                  || (strlen (sub) > sizeof (tmp) - 1))
                {
                  NSOPR_LOGERR
                    ("Error : Ipaddress notation must be in ip cidr notation!");
                  free (port_ip_cidr);
                  port_ip_cidr = NULL;
                  goto RETURN_ERROR;
                }

              retval =
                STRNCPY_S (tmp, sizeof (tmp), ip_cidr,
                           (size_t) (sub - ip_cidr));
              if (EOK != retval)
                {
                  NSOPR_LOGERR ("STRNCPY_S failed]ret=%d", retval);
                  free (port_ip_cidr);
                  port_ip_cidr = NULL;
                  goto RETURN_ERROR;
                }

              struct in_addr addr;
              int iRet;
              retval = MEMSET_S (&addr, sizeof (addr), 0, sizeof (addr));
              if (EOK != retval)
                {
                  NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retval);
                  free (port_ip_cidr);
                  port_ip_cidr = NULL;
                  goto RETURN_ERROR;
                }

              iRet = spl_inet_aton (tmp, &addr);
              if (0 == iRet)
                {
                  NSOPR_LOGERR ("spl_inet_aton failed");
                  free (port_ip_cidr);
                  port_ip_cidr = NULL;
                  goto RETURN_ERROR;
                }

              port_ip_cidr->ip = addr.s_addr;
              iRet = atoi (sub + 1);
              if ((iRet <= 0) || (iRet > IP_MODULE_LENGTH_32))
                {
                  NSOPR_LOGERR ("IP mask length is not correct");
                  free (port_ip_cidr);
                  port_ip_cidr = NULL;
                  goto RETURN_ERROR;
                }

              port_ip_cidr->mask_len = (unsigned int) iRet;
              add_ip_cidr (port, port_ip_cidr);
            }
        }
    }

  json_object_object_get_ex (port_obj, "multicast_id", &mcIDObj);
  if (mcIDObj)
    {
      int j;
      int arrLen = json_object_array_length (mcIDObj);
      if (0 == arrLen)
        {
          NSOPR_LOGERR ("arrLen is 0");
          goto RETURN_ERROR;
        }

      for (j = 0; j < arrLen; ++j)
        {
          struct json_object *elemObj =
            json_object_array_get_idx (mcIDObj, j);

          if (elemObj)
            {
              struct json_object *tObj = NULL;
              const char *tStr;
              int ret;
              struct in_addr addr;

              struct container_multicast_id *mcID =
                malloc (sizeof (struct container_multicast_id));
              if (NULL == mcID)
                {
                  NSOPR_LOGERR ("Can't alloc container multicast id");
                  goto RETURN_ERROR;
                }

              json_object_object_get_ex (elemObj, "group_ip", &tObj);
              if (NULL == tObj)
                {
                  NSOPR_LOGERR ("No group_IP");
                  free (mcID);
                  mcID = NULL;
                  goto RETURN_ERROR;
                }

              tStr = json_object_get_string (tObj);
              if (NULL == tStr)
                {
                  NSOPR_LOGERR ("Get Multiple cast group IP Failed");
                  free (mcID);
                  mcID = NULL;
                  goto RETURN_ERROR;
                }

              ret = spl_inet_aton (tStr, &addr);
              if (0 == ret)
                {
                  NSOPR_LOGERR ("Parse group IP Failed");
                  free (mcID);
                  mcID = NULL;
                  goto RETURN_ERROR;
                }

              mcID->ip = addr.s_addr;
              add_multilcast_ip (port, mcID);
            }
        }
    }

  const char *port_json = json_object_get_string (port_obj);
  if ((NULL == port_json) || (0 == strlen (port_json)))
    {
      NSOPR_LOGERR ("json_object_get_string failed");
      goto RETURN_ERROR;
    }

  port->buffer = malloc_port_buffer ();
  if (!port->buffer)
    {
      goto RETURN_ERROR;
    }

  retval =
    STRCPY_S (get_port_json (port), IP_MODULE_PORT_JSON_LEN, port_json);
  if (EOK != retval)
    {
      NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
      goto RETURN_ERROR;
    }

  return port;
RETURN_ERROR:
  free_container_port (port, IP_MODULE_TRUE);
  return NULL;
}

struct container_ip *
parse_container_ip_json (char *param)
{
  int retval;
  struct json_object *obj = json_tokener_parse (param);
  struct json_object *container_id_obj = NULL;
  struct json_object *ports_list_obj = NULL;

  if (!obj)
    {
      return NULL;
    }

  struct container_ip *container = malloc (sizeof (struct container_ip));
  if (container == NULL)
    {
      json_object_put (obj);
      return NULL;
    }

  retval =
    MEMSET_S (container, sizeof (struct container_ip), 0,
              sizeof (struct container_ip));
  if (EOK != retval)
    {
      NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retval);
      goto RETURN_ERROR;
    }

  json_object_object_get_ex (obj, "containerID", &container_id_obj);
  if (container_id_obj)
    {
      const char *container_id = json_object_get_string (container_id_obj);
      if ((container_id == NULL) || (container_id[0] == 0)
          || (strlen (container_id) >= IP_MODULE_MAX_NAME_LEN))
        {
          goto RETURN_ERROR;
        }

      retval =
        MEMSET_S (container->container_id, sizeof (container->container_id),
                  0, sizeof (container->container_id));
      if (EOK != retval)
        {
          NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retval);
          goto RETURN_ERROR;
        }

      retval =
        STRCPY_S (container->container_id, sizeof (container->container_id),
                  container_id);

      if (EOK != retval)
        {
          NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
          goto RETURN_ERROR;
        }
    }
  else
    {
      /* this mandatory parameter */
      goto RETURN_ERROR;
    }

  json_object_object_get_ex (obj, "ports_list", &ports_list_obj);
  if (ports_list_obj)
    {
      int i;
      int port_num = json_object_array_length (ports_list_obj);

      if (port_num == 0)
        {
          /* this mandatory parameter */
          goto RETURN_ERROR;
        }

      for (i = 0; i < port_num; i++)
        {
          struct json_object *port_obj =
            json_object_array_get_idx (ports_list_obj, i);
          struct container_port *port = parse_port_obj (port_obj);
          if (!port)
            {
              goto RETURN_ERROR;
            }

          add_port (container, port);
        }
    }
  else
    {
      /* mandatory parameter */
      goto RETURN_ERROR;
    }

  /* Check if this function is required, or needs more check inside this function,
     as some of them are alraedy validated
   */
  if (!is_container_ok (container))
    {
      goto RETURN_ERROR;
    }

  json_object_put (obj);
  return container;

RETURN_ERROR:
  json_object_put (obj);
  free_container (container, IP_MODULE_TRUE);
  return NULL;
}

bool_t
is_ip_match_netif (unsigned int ip, char *netif_name)
{
  if (!netif_name)
    {
      return 0;
    }

  if (trp_rb_search ((void *) (u64_t) ip, &g_container_ip_root, ip_compare))
    {
      struct network_configuration *network =
        get_network_by_ip_with_tree (ip);
      if (network && network->phy_net && network->phy_net->header)
        {
          if (0 ==
              strncmp (netif_name, network->phy_net->header->nic_name,
                       HAL_MAX_NIC_NAME_LEN))
            {
              return 1;
            }
        }
    }

  return 0;
}

inline bool_t
is_ip_exist (unsigned int ip)
{
  if (trp_rb_search ((void *) (u64_t) ip, &g_container_ip_root, ip_compare))
    {
      return 1;
    }

  return 0;
}

int
validate_addcontainerconfig (struct container_ip *container)
{
  struct container_port *port;
  struct container_ip *old =
    get_container_by_container_id (container->container_id);
  struct container_port *tmp = container->ports_list;

  if (old)
    {
      struct container_port *last = NULL;

      while (tmp)
        {
          if (get_port (container->container_id, tmp->port_name))
            {
              NSOPR_LOGERR ("port=%s already exists!", tmp->port_name);
              return NSCRTL_RD_EXIST;
            }

          last = tmp;
          tmp = tmp->next;
        }

      (void) last;
    }

  /* check if port_name duplicates in one json configuration */
  tmp = container->ports_list;
  while (tmp)
    {
      if (get_port_from_container (tmp))
        {
          NSOPR_LOGERR ("port=%s duplicates!", tmp->port_name);
          return NSCRTL_RD_EXIST;
        }

      tmp = tmp->next;
    }

  bool_t is_nstack_dpdk_port;
  struct container_port **ref = &container->ports_list;
  while ((port = *ref))
    {
      is_nstack_dpdk_port = 1;
      struct container_port_ip_cidr *ip_cidr = port->ip_cidr_list;
      while (ip_cidr)
        {
          struct network_configuration *network =
            get_network_by_ip_with_tree (ip_cidr->ip);
          if (network && (0 == strcmp (network->type_name, "nstack-dpdk")))
            {
              struct netif *pnetif;
              if (get_netif_by_ip (ip_cidr->ip))
                {
                  NSOPR_LOGERR ("ip  exists]IP=0x%08x", ip_cidr->ip);
                  return NSCRTL_RD_EXIST;
                }

              if (network->phy_net->bond_name[0] != 0)
                {
                  pnetif =
                    find_netif_by_if_name (network->phy_net->bond_name);
                }
              else
                {
                  pnetif =
                    find_netif_by_if_name (network->phy_net->
                                           header->nic_name);
                }

              if (!pnetif)
                {
                  NSOPR_LOGERR ("can't find netif, network json:%s",
                                get_network_json (network));
                  return NSCRTL_ERR;
                }

              if (0 == port->port_name[0])
                {
                  NSOPR_LOGINF
                    ("ip=0x%08x is in nstack dpdk network, but port_name is null, json:%s",
                     ip_cidr->ip, get_port_json (port));
                  is_nstack_dpdk_port = 0;
                  break;
                }
            }
          else
            {
              NSOPR_LOGINF ("port %s is not in nstack dpdk network, json:%s",
                            port->port_name, get_port_json (port));
              is_nstack_dpdk_port = 0;
              break;
            }

          ip_cidr = ip_cidr->next;
        }

      /* only use nstack dpdk port */
      if (is_nstack_dpdk_port)
        {
          ref = &port->next;
        }
      else
        {
          *ref = port->next;
          port->next = NULL;
          free_container_port (port, IP_MODULE_TRUE);
        }
    }

  return (!container->ports_list) ? NSCRTL_FREE_ALL_PORT : NSCRTL_OK;
}

/* get the num of IPs in a container , which  in a certain subnet */
extern struct network_list g_network_list;
extern inline int is_in_subnet (unsigned int ip, struct ip_subnet *subnet);
NSTACK_STATIC inline int get_network_ip_count
  (struct container_ip *container, struct ip_subnet *subnet)
{
  int ip_count = 0;
  struct container_port *port_list = NULL;
  struct container_ip *ci = container;

  while (ci)
    {
      port_list = ci->ports_list;
      while (port_list)
        {
          struct container_port_ip_cidr *ip_list = port_list->ip_cidr_list;
          while (ip_list)
            {
              if (!is_in_subnet (ip_list->ip, subnet))
                {
                  ip_count++;
                }

              ip_list = ip_list->next;
            }

          port_list = port_list->next;
        }

      ci = ci->next;
    }

  return ip_count;
}

int
check_ip_count (struct container_ip *container)
{
  int cur_count = 0;
  int new_count = 0;

  if (NULL == container)
    {
      return 1;
    }

  struct network_configuration *network = g_network_list.header;
  while (network)
    {
      cur_count =
        get_network_ip_count (g_container_list.header, network->ip_subnet);
      new_count = get_network_ip_count (container, network->ip_subnet);

      if ((cur_count > MAX_NETWORK_IP_COUNT)
          || (new_count > MAX_NETWORK_IP_COUNT)
          || (cur_count + new_count > MAX_NETWORK_IP_COUNT))
        {
          NSOPR_LOGERR
            ("reach ip addr max count]network=%s, max=%d, current=%d, new=%d.",
             network->network_name, MAX_NETWORK_IP_COUNT, cur_count,
             new_count);
          return 0;
        }

      network = network->next;
    }

  return 1;
}

int
match_groupaddr (struct container_multicast_id *multi_list,
                 spl_ip_addr_t * groupaddr)
{
  struct container_multicast_id *group_info = multi_list;

  while (group_info)
    {
      if (group_info->ip == groupaddr->addr)
        {
          return 1;
        }

      group_info = group_info->next;
    }

  return 0;
}

int
add_container (struct container_ip *container)
{
  int retVal = 0;

  /* need to check if any of the netif operation failed, then we should return fail */
  retVal = validate_addcontainerconfig (container);
  if (retVal != NSCRTL_OK)
    {
      free_container (container, IP_MODULE_TRUE);
      return (NSCRTL_FREE_ALL_PORT == retVal) ? NSCRTL_OK : retVal;
    }

  /* control max network and ipaddress count */
  if (!check_ip_count (container))
    {
      free_container (container, IP_MODULE_TRUE);
      return NSCRTL_IP_COUNT_EXCEED;
    }

  struct container_port *last = NULL;
  struct container_ip *old =
    get_container_by_container_id (container->container_id);
  if (old)
    {
      struct container_port *tmp = container->ports_list;
      while (tmp)
        {
          /* here we don't need to check "if tmp->port_name == NULL", as validate_addcontainerconfig() has done this. */
          if (get_port (container->container_id, tmp->port_name))
            {
              free_container (container, IP_MODULE_TRUE);
              NSOPR_LOGERR ("port exist!");
              return NSCRTL_RD_EXIST;
            }

          last = tmp;
          tmp = tmp->next;
        }
    }
  else
    {
      container->next = g_container_list.header;
      g_container_list.header = container;
    }

  output_api *api = get_output_api ();
  struct container_port *port = container->ports_list;
  while (port)
    {
      struct container_port_ip_cidr *ip_cidr = port->ip_cidr_list;
      while (ip_cidr)
        {
          if (api->add_netif_ip)
            {
              struct network_configuration *network =
                get_network_by_ip_with_tree (ip_cidr->ip);
              if (network)
                {
                  unsigned int mask = ~0;
                  mask =
                    (mask << (IP_MODULE_SUBNET_MASK_LEN - ip_cidr->mask_len));
                  mask = spl_htonl (mask);
                  if (network->phy_net->bond_name[0] != 0)
                    {
                      (void) api->add_netif_ip (network->phy_net->bond_name, ip_cidr->ip, mask);        //no need to check return value, validate_addcontainerconfig() has been checked parameters.
                    }
                  else
                    {
                      (void) api->add_netif_ip (network->phy_net->
                                                header->nic_name, ip_cidr->ip,
                                                mask);
                    }
                }
              else
                {
                  NSOPR_LOGERR ("can't find network by]IP=%u,port_name=%s",
                                ip_cidr->ip, port->port_name);
                }
            }

          retVal =
            trp_rb_insert ((void *) (u64_t) ip_cidr->ip, (void *) port,
                           &g_container_ip_root, ip_compare);

          if (0 != retVal)
            {
              NSOPR_LOGERR ("trp_rb_insert failed]ip_cidr->ip=%u",
                            ip_cidr->ip);
            }

          ip_cidr = ip_cidr->next;
        }
      port = port->next;
    }

  if (old)
    {
      if (last)
        {
          last->next = old->ports_list;
          old->ports_list = container->ports_list;
        }

      container->ports_list = NULL;
      free_container (container, IP_MODULE_FALSE);
    }

  return NSCRTL_OK;
}

struct container_ip *
get_container_by_container_id (char *container_id)
{
  if (NULL == container_id)
    {
      NSOPR_LOGERR ("Param input container ID is NULL");
      return NULL;
    }

  struct container_ip *container = g_container_list.header;
  while (container)
    {
      if (0 == strcmp (container->container_id, container_id))
        {
          return container;
        }

      container = container->next;
    }

  return NULL;
}

/*****************************************************************************
*   Prototype    : getIpCfgAll
*   Description  : Get All ip configurations
*   Input        : char *
*                     size_t
*   Output       : char * patern:[***,***,***]
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
getIpCfgAll (char *jsonBuf, size_t size)
{
  int retval;

  if (NULL == jsonBuf)
    {
      return NSCRTL_ERR;
    }

  if (size < 2)
    {
      NSOPR_LOGERR ("get all ip cfg error, buffer is not enough.");
      return NSCRTL_STATUS_ERR;
    }

  char bfirstData = 1;
  *jsonBuf = '[';
  jsonBuf = jsonBuf + 1;

  /*need another two char to keep [and ] */
  size_t len = size - 2;
  size_t strsize = 0;
  struct container_port *port = NULL;
  struct container_ip *container = g_container_list.header;
  while (container)
    {
      port = container->ports_list;
      while (port)
        {
          if (NULL == port->buffer)
            {
              port = port->next;
              continue;
            }

          strsize = strlen (get_port_json (port)) + 1;

          /*always reserve 1 char */
          if ((strsize > 0) && (strsize < len))
            {
              if (bfirstData)
                {
                  bfirstData = 0;
                }
              else
                {
                  *jsonBuf = ',';
                  jsonBuf = jsonBuf + 1;
                  len = len - 1;
                }

              retval = STRCPY_S (jsonBuf, len, get_port_json (port));
              if (EOK != retval)
                {
                  NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
                  return NSCRTL_ERR;
                }

              len = len - strlen (get_port_json (port));
              jsonBuf = jsonBuf + strlen (get_port_json (port));
            }
          else
            {
              NSOPR_LOGERR ("get all ip cfg error, buffer is not enough.");
              return NSCRTL_STATUS_ERR;
            }

          port = port->next;
        }

      container = container->next;
    }

  *jsonBuf = ']';
  return 0;
}

int
del_port (char *container_id, char *port_name)
{
  struct container_port *port = NULL;
  struct container_port **ref = NULL;
  struct container_ip *container = NULL;
  struct container_ip **container_ref = &g_container_list.header;

  while ((container = *container_ref))
    {
      NSOPR_LOGDBG ("container->container_id=%s,container_id=%p",
                    container->container_id, container_id);
      if (strcmp (container->container_id, container_id) == 0)
        {
          ref = &container->ports_list;
          while ((port = *ref))
            {
              if (strcmp (port_name, port->port_name) == 0)
                {
                  *ref = port->next;
                  port->next = NULL;
                  free_container_port (port, IP_MODULE_FALSE);
                  return 0;
                }

              ref = &port->next;
            }

          break;
        }

      container_ref = &container->next;
    }

  return NSCRTL_RD_NOT_EXIST;
}

struct container_port *
get_port (char *container_id, char *port_name)
{
  struct container_port *port = NULL;
  struct container_ip *container = g_container_list.header;

  while (container)
    {
      if (strcmp (container->container_id, container_id) == 0)
        {
          port = container->ports_list;
          while (port)
            {
              if (strcmp (port_name, port->port_name) == 0)
                {
                  return port;
                }

              port = port->next;
            }
        }

      container = container->next;
    }

  return NULL;
}

struct container_port *
get_port_from_container (struct container_port *port)
{
  char *port_name = port->port_name;
  struct container_port *tmp = port->next;

  while (tmp)
    {
      if (strcmp (port_name, tmp->port_name) == 0)
        {
          return tmp;
        }

      tmp = tmp->next;
    }

  return NULL;
}
