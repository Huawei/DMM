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

#ifndef __NSTACK_RD_DATA_H
#define __NSTACK_RD_DATA_H

/*choose router base on ip seg*/
#define STACK_NAME_MAX    32
#define RD_PLANE_NAMELEN     (32)
#define NSTACK_IP_BIT_MAX    (32)
#define NSTACK_RD_DATA_MAX   (2048)
#define RD_IP_STR_MAX_LEN    (32)

typedef enum __rd_data_type
{
  RD_DATA_TYPE_IP,
  RD_DATA_TYPE_PROTO,
  RD_DATA_TYPE_MAX,
} rd_data_type;

typedef enum __rd_node_state
{
  RD_NODE_USELESS,
  RD_NODE_USING,
  RD_NODE_DELETING,
  RD_NODE_MAX,
} rd_node_state;

/*route info base on ip*/
typedef struct __rd_route_ip_data
{
  unsigned int addr;
  unsigned int masklen;
  unsigned int resev[2];
} rd_ip_data;

/*route data*/
typedef struct __rd_route_data
{
  /*route info type , for example base on ip */
  rd_data_type type;
  char stack_name[RD_PLANE_NAMELEN];
  union
  {
    rd_ip_data ipdata;
    unsigned int proto_type;
    /*:::other type to be add */
  };
} rd_route_data;

typedef struct __rd_route_node
{
  rd_node_state flag;
  int agetime;
  rd_route_data data;
} rd_route_node;

typedef struct __rd_route_table
{
  int size;
  int icnt;
  rd_route_node node[NSTACK_RD_DATA_MAX];
} rd_route_table;

#define MASK_V(ipaddr, masklen)  ((ipaddr) & (~0 << (NSTACK_IP_BIT_MAX - (masklen))))

#endif
