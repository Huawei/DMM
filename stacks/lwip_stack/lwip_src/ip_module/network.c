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

#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include "json.h"
#include "trp_rb_tree.h"
#include "network.h"
#include "nstack_log.h"
#include "config_common.h"
#include "stackx_spl_share.h"
#include "stackx/spl_api.h"
#include "sharedmemory.h"
#include "nstack_securec.h"
#include "nstack_rd_mng.h"
#include "spl_hal.h"
#include "inet.h"

struct network_list g_network_list = { 0 };

extern struct stackx_port_zone *p_stackx_port_zone;
extern u32 spl_hal_is_nic_exist (const char *name);

static bool_t
is_phy_net_ok (struct phy_net *pst_phy_net)
{
  if (!pst_phy_net || !pst_phy_net->header)
    {
      NSOPR_LOGERR ("phy_net is not ok");
      return 0;
    }

  return 1;
}

static bool_t
is_network_configuration_ok (struct network_configuration *network)
{
  while (network)
    {
      if (!is_phy_net_ok (network->phy_net))
        {
          return 0;
        }

      network = network->next;
    }

  return 1;
}

static void
add_ref_nic (struct phy_net *pst_phy_net, struct ref_nic *pst_ref_nic)
{
  pst_ref_nic->next = pst_phy_net->header;
  pst_phy_net->header = pst_ref_nic;
}

static void
free_ref_nic (struct ref_nic *pst_ref_nic)
{
  struct ref_nic *nic = pst_ref_nic;
  struct ref_nic *tmp = NULL;

  while (nic)
    {
      tmp = nic;
      nic = tmp->next;
      free (tmp);
    }
}

static void
free_phy_net (struct phy_net *pst_phy_net)
{
  if (pst_phy_net)
    {
      free_ref_nic (pst_phy_net->header);
      free (pst_phy_net);
    }
}

static void
free_ip_subnet (struct ip_subnet *subnet, bool_t only_free)
{
  struct ip_subnet *tmp = NULL;

  while (subnet)
    {
      tmp = subnet;
      subnet = subnet->next;
      free (tmp);
    }
}

void
free_network_configuration (struct network_configuration *network,
                            bool_t only_free)
{
  if (network)
    {
      free_ip_subnet (network->ip_subnet, only_free);
      free_phy_net (network->phy_net);

      if (network->buffer)
        {
          free_network_buffer (network->buffer);
          network->buffer = NULL;
        }

      free (network);
    }
}

inline int
is_in_subnet (unsigned int ip, struct ip_subnet *subnet)
{
  unsigned int mask = ~0;
  unsigned int seg_ip, seg;

  mask = mask << (IP_MODULE_SUBNET_MASK_LEN - subnet->mask_len);

  seg_ip = ip & mask;
  seg = subnet->subnet & mask;

  return seg_ip != seg ? 1 : 0;
}

inline struct network_configuration *
get_network_by_ip_with_tree (unsigned int ip)
{
  unsigned int h_ip = spl_ntohl (ip);
  struct network_configuration *p = g_network_list.header;

  while (p)
    {
      if (!is_in_subnet (h_ip, p->ip_subnet))
        {
          return p;
        }

      p = p->next;
    }

  return NULL;
}

struct network_configuration *
get_network_by_name (char *name)
{
  struct network_configuration *network = g_network_list.header;

  while (network)
    {
      if (strcasecmp (name, network->network_name) == 0)
        {
          return network;
        }

      network = network->next;
    }

  return NULL;
}

struct network_configuration *
get_network_by_nic_name (char *name)
{
  if (NULL == name)
    {
      return NULL;
    }

  struct ref_nic *refnic = NULL;
  struct phy_net *phynet = NULL;
  struct network_configuration *network = g_network_list.header;
  while (network)
    {
      phynet = network->phy_net;
      if (phynet)
        {
          if ((phynet->bond_mode != -1)
              && strcmp (name, phynet->bond_name) == 0)
            {
              return network;
            }

          refnic = phynet->header;
          while (refnic)
            {
              if (strcmp (name, refnic->nic_name) == 0)
                {
                  return network;
                }

              refnic = refnic->next;
            }
        }

      network = network->next;
    }

  return NULL;
}

struct network_configuration *
get_network_by_name_from_json (struct network_configuration *network)
{
  char *name = network->network_name;
  struct network_configuration *tmp = network->next;

  while (tmp)
    {
      if (strcasecmp (name, tmp->network_name) == 0)
        {
          return tmp;
        }

      tmp = tmp->next;
    }

  return NULL;
}

NSTACK_STATIC inline int
get_network_count ()
{
  int count = 0;
  struct network_configuration *network = g_network_list.header;

  while (network)
    {
      count++;
      network = network->next;
    }

  return count;
}

int
get_network_all (char *jsonBuf, size_t size)
{
  if (NULL == jsonBuf)
    {
      return NSCRTL_ERR;
    }

  if (size < 2)
    {
      NSOPR_LOGERR ("get all ip cfg error, buffer is not enough.");
      return NSCRTL_STATUS_ERR;
    }

  int retVal;
  char bfirstData = 1;
  *jsonBuf = '[';
  jsonBuf = jsonBuf + 1;

  size_t len = size - 2;        // strlen("[]")
  size_t strsize = 0;
  struct network_configuration *network = g_network_list.header;
  while (network)
    {
      if (NULL == network->buffer)
        {
          network = network->next;
          continue;
        }

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

      strsize = strlen (get_network_json (network)) + 1;
      if ((strsize > 0) && (strsize < len))
        {
          retVal = STRCPY_S (jsonBuf, len, get_network_json (network));
          if (EOK != retVal)
            {
              NSOPR_LOGERR ("STRCPY_S failed]ret=%d.", retVal);
              return NSCRTL_STATUS_ERR;
            }

          len = len - strlen (get_network_json (network));
          jsonBuf = jsonBuf + strlen (get_network_json (network));
        }
      else
        {
          NSOPR_LOGERR ("get all network cfg error, buffer is not enough.");
          return NSCRTL_STATUS_ERR;
        }

      network = network->next;
    }

  *jsonBuf = ']';
  return 0;
}

int
nic_already_bind (const char *nic_name)
{
  struct ref_nic *pnic = NULL;
  struct network_configuration *pnetwork = g_network_list.header;

  while (pnetwork)
    {
      pnic = pnetwork->phy_net->header;
      while (pnic)
        {
          if (0 == strcmp (pnic->nic_name, nic_name))
            {
              return 1;
            }

          pnic = pnic->next;
        }

      pnetwork = pnetwork->next;
    }

  return 0;
}

int
nic_already_init (const char *nic_name)
{
  unsigned int i;

  for (i = 0; i < p_stackx_port_zone->port_num; i++)
    {
      if (0 ==
          strcmp (p_stackx_port_zone->stackx_one_port[i].linux_ip.if_name,
                  nic_name))
        {
          return 1;
        }
    }

  return 0;
}

extern struct stackx_port_info *head_used_port_list;
int
bonded_nic_already_bind (struct network_configuration *pnetwork,
                         const char *nic_name)
{
  struct stackx_port_info *p_port_list = head_used_port_list;

  while (p_port_list)
    {
      if (0 == strcmp (p_port_list->linux_ip.if_name, nic_name))
        {
          return 1;
        }

      p_port_list = p_port_list->next_use_port;
    }

  struct ref_nic *pnic = NULL;

  while (pnetwork)
    {
      if (pnetwork->phy_net->bond_mode == -1)
        {
          pnic = pnetwork->phy_net->header;
          if (0 == strcmp (pnic->nic_name, nic_name))
            {
              return 1;
            }
        }

      pnetwork = pnetwork->next;
    }

  return 0;
}

extern struct bond_ports_info bond_ports_array;
int
nic_already_bond (struct network_configuration *pnetwork,
                  const char *nic_name)
{
  u8_t i, j;
  struct bond_set *s;

  for (i = 0; i < bond_ports_array.cnt; i++)
    {
      s = &bond_ports_array.ports[i];
      for (j = 0; j < s->slave_port_cnt; j++)
        {
          if (strcmp (s->slave_ports[j], nic_name) == 0)
            {
              return 1;
            }
        }
    }

  struct ref_nic *pnic = NULL;

  while (pnetwork)
    {
      if (pnetwork->phy_net->bond_mode != -1)
        {
          pnic = pnetwork->phy_net->header;
          while (pnic)
            {
              if (0 == strcmp (pnic->nic_name, nic_name))
                {
                  return 1;
                }

              pnic = pnic->next;
            }
        }

      pnetwork = pnetwork->next;
    }

  return 0;
}

/*add network to rd*/
void
network_rd_proc (struct network_configuration *network, int op)
{
  struct ip_subnet *ptsubnet = NULL;
  struct network_configuration *pn = NULL;
  rd_ip_data rd_ip = { 0 };
  int ret = 0;

  pn = network;

  while (pn)
    {
      if (0 == strcmp ("nstack-dpdk", pn->type_name))
        {
          ptsubnet = network->ip_subnet;
          while (ptsubnet)
            {
              rd_ip.addr = ptsubnet->subnet;
              rd_ip.masklen = ptsubnet->mask_len;
              if (0 == op)
                {
                  ret = nstack_rd_ip_node_insert (network->type_name, &rd_ip);
                }
              else
                {
                  ret = nstack_rd_ip_node_delete (&rd_ip);
                }

              if (0 != ret)
                {
                  NSOPR_LOGERR ("nstack rd subnet:0x%x, masklen:0x%x %s fail",
                                rd_ip.addr, rd_ip.masklen,
                                (0 == op ? "insert" : "delete"));
                }
              else
                {
                  NSOPR_LOGDBG
                    ("nstack rd subnet:0x%x, masklen:0x%x %s success",
                     rd_ip.addr, rd_ip.masklen,
                     (0 == op ? "insert" : "delete"));
                }

              ptsubnet = ptsubnet->next;
            }
        }

      pn = pn->next;
    }

  return;
}

/* add network to list in descending sort */
void
add_network_to_list (struct network_configuration *network)
{
  struct network_configuration *curr = g_network_list.header;
  struct network_configuration *prev = NULL;

  network->next = NULL;

  /*add network to rd */
  network_rd_proc (network, 0);

  while (curr)
    {
      if (network->ip_subnet->mask_len >= curr->ip_subnet->mask_len)
        {
          break;
        }

      prev = curr;
      curr = curr->next;
    }

  if (NULL == prev)
    {
      network->next = curr;
      g_network_list.header = network;
    }
  else
    {
      network->next = prev->next;
      prev->next = network;
    }
}

int
add_network_configuration (struct network_configuration
                           *pst_network_configuration)
{
  struct network_configuration *tmp = pst_network_configuration;
  struct ref_nic *pheader;

  if (tmp)
    {
      if (get_network_by_name (tmp->network_name)
          || get_network_by_name_from_json (tmp))
        {
          NSOPR_LOGERR ("network exists or duplicates]network_name=%s",
                        tmp->network_name);
          free_network_configuration (pst_network_configuration,
                                      IP_MODULE_TRUE);
          return NSCRTL_RD_EXIST;
        }

      if (strcasecmp ("nstack-dpdk", tmp->type_name) != 0)
        {
          NSOPR_LOGWAR ("illegal network type]type_name=%s", tmp->type_name);
        }

      /* control max network and ipaddress count */
      if (get_network_count () >= MAX_NETWORK_COUNT)
        {
          NSOPR_LOGERR ("network reach]max_count=%d", MAX_NETWORK_COUNT);
          free_network_configuration (pst_network_configuration,
                                      IP_MODULE_TRUE);
          return NSCRTL_NETWORK_COUNT_EXCEED;
        }

      /* If nic is not existed or not initiated, return error */
      pheader = tmp->phy_net->header;
      while (pheader)
        {
          if (!spl_hal_is_nic_exist (pheader->nic_name)
              && !nic_already_init (pheader->nic_name))
            {
              NSOPR_LOGERR ("Invalid configuration %s not exist Error! ",
                            pheader->nic_name);
              free_network_configuration (pst_network_configuration,
                                          IP_MODULE_TRUE);
              return NSCRTL_RD_NOT_EXIST;
            }

          pheader = pheader->next;
        }

      /* if a bonded nic has been inited in a non-bond network, return error */
      if (tmp->phy_net->bond_mode != -1)
        {
          pheader = tmp->phy_net->header;
          while (pheader)
            {
              if (bonded_nic_already_bind
                  (pst_network_configuration, pheader->nic_name))
                {
                  NSOPR_LOGERR ("Invalid configuration %s already bind! ",
                                pheader->nic_name);
                  free_network_configuration (pst_network_configuration,
                                              IP_MODULE_TRUE);
                  return NSCRTL_INPUT_ERR;
                }

              pheader = pheader->next;
            }
        }

      /* if a non-bond nic has been inited in a bonded network, return error */
      if (tmp->phy_net->bond_mode == -1)
        {
          pheader = tmp->phy_net->header;
          while (pheader)
            {
              if (nic_already_bond
                  (pst_network_configuration, pheader->nic_name))
                {
                  NSOPR_LOGERR ("Invalid configuration %s already bind! ",
                                pheader->nic_name);
                  free_network_configuration (pst_network_configuration,
                                              IP_MODULE_TRUE);
                  return NSCRTL_INPUT_ERR;
                }

              pheader = pheader->next;
            }
        }
    }
  else
    {
      NSOPR_LOGERR ("Invalid network configuration!");
      return NSCRTL_INPUT_ERR;
    }

  /*looping through each node has move to read_ipmoduleoperateadd_configuration */
  ip_subnet_print (tmp->ip_subnet);
  add_network_to_list (tmp);

  return NSCRTL_OK;
}

struct network_configuration *
parse_network_obj (struct json_object *network_obj)
{
  int retVal;
  struct network_configuration *pst_network_configuration = NULL;
  struct json_object *network_name_obj = NULL, *args_obj = NULL, *phy_obj =
    NULL, *type_name_obj = NULL;
  struct json_object *ref_nic_list_obj = NULL, *bond_mode_obj =
    NULL, *bond_name_obj = NULL, *ipam_obj = NULL;
  struct json_object *subnet_obj = NULL;

  if (!network_obj)
    {
      NSOPR_LOGERR ("network_obj is null");
      goto RETURN_ERROR;
    }

  pst_network_configuration = malloc (sizeof (struct network_configuration));
  if (NULL == pst_network_configuration)
    {
      NSOPR_LOGERR ("malloc failed");
      goto RETURN_ERROR;
    }

  retVal =
    MEMSET_S (pst_network_configuration,
              sizeof (struct network_configuration), 0,
              sizeof (struct network_configuration));
  if (EOK != retVal)
    {
      NSOPR_LOGERR ("MEMSET_S failed]ret=%d.", retVal);
      goto RETURN_ERROR;
    }

  json_object_object_get_ex (network_obj, "name", &network_name_obj);
  if (network_name_obj)
    {
      const char *network_name = json_object_get_string (network_name_obj);
      if ((NULL == network_name) || (network_name[0] == 0)
          || (strlen (network_name) >= IP_MODULE_MAX_NAME_LEN))
        {
          NSOPR_LOGERR ("network_name is not ok");
          goto RETURN_ERROR;
        }

      retVal =
        STRCPY_S (pst_network_configuration->network_name,
                  sizeof (pst_network_configuration->network_name),
                  network_name);

      if (EOK != retVal)
        {
          NSOPR_LOGERR ("STRCPY_S failed]ret=%d.", retVal);
          goto RETURN_ERROR;
        }
    }
  else
    {
      NSOPR_LOGERR ("name is not ok");
      goto RETURN_ERROR;
    }

  json_object_object_get_ex (network_obj, "type", &type_name_obj);
  if (type_name_obj)
    {
      const char *type_name = json_object_get_string (type_name_obj);
      if ((NULL == type_name) || (type_name[0] == 0)
          || (strlen (type_name) >= IP_MODULE_MAX_NAME_LEN))
        {
          NSOPR_LOGERR ("type parse error");
          goto RETURN_ERROR;
        }

      retVal =
        STRCPY_S (pst_network_configuration->type_name,
                  sizeof (pst_network_configuration->type_name), type_name);

      if (EOK != retVal)
        {
          NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retVal);
          goto RETURN_ERROR;
        }
    }
  else
    {
      NSOPR_LOGERR ("type is not ok");
      goto RETURN_ERROR;
    }

  json_object_object_get_ex (network_obj, "args", &args_obj);
  if (args_obj)
    {
      json_object_object_get_ex (args_obj, "phynet", &phy_obj);
      if (phy_obj)
        {
          struct phy_net *pst_phy_net = malloc (sizeof (struct phy_net));
          if (NULL == pst_phy_net)
            {
              NSOPR_LOGERR ("malloc failed");
              goto RETURN_ERROR;
            }

          retVal =
            MEMSET_S (pst_phy_net, sizeof (struct phy_net), 0,
                      sizeof (struct phy_net));
          if (EOK != retVal)
            {
              NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retVal);
              free_phy_net (pst_phy_net);
              goto RETURN_ERROR;
            }

          json_object_object_get_ex (phy_obj, "ref_nic", &ref_nic_list_obj);
          if (ref_nic_list_obj)
            {
              int j;
              int ref_nic_num = json_object_array_length (ref_nic_list_obj);
              if (0 == ref_nic_num)
                {
                  NSOPR_LOGERR ("ref_nic is empty");
                  free_phy_net (pst_phy_net);
                  goto RETURN_ERROR;
                }

              for (j = ref_nic_num - 1; j >= 0; j--)
                {
                  struct json_object *ref_nic_obj =
                    json_object_array_get_idx (ref_nic_list_obj, j);
                  if (ref_nic_obj)
                    {
                      const char *nic_name =
                        json_object_get_string (ref_nic_obj);
                      if ((NULL == nic_name) || (nic_name[0] == 0)
                          || (strlen (nic_name) >= HAL_MAX_NIC_NAME_LEN))
                        {
                          NSOPR_LOGERR ("nic_name is not ok");
                          free_phy_net (pst_phy_net);
                          goto RETURN_ERROR;
                        }

                      struct ref_nic *pst_ref_nic =
                        malloc (sizeof (struct ref_nic));
                      if (NULL == pst_ref_nic)
                        {
                          NSOPR_LOGERR ("malloc failed");
                          free_phy_net (pst_phy_net);
                          goto RETURN_ERROR;
                        }

                      retVal =
                        MEMSET_S (pst_ref_nic, sizeof (struct ref_nic), 0,
                                  sizeof (struct ref_nic));
                      if (EOK != retVal)
                        {
                          NSOPR_LOGERR ("MEMSET_S failed]ret=%d.", retVal);
                          free (pst_ref_nic);
                          pst_ref_nic = NULL;
                          free_phy_net (pst_phy_net);
                          goto RETURN_ERROR;
                        }

                      retVal =
                        STRCPY_S (pst_ref_nic->nic_name,
                                  sizeof (pst_ref_nic->nic_name), nic_name);

                      if (EOK != retVal)
                        {
                          NSOPR_LOGERR ("STRCPY_S failed]ret=%d.", retVal);
                          free (pst_ref_nic);
                          free_phy_net (pst_phy_net);
                          goto RETURN_ERROR;
                        }

                      add_ref_nic (pst_phy_net, pst_ref_nic);
                    }
                }
            }
          else
            {
              NSOPR_LOGERR ("ref_nic is not ok");
              free_phy_net (pst_phy_net);
              goto RETURN_ERROR;
            }

          json_object_object_get_ex (phy_obj, "bond_mode", &bond_mode_obj);
          if (bond_mode_obj)
            {
              pst_phy_net->bond_mode = json_object_get_int (bond_mode_obj);
              if (pst_phy_net->bond_mode != -1)
                {
                  json_object_object_get_ex (phy_obj, "bond_name",
                                             &bond_name_obj);
                  if (bond_name_obj)
                    {
                      const char *bond_name =
                        json_object_get_string (bond_name_obj);
                      if ((NULL == bond_name)
                          || (strlen (bond_name) >= IP_MODULE_MAX_NAME_LEN))
                        {
                          NSOPR_LOGERR ("bond_name is not ok");
                          free_phy_net (pst_phy_net);
                          goto RETURN_ERROR;
                        }

                      retVal =
                        MEMSET_S (pst_phy_net->bond_name,
                                  sizeof (pst_phy_net->bond_name), 0,
                                  sizeof (pst_phy_net->bond_name));

                      if (EOK != retVal)
                        {
                          NSOPR_LOGERR ("MEMSET_S failed]ret=%d.", retVal);
                          free_phy_net (pst_phy_net);
                          goto RETURN_ERROR;
                        }

                      retVal =
                        STRNCPY_S (pst_phy_net->bond_name,
                                   sizeof (pst_phy_net->bond_name), bond_name,
                                   strlen (bond_name));

                      if (EOK != retVal)
                        {
                          NSOPR_LOGERR ("STRNCPY_S failed]retVal=%d", retVal);
                          free_phy_net (pst_phy_net);
                          goto RETURN_ERROR;
                        }
                    }
                  else
                    {
                      NSOPR_LOGERR ("bond_name is not ok");
                      free_phy_net (pst_phy_net);
                      goto RETURN_ERROR;
                    }
                }
            }
          else
            {
              pst_phy_net->bond_mode = -1;
            }

          pst_network_configuration->phy_net = pst_phy_net;
        }
      else
        {
          NSOPR_LOGERR ("phy_net is not ok");
          goto RETURN_ERROR;
        }
    }
  else
    {
      NSOPR_LOGERR ("args is not ok");
      goto RETURN_ERROR;
    }

  json_object_object_get_ex (network_obj, "ipam", &ipam_obj);
  if (ipam_obj)
    {
      json_object_object_get_ex (ipam_obj, "subnet", &subnet_obj);
      if (subnet_obj)
        {
          int iRet;
          char tmp[IP_MODULE_LENGTH_32] = { 0 };
          const char *subnet = json_object_get_string (subnet_obj);
          struct in_addr addr;

          retVal = MEMSET_S (&addr, sizeof (addr), 0, sizeof (addr));
          if (EOK != retVal)
            {
              NSOPR_LOGERR ("MEMSET_S failed]ret=%d.", retVal);
              goto RETURN_ERROR;
            }

          if ((NULL == subnet) || (subnet[0] == 0)
              || (strlen (subnet) > IP_MODULE_LENGTH_32))
            {
              NSOPR_LOGERR ("subnet is not ok");
              goto RETURN_ERROR;
            }

          const char *sub = strstr (subnet, "/");
          if ((NULL == sub)
              || (sizeof (tmp) - 1 <= (unsigned int) (sub - subnet))
              || (strlen (sub) >= sizeof (tmp) - 1))
            {
              NSOPR_LOGERR ("subnet is not ok");
              goto RETURN_ERROR;
            }

          iRet =
            STRNCPY_S (tmp, sizeof (tmp), subnet, (size_t) (sub - subnet));
          if (EOK != iRet)
            {
              NSOPR_LOGERR ("STRNCPY_S failed]ret=%d", iRet);
              goto RETURN_ERROR;
            }

          iRet = spl_inet_aton (tmp, &addr);
          if (0 == iRet)
            {
              NSOPR_LOGERR ("subnet is not ok");
              goto RETURN_ERROR;
            }

          pst_network_configuration->ip_subnet =
            (struct ip_subnet *) malloc (sizeof (struct ip_subnet));
          if (!pst_network_configuration->ip_subnet)
            {
              NSOPR_LOGERR ("malloc failed");
              goto RETURN_ERROR;
            }

          retVal =
            MEMSET_S (pst_network_configuration->ip_subnet,
                      sizeof (struct ip_subnet), 0,
                      sizeof (struct ip_subnet));
          if (EOK != retVal)
            {
              NSOPR_LOGERR ("MEMSET_S failed]ret=%d.", retVal);
              goto RETURN_ERROR;
            }

          pst_network_configuration->ip_subnet->next = NULL;
          pst_network_configuration->ip_subnet->subnet =
            spl_ntohl (addr.s_addr);

          iRet = atoi (sub + 1);
          if ((iRet < 0) || (iRet > IP_MODULE_LENGTH_32))
            {
              NSOPR_LOGERR ("mask is not ok");
              goto RETURN_ERROR;
            }

          pst_network_configuration->ip_subnet->mask_len =
            (unsigned int) iRet;
        }
      else
        {
          NSOPR_LOGERR ("subnet is not ok");
          goto RETURN_ERROR;
        }
    }
  else
    {
      NSOPR_LOGERR ("ipam is not ok");
      goto RETURN_ERROR;
    }

  const char *network_json = json_object_get_string (network_obj);
  if ((NULL == network_json) || (network_json[0] == 0))
    {
      NSOPR_LOGERR ("json_object_get_string failed");
      goto RETURN_ERROR;
    }

  pst_network_configuration->buffer = malloc_network_buffer ();
  if (!pst_network_configuration->buffer)
    {
      NSOPR_LOGERR ("malloc_network_buffer failed");
      goto RETURN_ERROR;
    }

  retVal =
    STRCPY_S (get_network_json (pst_network_configuration),
              IP_MODULE_NETWORK_JSON_LEN, network_json);
  if (EOK != retVal)
    {
      NSOPR_LOGERR ("STRCPY_S failed]ret=%d.", retVal);
      goto RETURN_ERROR;
    }

  return pst_network_configuration;

RETURN_ERROR:
  if (pst_network_configuration)
    {
      free_network_configuration (pst_network_configuration, IP_MODULE_TRUE);
      pst_network_configuration = NULL;
    }

  return NULL;
}

struct network_configuration *
parse_network_json (char *param, struct network_configuration *pnetwork_list)
{
  struct json_object *obj = json_tokener_parse (param);
  struct network_configuration *pst_network_configuration = NULL;

  if (!obj)
    {
      NSOPR_LOGERR ("parse error");
      return NULL;
    }

  int network_num = json_object_array_length (obj);
  if (0 == network_num)
    {
      json_object_put (obj);
      return NULL;
    }

  int i;
  for (i = 0; i < network_num; i++)
    {
      struct json_object *network_obj = json_object_array_get_idx (obj, i);
      if (network_obj)
        {
          pst_network_configuration = parse_network_obj (network_obj);
          if (!pst_network_configuration)
            {
              NSOPR_LOGERR ("parse_network_obj error");
              goto RETURN_ERROR;
            }

          pst_network_configuration->next = pnetwork_list;
          pnetwork_list = pst_network_configuration;
          pst_network_configuration = NULL;
        }
      else
        {
          NSOPR_LOGERR ("network_obj is NULL");
          goto RETURN_ERROR;
        }
    }

  if (pnetwork_list)
    {
      if (!is_network_configuration_ok (pnetwork_list))
        {
          NSOPR_LOGERR ("network_configuration is not ok");
          goto RETURN_ERROR;
        }
    }

  json_object_put (obj);
  return pnetwork_list;

RETURN_ERROR:
  free_network_configuration (pnetwork_list, IP_MODULE_TRUE);
  json_object_put (obj);
  return NULL;
}

int
del_network_by_name (char *name)
{
  struct network_configuration *network = NULL;
  struct network_configuration **ref = &g_network_list.header;

  while ((network = *ref))
    {
      if (strcasecmp (name, network->network_name) == 0)
        {
          *ref = network->next;
          network->next = NULL;

          /*delete netework from rd */
          network_rd_proc (network, 1);

          free_network_configuration (network, IP_MODULE_FALSE);
          return 0;
        }

      ref = &network->next;
    }

  return NSCRTL_RD_NOT_EXIST;
}

int
is_in_same_network (unsigned int src_ip, unsigned int dst_ip)
{
  if (src_ip == dst_ip)
    {
      return 1;
    }

  struct network_configuration *src = get_network_by_ip_with_tree (src_ip);
  struct network_configuration *dst = get_network_by_ip_with_tree (dst_ip);
  if (src && (src == dst))
    {
      return 1;
    }

  return 0;
}

struct network_configuration *
get_network_list ()
{
  return g_network_list.header;
}
