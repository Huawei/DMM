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
#include <string.h>
#include "inet.h"
#include "spl_ip_addr.h"
#include "ip_module_api.h"
#include "container_ip.h"
#include "network.h"
#include "config_common.h"
#include "configuration_reader.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "spl_hal.h"
#include "stackx_spl_share.h"
#include "stackx/spl_api.h"
#include "stackx_common.h"
#include "spl_tcpip.h"

static output_api g_output_api = { 0 };

void
regist_output_api (output_api * api)
{
  if (NULL == api)
    {
      NSOPR_LOGERR ("error!!!param api==NULL]");
      return;
    }

  g_output_api = *api;
}

output_api *
get_output_api ()
{
  return &g_output_api;
}

int
process_post (void *arg, ip_module_type Type,
              ip_module_operate_type operate_type)
{
  /* only when add network, in other words:
     only when (IP_MODULE_NETWORK == Type) && (IP_MODULE_OPERATE_ADD == operate_type),
     process_configuration() is called in read_fn thread itself.
     other cases, will post_to tcpip_thread to handle them.

     tips: when add network, it need to post msg(add netif) to tcpip thread and wait a sem,
     but after the msg is handled by tcpip thread, the sem could be post.
     so adding netword is handled in read_fn thread, can't be moved to tcpip thread.

     But we should know, many global and static variables maybe not safe(netif,sc_dpdk etc.),
     because they can be visited by multi-thread. */
  if ((IP_MODULE_NETWORK == Type) && (IP_MODULE_OPERATE_ADD == operate_type))
    {
      return process_configuration (arg, Type, operate_type);
    }

  if (g_output_api.post_to)
    {
      int retval = g_output_api.post_to (arg, Type, operate_type);
      if ((post_ip_module_msg == g_output_api.post_to) && (retval != ERR_OK)
          && (arg != NULL))
        {
          if ((IP_MODULE_IP == Type)
              && (IP_MODULE_OPERATE_ADD == operate_type))
            {
              free_container ((struct container_ip *) arg, IP_MODULE_TRUE);
            }
          else
            {
              free (arg);
            }

          arg = NULL;
          NSOPR_LOGERR ("post failed]ret=%d", retval);
          NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "process_post failed!");
        }

      return retval;
    }
  else
    {
      NSOPR_LOGERR ("g_output_api.post_to==NULL");
      NSOPR_SET_ERRINFO (NSCRTL_ERR,
                         "ERR:internal error, g_output_api.post_to==NULL]");
      return -1;
    }
}

/* arg can be NULL, no need check */
int
process_configuration (void *arg, ip_module_type Type,
                       ip_module_operate_type operate_type)
{
  NSOPR_LOGINF ("precoess begin]arg=%p,action=%d,type=%d", arg, operate_type,
                Type);
  int retval = 0;

  switch (Type)
    {
    case IP_MODULE_NETWORK:
      switch (operate_type)
        {
        case IP_MODULE_OPERATE_ADD:
          retval =
            add_network_configuration ((struct network_configuration *) arg);

          /* When network exceeds max number, just log warning at operation.log */
          if (retval == NSCRTL_NETWORK_COUNT_EXCEED)
            {
              NSOPR_LOGWAR
                ("Warning!! Network count exceed max allowed number]max=%d",
                 MAX_NETWORK_COUNT);
            }
          else
            {
              NSOPR_SET_ERRINFO (retval,
                                 "add_network_configuration return %d",
                                 retval);
            }

          if (!retval)
            {
              /*init DPDK eth */
              if ((retval = init_new_network_configuration ()) != ERR_OK)
                {
                  free_network_configuration ((struct network_configuration *)
                                              arg, IP_MODULE_TRUE);
                  NSOPR_SET_ERRINFO (retval,
                                     "init_new_network_configuration return %d",
                                     retval);
                }
            }
          break;
        case IP_MODULE_OPERATE_DEL:
          {
            retval = del_network_by_name ((char *) arg);
            NSOPR_SET_ERRINFO (retval, "del_network_by_name return %d",
                               retval);
            free (arg);
            arg = NULL;
          }
          break;
        case IP_MODULE_OPERATE_QUERY:
          {
            struct network_configuration *network =
              get_network_by_name ((char *) arg);
            if (!network)
              {
                retval = NSCRTL_RD_NOT_EXIST;
                NSOPR_SET_ERRINFO (retval, "get_network_by_name return %d",
                                   retval);
              }
            else
              {
                if (strlen (get_network_json (network)) >
                    sizeof (get_config_data ()->json_buff) - 1)
                  {
                    NSOPR_LOGERR
                      ("length of network->network_json too big]len=%u",
                       strlen (get_network_json (network)));
                    NSOPR_SET_ERRINFO (NSCRTL_ERR,
                                       "ERR:internal error, buf is not enough]");
                    retval = NSCRTL_ERR;
                  }

                retval =
                  STRCPY_S (get_config_data ()->json_buff,
                            sizeof (get_config_data ()->json_buff),
                            get_network_json (network));
                if (EOK != retval)
                  {
                    NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
                    NSOPR_SET_ERRINFO (NSCRTL_ERR,
                                       "ERR:internal error, STRCPY_S failed]ret=%d",
                                       retval);
                    retval = NSCRTL_ERR;
                  }

              }

            free (arg);
            arg = NULL;
          }
          break;
        default:
          break;
        }

      break;
    case IP_MODULE_IP:
      switch (operate_type)
        {
        case IP_MODULE_OPERATE_ADD:
          retval = add_container ((struct container_ip *) arg);
          NSOPR_SET_ERRINFO (retval, "add_container return %d", retval);
          break;
        case IP_MODULE_OPERATE_DEL:
          {
            struct ip_action_param *p = (struct ip_action_param *) arg;
            retval = del_port (p->container_id, p->port_name);
            NSOPR_SET_ERRINFO (retval, "del_port return %d", retval);
            free (arg);
            arg = NULL;
          }
          break;
        case IP_MODULE_OPERATE_QUERY:
          {
            struct ip_action_param *p = (struct ip_action_param *) arg;
            struct container_port *port =
              get_port (p->container_id, p->port_name);
            if (!port)
              {
                retval = NSCRTL_RD_NOT_EXIST;
                NSOPR_SET_ERRINFO (retval, "get_port return %d", retval);
              }
            else
              {
                if (strlen (get_port_json (port)) >
                    sizeof (get_config_data ()->json_buff) - 1)
                  {
                    NSOPR_LOGERR
                      ("length of network->network_json too big]len=%u",
                       strlen (get_port_json (port)));
                    retval = NSCRTL_ERR;
                  }

                retval =
                  STRCPY_S (get_config_data ()->json_buff,
                            sizeof (get_config_data ()->json_buff),
                            get_port_json (port));
                if (EOK != retval)
                  {
                    NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
                    retval = NSCRTL_ERR;
                  }

              }

            free (arg);
            arg = NULL;
          }
          break;
        default:
          break;
        }

      break;
    case IP_MODULE_NETWORK_ALL:
      if (operate_type == IP_MODULE_OPERATE_QUERY)
        {
          retval =
            get_network_all (get_config_data ()->json_buff,
                             sizeof (get_config_data ()->json_buff));
          NSOPR_SET_ERRINFO (retval, "get_network_all return %d", retval);
        }

      break;

    case IP_MODULE_IP_ALL:
      if (operate_type == IP_MODULE_OPERATE_QUERY)
        {
          retval =
            getIpCfgAll (get_config_data ()->json_buff,
                         sizeof (get_config_data ()->json_buff));
          NSOPR_SET_ERRINFO (retval, "getIpCfgAll return %d", retval);
        }

      break;

    case IP_MODULE_ALL:
      if (operate_type == IP_MODULE_OPERATE_QUERY)
        {
          NSOPR_LOGERR
            ("---------- IP_MODULE_ALL query is not implemented --------------");
          NSOPR_SET_ERRINFO (NSCRTL_ERR,
                             "ERR:This query interface is not implemented. ErrCode:%d",
                             NSCRTL_ERR);
        }

      break;

    default:
      break;
    }

  NSOPR_LOGINF ("process finished]result=%d", retval);

  return retval;
}

void
ip_subnet_print (struct ip_subnet *subnet)
{
  spl_ip_addr_t ipAddr;

  if (subnet == NULL)
    {
      return;
    }

  ipAddr.addr = spl_htonl (subnet->subnet);
  NSPOL_LOGINF (IP_DEBUG, "]\t Subnet=%s/%u", spl_inet_ntoa (ipAddr),
                subnet->mask_len);
}

port_buffer *
malloc_port_buffer ()
{
  port_buffer *buffer = malloc (sizeof (port_buffer));
  return buffer;
}

void
free_port_buffer (port_buffer * buffer)
{
  free (buffer);
}

network_buffer *
malloc_network_buffer ()
{
  network_buffer *buffer = malloc (sizeof (network_buffer));
  return buffer;
}

void
free_network_buffer (network_buffer * buffer)
{
  free (buffer);
}
