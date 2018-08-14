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

#ifndef _IP_MODULE_API_H_
#define _IP_MODULE_API_H_
#include "types.h"

typedef int bool_t;
#define IP_MODULE_TRUE 1
#define IP_MODULE_FALSE 0
#define IP_MODULE_LENGTH_32 32
#define IP_MODULE_LENGTH_64 64
#define IP_MODULE_LENGTH_256 256
#define IP_MODULE_LENGTH_1024 1024
#define IP_MODULE_SUBNET_MASK_LEN IP_MODULE_LENGTH_32
#define IP_MODULE_MAX_NAME_LEN IP_MODULE_LENGTH_256
#define IP_MODULE_PORT_JSON_LEN (2048 * 2)
#define IP_MODULE_NETWORK_JSON_LEN (2048 * 2)
#define MAX_NETWORK_COUNT IP_MODULE_LENGTH_64
#define MAX_NETWORK_IP_COUNT IP_MODULE_LENGTH_1024
#define MAX_NETWORK_NUM (MAX_NETWORK_COUNT * 2)
COMPAT_PROTECT (MAX_NETWORK_NUM, 128);
#define MAX_PORT_NUM (MAX_NETWORK_IP_COUNT * 2)
COMPAT_PROTECT (MAX_PORT_NUM, 2048);
#define NSCRTL_ERRBUF_LEN IP_MODULE_LENGTH_256

#define INVALID_EXPIRE_TIME 0x7FFFFFFF
#define NSTACLCTRL_MAX_TRY_TIMES 60

/*sockaddr_un.sun_path is an array of 108 bytes*/
#define IP_MODULE_MAX_PATH_LEN 108

typedef enum enumADERRCODE
{
  NSCRTL_OK = 0,
  NSCRTL_ERR,
  NSCRTL_RD_NOT_EXIST,
  NSCRTL_RD_EXIST = 3,
  NSCRTL_INPUT_ERR,
  NSCRTL_STATUS_ERR,
  NSCRTL_NETWORK_COUNT_EXCEED,
  NSCRTL_IP_COUNT_EXCEED,
  NSCRTL_FREE_ALL_PORT,         //when all ports in container were deleted, need to free container.

  NSCRTL_MAX_ERR = 127
} NSCRTL_ERR_CODE;

#define NSOPR_SET_ERRINFO(_errno, fmt, ...) \
    {\
        struct config_data* cf_data = get_config_data(); \
        if (cf_data)\
        {\
            size_t len_error_desc = strlen(cf_data->param.error_desc); \
            cf_data->param.error = _errno; \
            if ((_errno != NSCRTL_OK) && (len_error_desc < NSCRTL_ERRBUF_LEN))\
            {\
                if (-1 == (SNPRINTF_S(cf_data->param.error_desc + len_error_desc, \
                                      NSCRTL_ERRBUF_LEN - len_error_desc, NSCRTL_ERRBUF_LEN - len_error_desc - 1, fmt, ## __VA_ARGS__)))\
                {\
                    NSOPR_LOGERR("SNPRINTF_S failed]"); \
                } \
            } \
        } \
    } \

struct ref_nic
{
  struct ref_nic *next;
  char nic_name[IP_MODULE_MAX_NAME_LEN];
};

struct phy_net
{
  struct ref_nic *header;
  char bond_name[IP_MODULE_MAX_NAME_LEN];
  int bond_mode;
};

struct container_port_ip_cidr
{
  struct container_port_ip_cidr *next;
  unsigned int ip;
  unsigned int mask_len;
};

struct container_multicast_id
{
  struct container_multicast_id *next;
  unsigned int ip;
};

typedef struct
{
  char port_json[IP_MODULE_PORT_JSON_LEN];
} port_buffer;

struct container_port
{
  struct container_port *next;
  struct container_port_ip_cidr *ip_cidr_list;
  struct container_multicast_id *multicast_list;
  char port_name[IP_MODULE_MAX_NAME_LEN];
  port_buffer *buffer;
};

#define get_port_json(obj) obj->buffer->port_json

struct container_ip
{
  struct container_ip *next;
  struct container_port *ports_list;
  char container_id[IP_MODULE_MAX_NAME_LEN];
};

struct container_list
{
  struct container_ip *header;
};

struct ip_subnet
{
  struct ip_subnet *next;
  unsigned int subnet;
  unsigned int mask_len;
};

typedef struct
{
  char network_json[IP_MODULE_NETWORK_JSON_LEN];
} network_buffer;

struct network_configuration
{
  struct network_configuration *next;
  struct phy_net *phy_net;
  struct ip_subnet *ip_subnet;
  char network_name[IP_MODULE_MAX_NAME_LEN];
  char type_name[IP_MODULE_MAX_NAME_LEN];
  network_buffer *buffer;
};

#define get_network_json(obj) obj->buffer->network_json

struct network_list
{
  struct network_configuration *header;
};

void ip_subnet_print (struct ip_subnet *subnet);
bool_t is_in_same_network (unsigned int src_ip, unsigned int dst_ip);
bool_t is_ip_match_netif (unsigned int ip, char *netif_name);
bool_t is_ip_exist (unsigned int ip);
struct network_configuration *get_network_list ();
inline struct network_configuration *get_network_by_ip_with_tree (unsigned int
                                                                  ip);

/* "type" option */
typedef enum
{
  IP_MODULE_NETWORK,
  IP_MODULE_IP,
  IP_MODULE_NETWORK_ALL,
  IP_MODULE_IP_ALL,
  IP_MODULE_ALL,
} ip_module_type;

/* "action" option */
typedef enum
{
  IP_MODULE_OPERATE_NULL,
  IP_MODULE_OPERATE_ADD,
  IP_MODULE_OPERATE_DEL,
  IP_MODULE_OPERATE_QUERY,
  IP_MODULE_OPERATE_SET,
  IP_MODULE_GET_VERSION,
  IP_MODULE_QUERY_NET,
  IP_MODULE_MAX,                //new type should be added before IP_MODULE_MAX
  IP_MODULE_BOTTOM = 0xFFFFFFFF
} ip_module_operate_type;

typedef int (*post_to_fn) (void *arg, ip_module_type type,
                           ip_module_operate_type operate_type);
typedef int (*add_netif_ip_fn) (char *netif_name, unsigned int ip,
                                unsigned int mask);
typedef int (*del_netif_ip_fn) (char *netif_name, unsigned int ip);

typedef struct
{
  post_to_fn post_to;
  add_netif_ip_fn add_netif_ip;
  del_netif_ip_fn del_netif_ip;
} output_api;

void regist_output_api (output_api * api);
output_api *get_output_api ();
int init_configuration_reader ();
int process_post (void *arg, ip_module_type type,
                  ip_module_operate_type operate_type);
int process_configuration (void *arg, ip_module_type type,
                           ip_module_operate_type operate_type);

port_buffer *malloc_port_buffer ();
void free_port_buffer (port_buffer * buffer);
network_buffer *malloc_network_buffer ();
void free_network_buffer (network_buffer * buffer);
int get_network_json_data ();
int get_ip_json_data ();

#endif
