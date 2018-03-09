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

#include "common_sys_config.h"
#include "common_mem_mbuf.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "hal.h"
#include "hal_api.h"

#define HAL_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
/* *INDENT-OFF* */
static char hal_invalid_char_script[] = {'|', ';', '&', '$', '>', '<', '`', '\\', '\"', '\'',
                                     '(', ')', '[', ']', '~', '?', '*'
                                    };

static char* hal_invalid_str_script[] = {"&&", "||", ">>", "${", ";;", "/./", "/../"};

static char* hal_invalid_str_script_begin[] = {"./", "../"};

extern const netif_ops_t dpdk_netif_ops;

static hal_hdl_t hal_invaldi_hdl = {.id = -1};

static const netif_ops_t* netif_ops_table[HAL_DRV_MAX];
static int netif_ops_init_flag = 0;

netif_inst_t netif_tbl[HAL_MAX_NIC_NUM];
/* *INDENT-ON* */

void
hal_io_adpt_register (const netif_ops_t * ops)
{
  int icnt = 0;
  if (netif_ops_init_flag == 0)
    {
      (void) MEMSET_S (&netif_ops_table[0], sizeof (netif_ops_table), 0,
                       sizeof (netif_ops_table));
      netif_ops_init_flag = 1;
    }

  for (icnt = 0; icnt < HAL_DRV_MAX; icnt++)
    {
      if (netif_ops_table[icnt] == 0)
        {
          netif_ops_table[icnt] = ops;
          break;
        }
    }
  return;
}

/*****************************************************************************
 Prototype    : hal_snprintf
 Description  : create shell script
 Input        : char* buffer
                size_t buflen
                const char* format
                ...
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
int
hal_snprintf (char *buffer, size_t buflen, const char *format, ...)
{
  int len;
  va_list ap;

  /* check buffer validity */
  if (NULL == buffer || 0 == buflen)
    {
      goto einval_error;
    }

  if (format == NULL)
    {
      buffer[0] = '\0';

      goto einval_error;
    }

  (void) va_start (ap, format);
  len = VSNPRINTF_S (buffer, buflen, buflen - 1, format, ap);

  if (-1 == len)
    {
      va_end (ap);
      goto einval_error;
    }

  va_end (ap);

  buffer[buflen - 1] = '\0';

  return len;

einval_error:
  errno = EINVAL;
  return -1;
}

/*****************************************************************************
 Prototype    : hal_is_script_valid
 Description  : Check External Injection of Shell script Validation
 Input        : const char* cmd
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
int
hal_is_script_valid (const char *cmd)
{
  unsigned int i;

  if (cmd)
    {
      char *cmd_str = (char *) cmd;

      /* skip space */
      while (*cmd_str == ' ' || *cmd_str == '\t')
        {
          cmd_str++;
        }

      /* cmd can not start with ./ and ../ */
      for (i = 0; i < HAL_ARRAY_SIZE (hal_invalid_str_script_begin); i++)
        {
          if (strstr (cmd_str, hal_invalid_str_script_begin[i]) == cmd_str)
            {
              return 0;
            }
        }

      /* cmd can not include | ; $ and so on */
      for (i = 0; i < HAL_ARRAY_SIZE (hal_invalid_char_script); i++)
        {
          if (strchr (cmd, hal_invalid_char_script[i]))
            {
              return 0;
            }
        }

      /* cmd can not include && || >> and so on */
      for (i = 0; i < HAL_ARRAY_SIZE (hal_invalid_str_script); i++)
        {
          if (strstr (cmd, hal_invalid_str_script[i]))
            {
              return 0;
            }
        }

      return 1;
    }

  return 0;
}

/*****************************************************************************
 Prototype    : hal_run_script
 Description  : run shell script
 Input        : const char* cmd
                char* result_buf
                size_t max_result_len
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
int
hal_run_script (const char *cmd, char *result_buf, size_t max_result_len)
{
  size_t n;
  if (!cmd || !result_buf || max_result_len <= 1)
    {
      return -1;
    }

  FILE *fd = popen (cmd, "r");

  if (fd != NULL)
    {
      n = fread (result_buf, sizeof (char), max_result_len - 1, fd);

      if (n == 0)
        {
          result_buf[0] = '\0';
        }
      else if ('\n' == result_buf[n - 1])
        {
          result_buf[n - 1] = '\0';
        }
      /* make it null terminated */
      else
        {
          result_buf[n] = '\0';
        }

      (void) pclose (fd);
      return n;
    }

  return -1;
}

/*****************************************************************************
 Prototype    : hal_init_global
 Description  : init hal when proccess start
 Input        : int argc
                char** argv
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
int
hal_init_global (int argc, char **argv)
{
  int ret;
  int netif_type;

  ret =
    MEMSET_S (netif_tbl, HAL_MAX_NIC_NUM * sizeof (netif_inst_t), 0,
              HAL_MAX_NIC_NUM * sizeof (netif_inst_t));
  if (EOK != ret)
    {
      NSHAL_LOGERR ("MEMSET_S failed");
      return -1;
    }

  for (netif_type = 0; netif_ops_table[netif_type]; ++netif_type)
    {
      if (netif_ops_table[netif_type]->init_global)
        {
          if (netif_ops_table[netif_type]->init_global (argc, argv))
            {
              NSHAL_LOGERR ("failed to init global]netif type=%d",
                            netif_type);
              return -1;
            }
        }
    }

  return 0;
}

/*****************************************************************************
 Prototype    : hal_init_local
 Description  : init hal when process or thread start
 Input        : None
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
int
hal_init_local ()
{
  int netif_type;

  for (netif_type = 0; netif_ops_table[netif_type]; ++netif_type)
    {
      if (netif_ops_table[netif_type]->init_local)
        {
          if (netif_ops_table[netif_type]->init_local ())
            {
              NSHAL_LOGERR ("failed to init local]netif type=%d", netif_type);
              return -1;
            }
        }
    }

  return 0;
}

/*****************************************************************************
 Prototype    : hal_get_invalid_hdl
 Description  : get the invalid object
 Input        : None
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
hal_hdl_t
hal_get_invalid_hdl ()
{
  return hal_invaldi_hdl;
}

/*****************************************************************************
 Prototype    : hal_create
 Description  : create hal object
 Input        : const char* name
                hal_netif_config_t* conf
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
hal_hdl_t
hal_create (const char *name, hal_netif_config_t * conf)
{
  int ret = -1;
  uint32_t netif_type;
  netif_inst_t *inst;

  if ((NULL == name) || (NULL == conf))
    {
      NSHAL_LOGERR ("invalid para]name=%p,conf=%p", name, conf);
      return hal_get_invalid_hdl ();
    }

  inst = alloc_netif_inst ();

  if (NULL == inst)
    {
      NSHAL_LOGERR ("failed to alloc netif inst]netif name=%s", name);

      return hal_get_invalid_hdl ();
    }

  /*open */
  for (netif_type = 0; NULL != netif_ops_table[netif_type]; ++netif_type)
    {
      ret = netif_ops_table[netif_type]->open (inst, name);

      if (0 == ret)
        {
          inst->ops = netif_ops_table[netif_type];

          NSHAL_LOGINF ("netif ops]netif type=%u, netif name=%s", netif_type,
                        inst->ops->name);

          break;
        }
    }

  if (ret != 0)
    {
      inst->state = NETIF_STATE_FREE;

      inst->ops = NULL;

      NSHAL_LOGERR ("open fail]netif name=%s", name);

      return hal_get_invalid_hdl ();
    }

  /*config */
  ret = inst->ops->config (inst, conf);

  if (ret != 0)
    {
      inst->state = NETIF_STATE_FREE;

      NSHAL_LOGERR ("config fail]netif name=%s", name);

      return hal_get_invalid_hdl ();
    }

  /*start */
  ret = inst->ops->start (inst);

  if (ret != 0)
    {
      inst->state = NETIF_STATE_FREE;

      NSHAL_LOGERR ("start fail]netif name=%s", name);

      return hal_get_invalid_hdl ();
    }

  return inst->hdl;
}

/*****************************************************************************
 Prototype    : hal_bond
 Description  : create hal object for bond mode
 Input        : const char* bond_name
                uint8_t slave_num
                hal_hdl_t slave_hdl[]
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
hal_hdl_t
hal_bond (const char *bond_name, uint8_t slave_num, hal_hdl_t slave_hdl[])
{
  int i, ret;
  netif_inst_t *inst;
  netif_inst_t *slave_inst[HAL_MAX_SLAVES_PER_BOND];

  if ((0 == slave_num) || (HAL_MAX_SLAVES_PER_BOND < slave_num)
      || (NULL == bond_name) || (NULL == slave_hdl))
    {
      NSHAL_LOGERR ("invalid para]bond_name=%p,slave_num=%u,slave_hdl=%p,",
                    bond_name, slave_num, slave_hdl);

      return hal_get_invalid_hdl ();
    }

  for (i = 0; i < slave_num; i++)
    {
      slave_inst[i] = get_netif_inst (slave_hdl[i]);

      if (NULL == slave_inst[i])
        {
          NSHAL_LOGERR ("invalid para slave_hdl]index=%d, slave_inst=%d", i,
                        slave_hdl[i].id);
          return hal_get_invalid_hdl ();
        }
    }

  inst = alloc_netif_inst ();

  if (NULL == inst)
    {
      NSHAL_LOGERR ("failed to alloc nic inst]bond_name=%s", bond_name);
      return hal_get_invalid_hdl ();
    }

  inst->ops = slave_inst[0]->ops;

  ret = inst->ops->bond (inst, bond_name, slave_num, slave_inst);

  if (0 != ret)
    {
      inst->state = NETIF_STATE_FREE;

      inst->ops = NULL;

      NSHAL_LOGERR ("bond netif fail]bond_name=%s", bond_name);

      return hal_get_invalid_hdl ();
    }

  return inst->hdl;
}

/*****************************************************************************
 Prototype    : hal_close
 Description  : destroy hal object
 Input        : hal_hdl_t hdl
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
int
hal_close (hal_hdl_t hdl)
{
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return -1;
    }

  inst->state = NETIF_STATE_FREE;

  return inst->ops->close (inst);
}

/*****************************************************************************
 Prototype    : hal_stop
 Description  : stop recv packet
 Input        : hal_hdl_t hdl
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
int
hal_stop (hal_hdl_t hdl)
{
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return -1;
    }

  return inst->ops->stop (inst);
}

/*****************************************************************************
 Prototype    : hal_get_mtu
 Description  : get the mtu from nic
 Input        : hal_hdl_t hdl
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
uint32_t
hal_get_mtu (hal_hdl_t hdl)
{
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return 0;
    }

  return inst->ops->mtu (inst);
}

/*****************************************************************************
 Prototype    : hal_get_macaddr
 Description  : in normal mode, get the mac addr from nic
                in bond mode1, get the mac addr from primary nic
 Input        : hal_hdl_t hdl
                void* mac_addr
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
void
hal_get_macaddr (hal_hdl_t hdl, void *mac_addr)
{
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return;
    }

  (void) inst->ops->macaddr (inst, mac_addr);
}

/*****************************************************************************
 Prototype    : hal_get_capability
 Description  : get offload capability from nic
 Input        : hal_hdl_t hdl
                hal_netif_capa_t* info
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
void
hal_get_capability (hal_hdl_t hdl, hal_netif_capa_t * info)
{
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return;
    }

  (void) inst->ops->capability (inst, info);
}

/*****************************************************************************
 Prototype    : hal_recv_packet
 Description  : recv packet from nic
 Input        : hal_hdl_t hdl
                uint16_t queue_id
                struct common_mem_mbuf** rx_pkts
                uint16_t nb_pkts
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
uint16_t
hal_recv_packet (hal_hdl_t hdl, uint16_t queue_id,
                 struct common_mem_mbuf **rx_pkts, uint16_t nb_pkts)
{
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return 0;
    }

  return inst->ops->recv (inst, queue_id, rx_pkts, nb_pkts);
}

/*****************************************************************************
 Prototype    : hal_send_packet
 Description  : send packet to nic
 Input        : hal_hdl_t hdl
                uint16_t queue_id
                struct common_mem_mbuf** tx_pkts
                uint16_t nb_pkts
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
uint16_t
hal_send_packet (hal_hdl_t hdl, uint16_t queue_id,
                 struct common_mem_mbuf ** tx_pkts, uint16_t nb_pkts)
{
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return 0;
    }

  return inst->ops->send (inst, queue_id, tx_pkts, nb_pkts);
}

/*****************************************************************************
 Prototype    : hal_link_status
 Description  : get link status form nic
 Input        : hal_hdl_t hdl
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
uint32_t
hal_link_status (hal_hdl_t hdl)
{
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return 0;
    }

  return inst->ops->link_status (inst);
}

/*****************************************************************************
 Prototype    : hal_stats
 Description  : get link statistics form nic
 Input        : hal_hdl_t hdl
                hal_netif_stats_t* stats
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
int
hal_stats (hal_hdl_t hdl, hal_netif_stats_t * stats)
{
  netif_inst_t *inst;

  if (NULL == stats)
    {
      NSHAL_LOGERR ("invalid para");
      return -1;
    }

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return -1;
    }

  return inst->ops->stats (inst, stats);
}

/*****************************************************************************
 Prototype    : hal_stats_reset
 Description  : reset link statistics to nic
 Input        : hal_hdl_t hdl
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
void
hal_stats_reset (hal_hdl_t hdl)
{
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return;
    }

  (void) inst->ops->stats_reset (inst);
}

/*****************************************************************************
 Prototype    : hal_add_mcastaddr
 Description  : set multicast addrs to nic
 Input        : hal_hdl_t hdl
                void* mc_addr_set
                void* mc_addr
                uint32_t nb_mc_addr
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
int
hal_add_mcastaddr (hal_hdl_t hdl, void *mc_addr_set,
                   void *mc_addr, uint32_t nb_mc_addr)
{
  int ret;
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return -1;
    }

  ret = inst->ops->mcastaddr (inst, mc_addr_set, mc_addr, nb_mc_addr);
  /* if set mcast addr failed, we have to manually add the mac addr to nic. */
  if (ret < 0)
    {
      ret = inst->ops->add_mac (inst, mc_addr);
    }
  return ret;
}

/*****************************************************************************
 Prototype    : hal_del_mcastaddr
 Description  : delete multicast addrs to nic
 Input        : hal_hdl_t hdl
                void* mc_addr_set
                void* mc_addr
                uint32_t nb_mc_addr
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
int
hal_del_mcastaddr (hal_hdl_t hdl, void *mc_addr_set,
                   void *mc_addr, uint32_t nb_mc_addr)
{
  int ret;
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return -1;
    }

  ret = inst->ops->mcastaddr (inst, mc_addr_set, mc_addr, nb_mc_addr);
  /* if set mcast addr failed, we have to manually delete the mac addr from nic. */
  if (ret < 0)
    {
      ret = inst->ops->rmv_mac (inst, mc_addr);
    }
  return ret;
}

/*****************************************************************************
 Prototype    : hal_set_allmulti_mode
 Description  : set all multicast mode
 Input        : hal_hdl_t hdl
                uint8_t enable
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
void
hal_set_allmulti_mode (hal_hdl_t hdl, uint8_t enable)
{
  netif_inst_t *inst;

  inst = get_netif_inst (hdl);

  if (inst == NULL)
    {
      NSHAL_LOGERR ("netif does not exist]inst=%i", HAL_HDL_TO_ID (hdl));
      return;
    }

  (void) inst->ops->allmcast (inst, enable);
}

/*****************************************************************************
 Prototype    : hal_is_nic_exist
 Description  : check nic is exist
 Input        : const char *name
 Output       : None
 Return Value :
 Calls        :
 Called By    :
*****************************************************************************/
uint32_t
hal_is_nic_exist (const char *name)
{
  char script_cmmd[HAL_SCRIPT_LENGTH];
  char result_buf[HAL_SCRIPT_LENGTH];
  int len_out;
  int retval;

  if (!hal_is_script_valid (name))
    {
      NSHAL_LOGERR ("nic name is not valid");
      return 0;
    }

  retval =
    hal_snprintf (script_cmmd, sizeof (script_cmmd),
                  "sudo ifconfig -a | grep -w \"%s[ :]\"", name);
  if (-1 == retval)
    {
      NSHAL_LOGERR ("rte_snprintf failed]retval=%d", retval);
      return 0;
    }

  len_out = hal_run_script (script_cmmd, result_buf, sizeof (result_buf) - 1);
  /* buffer not initialized, should take length as decision */
  if (0 != len_out)
    {
      return 1;
    }

  return 0;
}
