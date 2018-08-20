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

#include <stdint.h>
#include <sched.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/file.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fnmatch.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>

#include <rte_config.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_eth_bond.h>
#include "nsfw_init.h"
#include "common_mem_mbuf.h"
#include "common_mem_mempool.h"
#include "common_func.h"
#include "hal.h"
#include "nstack_securec.h"
#include <rte_ethdev_driver.h>

#define DPDK_NON_ROOT_USER_NAME "paas"
#define DPDK_TOOL_ENV "DPDK_TOOL_DIR"
#define DPDK_NIC_LIST_FILE   "%s/ip_module/.nstack_dpdk_nic_list"
#define SOCKET_ID_0 0

NSTACK_STATIC struct passwd *dpdk_non_root_user;
NSTACK_STATIC char dpdk_tool_path[HAL_MAX_PATH_LEN] = { 0 };

/* Default configuration for rx and tx thresholds etc. */
NSTACK_STATIC const struct rte_eth_rxconf rx_conf_default_igb = {
  .rx_thresh = {
                .pthresh = 8,
                .hthresh = 8,
                .wthresh = 1,   //not bigger than 1
                },
};

/*
 * These default values are optimized for use with the Intel(R) 82576 1 GbE
 * Controller and the DPDK e1000 PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
NSTACK_STATIC const struct rte_eth_txconf tx_conf_default_igb = {
  .tx_thresh = {
                .pthresh = 8,
                .hthresh = 1,
                .wthresh = 16,
                },
  .tx_free_thresh = 0,          /* Use PMD default values */
  .tx_rs_thresh = 0,            /* Use PMD default values */
};

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */

/* Default configuration for rx and tx thresholds etc. */
NSTACK_STATIC const struct rte_eth_rxconf rx_conf_default_ixgbe = {
  .rx_thresh = {
                .pthresh = 8,
                .hthresh = 8,
                .wthresh = 4,
                },
  .rx_free_thresh = 0,
};

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
NSTACK_STATIC const struct rte_eth_txconf tx_conf_default_ixgbe = {
  .tx_thresh = {
                .pthresh = 36,
                .hthresh = 0,
                .wthresh = 0,
                },
  .tx_free_thresh = 0,          /* Use PMD default values */
  .tx_rs_thresh = 0,            /* Use PMD default values */
  .txq_flags = 0,
};

/* the port configuration of normal port */
NSTACK_STATIC struct rte_eth_conf port_conf_default_normal = {
  .rxmode = {
             .mq_mode = ETH_RSS,
             .max_rx_pkt_len = ETHER_MAX_LEN,
             .split_hdr_size = 0,
             .header_split = 0,
             .hw_ip_checksum = 1,
             .hw_vlan_filter = 1,
             .hw_vlan_strip = 1,
             .jumbo_frame = 0,
             .hw_strip_crc = 0,
             },
  .rx_adv_conf = {
                  .rss_conf = {
                               .rss_key = NULL,
                               .rss_hf = (ETH_RSS_IPV4 | ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_NONFRAG_IPV4_UDP),  //rss hash key
                               },
                  },
  .txmode = {
             .mq_mode = ETH_DCB_NONE,
             },
  .intr_conf = {
                .lsc = 0,
                },
};

/* the port configuration of virtio port */
NSTACK_STATIC struct rte_eth_conf port_conf_default_virtio = {
  .rxmode = {
             .mq_mode = ETH_RSS,
             .max_rx_pkt_len = ETHER_MAX_LEN,
             .split_hdr_size = 0,
             .header_split = 0,
             .hw_ip_checksum = 0,       /* Virtio NIC doesn't support HW IP CheckSUM */
             .hw_vlan_filter = 1,
             .hw_vlan_strip = 1,
             .jumbo_frame = 0,
             .hw_strip_crc = 0,
             },
  .rx_adv_conf = {
                  .rss_conf = {
                               .rss_key = NULL,
                               .rss_hf = (ETH_RSS_IPV4 | ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_NONFRAG_IPV4_UDP),  //rss hash key
                               },
                  },
  .txmode = {
             .mq_mode = ETH_DCB_NONE,
             },
  .intr_conf = {
                .lsc = 0,
                },
};

/* the port configuration of bond port */
NSTACK_STATIC struct rte_eth_conf port_conf_default_bond = {
  .rxmode = {
             .mq_mode = ETH_MQ_RX_NONE,
             .max_rx_pkt_len = ETHER_MAX_LEN,
             .split_hdr_size = 0,
             .header_split = 0,
                             /**< Header Split disabled */
             .hw_ip_checksum = 0,
                             /**< IP checksum offload enabled */
             .hw_vlan_filter = 1,
                             /**< VLAN filtering enabled */
             .hw_vlan_strip = 1,
             .jumbo_frame = 0,
                             /**< Jumbo Frame Support disabled */
             .hw_strip_crc = 0,
                             /**< CRC stripped by hardware */
             },
  .rx_adv_conf = {
                  .rss_conf = {
                               .rss_key = NULL,
                               .rss_hf = ETH_RSS_IP,
                               },
                  },
  .txmode = {
             .mq_mode = ETH_MQ_TX_NONE,
             },
};

/*****************************************************************************
*   Prototype    : hal_rte_eth_rx_burst
*   Description  : a copy of rte_eth_rx_burst, because this function invokes
                   a global(rte_eth_devices), which cannt be access by dlsym
                   symbols
*   Input        : uint8_t port_id
*                  uint16_t queue_id
*                  struct rte_mbuf **rx_pkts
*                  const uint16_t nb_pkts
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
NSTACK_STATIC inline uint16_t
hal_rte_eth_rx_burst (uint8_t port_id, uint16_t queue_id,
                      struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
#ifdef RTE_ETHDEV_RXTX_CALLBACKS
  struct rte_eth_rxtx_callback *cb;
#endif
  int16_t nb_rx;

  struct rte_eth_dev *dev = &rte_eth_devices[port_id];

  if (NULL == dev->rx_pkt_burst)
    {
      NSHAL_LOGERR ("dev->rx_pkt_burst is NULL,dev=%p", dev);
      return 0;
    }

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
  RTE_ETH_VALID_PORTID_OR_ERR_RET (port_id, 0);
  RTE_FUNC_PTR_OR_ERR_RET (*dev->rx_pkt_burst, 0);

  if (queue_id >= dev->data->nb_rx_queues)
    {
      RTE_PMD_DEBUG_TRACE ("Invalid RX queue_id=%d\n", queue_id);
      return 0;
    }
#endif
  nb_rx = (*dev->rx_pkt_burst) (dev->data->rx_queues[queue_id],
                                rx_pkts, nb_pkts);

#ifdef RTE_ETHDEV_RXTX_CALLBACKS
  cb = dev->post_rx_burst_cbs[queue_id];

  if (unlikely (cb != NULL))
    {
      do
        {
          nb_rx = cb->fn.rx (port_id, queue_id, rx_pkts, nb_rx,
                             nb_pkts, cb->param);
          cb = cb->next;
        }
      while (cb != NULL);
    }
#endif

  return (uint16_t) nb_rx;
}

/*****************************************************************************
*   Prototype    : hal_rte_eth_tx_burst
*   Description  : a copy of rte_eth_tx_burst, because this function invokes

*   Input        : uint8_t port_id
*                  uint16_t queue_id
*                  struct rte_mbuf **tx_pkts
*                  uint16_t nb_pkts
*   Output       : None
*   Return Value :
*   Calls        :
*   Called By    :
*
*****************************************************************************/
NSTACK_STATIC inline uint16_t
hal_rte_eth_tx_burst (uint8_t port_id, uint16_t queue_id,
                      struct rte_mbuf ** tx_pkts, uint16_t nb_pkts)
{
#ifdef RTE_ETHDEV_RXTX_CALLBACKS
  struct rte_eth_rxtx_callback *cb;
#endif

  struct rte_eth_dev *dev = &rte_eth_devices[port_id];

  if (NULL == dev->tx_pkt_burst)
    {
      NSHAL_LOGERR ("dev->tx_pkt_burst is NULL");
      return 0;
    }

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
  RTE_ETH_VALID_PORTID_OR_ERR_RET (port_id, 0);
  RTE_FUNC_PTR_OR_ERR_RET (*dev->tx_pkt_burst, 0);

  if (queue_id >= dev->data->nb_tx_queues)
    {
      RTE_PMD_DEBUG_TRACE ("Invalid TX queue_id=%d\n", queue_id);
      return 0;
    }
#endif

#ifdef RTE_ETHDEV_RXTX_CALLBACKS
  cb = dev->pre_tx_burst_cbs[queue_id];

  if (unlikely (cb != NULL))
    {
      do
        {
          nb_pkts = cb->fn.tx (port_id, queue_id, tx_pkts, nb_pkts,
                               cb->param);
          cb = cb->next;
        }
      while (cb != NULL);
    }
#endif

  return (*dev->tx_pkt_burst) (dev->data->tx_queues[queue_id], tx_pkts,
                               nb_pkts);
}

/*****************************************************************************
 Prototype    : dpdk_get_hugepage_size
 Description  : get the free hugepage size
 Input        : the dir of the nstack hugepage
 Output       : free hugepage size
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
int
dpdk_read_hugepage_size (int *freehuge)
{
  int fd_huge;
  int len;
  char buf[5] = { '\0' };
  fd_huge =
    open ("/sys/kernel/mm/hugepages/hugepages-1048576kB/free_hugepages",
          O_RDONLY);
  if (fd_huge < 0)
    {
      NSHAL_LOGERR ("errno=%d", errno);
      return -1;
    }

  len = read (fd_huge, buf, sizeof (buf));
  if (len < 0)
    {
      NSHAL_LOGERR ("errno=%d", errno);
      close (fd_huge);          //fix codeDEX 124547
      return -1;
    }
  *freehuge = buf[0] - '0';
  NSHAL_LOGINF ("hugepage size=%d", *freehuge);
  close (fd_huge);

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_clear_hugedir
 Description  : clear the hugepage which is used by dpdk
 Input        : the dir of the nstack hugepage
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_clear_hugedir (const char *hugedir)
{
  DIR *dir;
  struct dirent *dirent_dpdk;
  int dir_fd, fd, lck_result, lk_result;
  const char filter[] = "*mapns*";      /* matches hugepage files */

  /* open directory */
  dir = opendir (hugedir);
  if (!dir)
    {
      NSHAL_LOGERR ("the path %s is not exist, errno = %d", hugedir, errno);
      goto error;
    }
  dir_fd = dirfd (dir);

  dirent_dpdk = readdir (dir);
  if (!dirent_dpdk)
    {
      NSHAL_LOGERR ("the dir %s can not read, errno = %d", hugedir, errno);
      goto error;
    }

  while (dirent_dpdk != NULL)
    {
      /* skip files that don't match the hugepage pattern */
      if (fnmatch (filter, dirent_dpdk->d_name, 0) > 0)
        {
          NSHAL_LOGWAR ("the file name %s is not match mapns, errno = %d",
                        dirent_dpdk->d_name, errno);
          dirent_dpdk = readdir (dir);
          continue;
        }

      /* try and lock the file */
      fd = openat (dir_fd, dirent_dpdk->d_name, O_RDONLY);

      /* skip to next file */
      if (fd == -1)
        {
          NSHAL_LOGERR ("the file name %s can not be lock, errno = %d",
                        dirent_dpdk->d_name, errno);
          dirent_dpdk = readdir (dir);
          continue;
        }

      /* non-blocking lock */
      lck_result = flock (fd, LOCK_EX | LOCK_NB);

      /* if lock succeeds, unlock and remove the file */
      if (lck_result != -1)
        {
          NSHAL_LOGWAR
            ("the file name %s can be lock and will delete, errno = %d",
             dirent_dpdk->d_name, errno);
          lck_result = flock (fd, LOCK_UN);
          if (-1 == lck_result)
            NSHAL_LOGERR ("the file name %s unlock fail, errno = %d",
                          dirent_dpdk->d_name, errno);
          lk_result = unlinkat (dir_fd, dirent_dpdk->d_name, 0);
          if (-1 == lk_result)
            NSHAL_LOGERR ("the file name %s is unlinkat fail, errno = %d",
                          dirent_dpdk->d_name, errno);
        }
      close (fd);
      dirent_dpdk = readdir (dir);
    }

  (void) closedir (dir);
  return 0;

error:
  if (dir)
    (void) closedir (dir);

  return -1;
}

/*****************************************************************************
 Prototype    : dpdk_init_global
 Description  : DPDK global init
 Input        : int argc
                char** argv
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_init_global (int argc, char **argv)
{
  //int ret;
  const char hugepath[] = "/mnt/nstackhuge";
  //int freeHuge = 0;
  //int retryCount = 10;

  if (-1 == dpdk_clear_hugedir (hugepath))
    {
      NSHAL_LOGERR ("clear hugedir fail, try again!");
      sys_sleep_ns (0, 100000000);
      (void) dpdk_clear_hugedir (hugepath);
    }
  NSHAL_LOGINF ("init global succ");

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_init_env
 Description  : init dpdk run env
 Input        : void
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_init_env (void)
{
  int ret;
  size_t len_size;
  char *dpdk_env;
  char *dpdk_path;

  /* Get dpdk_tool_path */
  dpdk_env = getenv (DPDK_TOOL_ENV);
  if (NULL == dpdk_env)
    {
      NSHAL_LOGERR ("please set enviroment:%s before start this stack"
                    "\nthe value of the %s must be the path of dpdk tools",
                    DPDK_TOOL_ENV, DPDK_TOOL_ENV);
      return -1;
    }

  /* modify ugly len_size judgement and strcpy */
  /* check len_size for malloc */
  len_size = strlen (dpdk_env);
  if (0 == len_size || len_size >= HAL_MAX_PATH_LEN)
    {
      NSHAL_LOGERR ("fail to dpdk_env strlen(DPDK_TOOL_ENV)");
      return -1;
    }

  /* DPDK_TOOL_ENV's value will be use as popen's paramter,we need check's validity */
  dpdk_path = realpath (dpdk_env, NULL);
  if (NULL == dpdk_path)
    {
      NSHAL_LOGERR ("env:%s value incorrect]value=%s,errno=%d", DPDK_TOOL_ENV,
                    dpdk_env, errno);
      return -1;
    }

  len_size = strlen (dpdk_path);
  if (0 == len_size || len_size >= HAL_MAX_PATH_LEN)
    {
      NSHAL_LOGERR ("fail to dpdk_path strlen(DPDK_TOOL_ENV)");
      return -1;
    }

  ret = STRCPY_S (dpdk_tool_path, HAL_MAX_PATH_LEN, dpdk_path);
  if (EOK != ret)
    {
      NSHAL_LOGERR ("STRCPY_S failed]ret=%d", ret);
      return -1;
    }

  if (!hal_is_script_valid (dpdk_tool_path))
    {
      NSHAL_LOGERR ("dpdk_tool_path is invalid]dpdk_tool_path=%s",
                    dpdk_tool_path);
      return -1;
    }

  /* get non-root user's id */
  dpdk_non_root_user = getpwnam (DPDK_NON_ROOT_USER_NAME);
  if (dpdk_non_root_user)
    {
      NSHAL_LOGINF ("non-root]name=%s,uid=%u,gid=%u,errno=%d",
                    dpdk_non_root_user->pw_name, dpdk_non_root_user->pw_uid,
                    dpdk_non_root_user->pw_gid, errno);
    }
  else
    {
      NSHAL_LOGERR ("non-root]cannot find user %s", DPDK_NON_ROOT_USER_NAME);
    }

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_init_local
 Description  : DPDK local init
 Input        : void
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_init_local (void)
{
  int ret;

  ret = dpdk_init_env ();

  if (ret < 0)
    {
      NSHAL_LOGERR ("dpdk_init_env failed");
      return -1;
    }

  NSHAL_LOGINF ("init local succ");

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_set_port
 Description  : check and save the port num
 Input        : netif_inst_t* inst
                int port
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_set_port (netif_inst_t * inst, uint8_t port)
{
  if (port >= rte_eth_dev_count ())
    {
      NSHAL_LOGERR ("the number of port=%d is more than rte_eth_dev_count=%d",
                    port, rte_eth_dev_count ());
      return -1;
    }

  inst->data.dpdk_if.port_id = port;

  return 0;

}

/*****************************************************************************
 Prototype    : dpdk_set_nic_name
 Description  : check and save nic name
 Input        : netif_inst_t* inst
                const char* name
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_set_nic_name (netif_inst_t * inst, const char *name)
{
  int ret;

  /* sizeof(pointer) always = 8 in 64 bit system */
  ret =
    STRCPY_S (inst->data.dpdk_if.nic_name,
              sizeof (inst->data.dpdk_if.nic_name), name);
  if (EOK != ret)
    {
      NSHAL_LOGERR ("STRCPY_S set nic_name failed]ret=%d", ret);
      return -1;
    }

  if (!hal_is_script_valid (inst->data.dpdk_if.nic_name))
    {
      NSHAL_LOGERR ("nic_name is invalid");
      return -1;
    }

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_get_driver_name
 Description  : get and save driver name
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_get_driver_name (netif_inst_t * inst)
{
  int ret_len, ret;
  char script_cmmd[HAL_SCRIPT_LENGTH];
  char driver_name[HAL_SCRIPT_LENGTH] = { 0 };

  ret = hal_snprintf (script_cmmd, sizeof (script_cmmd),
                      "readlink -f /sys/class/net/" "%s"
                      "/device/driver| awk -F'/' '{print $6}'",
                      inst->data.dpdk_if.nic_name);
  if (-1 == ret)
    {
      NSHAL_LOGERR ("hal_snprintf failed");
      return -1;
    }

  ret_len =
    hal_run_script (script_cmmd, driver_name, sizeof (driver_name) - 1);

  if (ret_len > HAL_MAX_DRIVER_NAME_LEN)
    {
      ret_len = HAL_MAX_DRIVER_NAME_LEN;
    }

  if (ret_len <= 0)
    {
      NSHAL_LOGERR ("%s does't have a driver", driver_name);

      ret =
        STRNCPY_S (inst->data.dpdk_if.driver_name,
                   sizeof (inst->data.dpdk_if.driver_name), "NULL",
                   sizeof ("NULL"));

      if (EOK != ret)
        {
          NSHAL_LOGERR ("STRNCPY_S failed]ret=%d.", ret);
          return -1;
        }

      return -1;
    }
  else
    {
      ret =
        STRNCPY_S (inst->data.dpdk_if.driver_name,
                   sizeof (inst->data.dpdk_if.driver_name), driver_name,
                   ret_len);

      if (EOK != ret)
        {
          NSHAL_LOGERR ("STRNCPY_S failed]ret=%d.", ret);
          return -1;
        }

      inst->data.dpdk_if.driver_name[(ret_len - 1)] = '\0';

      return 0;
    }
}

/*****************************************************************************
 Prototype    : dpdk_set_pci_permission
 Description  : set pci permission
 Input        : char *pci_addr
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_set_pci_permission (char *pci_addr)
{
  DIR *dir_desc;
  char file_path[HAL_SCRIPT_LENGTH] = { 0 }, dir_path[HAL_SCRIPT_LENGTH] =
  {
  0};
  struct dirent *ent;
  struct stat statbuf;
  int ret;

  ret =
    SNPRINTF_S (dir_path, sizeof (dir_path), sizeof (dir_path) - 1,
                "/sys/bus/pci/devices/%s", pci_addr);
  if (ret < 0)
    {
      NSHAL_LOGERR ("SNPRINTF_S fail");
      return -1;
    }

  if ((dir_desc = opendir (dir_path)) == NULL)
    {
      NSHAL_LOGERR ("opendir fail:errno=%d", errno);
      return -1;
    }

  while ((ent = readdir (dir_desc)) != NULL)
    {
      if (strstr (ent->d_name, "resource"))
        {
          ret =
            SNPRINTF_S (file_path, sizeof (file_path), sizeof (file_path) - 1,
                        "%s/%s", dir_path, ent->d_name);
          if (ret < 0)
            {
              NSHAL_LOGERR ("SNPRINTF_S fail");
              (void) closedir (dir_desc);
              return -1;
            }

          if (!lstat (file_path, &statbuf) && !S_ISDIR (statbuf.st_mode))
            {
              ret =
                chown (file_path, dpdk_non_root_user->pw_uid,
                       dpdk_non_root_user->pw_gid);
              if (ret < 0)
                {
                  NSHAL_LOGERR ("chown fail]file_path=%s,ret=%d,errno=%d",
                                file_path, ret, errno);
                  (void) closedir (dir_desc);
                  return -1;
                }
              NSHAL_LOGWAR ("chown succ]file_path=%s,ret=%d", file_path, ret);
              ret = chmod (file_path, 0640);
              if (ret < 0)
                {
                  NSHAL_LOGERR ("chmod fail]file_path=%s,ret=%d,errno=%d",
                                file_path, ret, errno);
                  (void) closedir (dir_desc);
                  return -1;
                }
              NSHAL_LOGWAR ("chmod succ]file_path=%s,ret=%d", file_path, ret);
            }
        }
    }

  (void) closedir (dir_desc);
  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_get_pci_addr
 Description  : get and save pci addr
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_get_pci_addr (netif_inst_t * inst)
{
  int ret, pci_len;
  struct ethtool_drvinfo edata = { 0 };
  struct ifreq ifr;
  int fd = -1;

  /* use ioctl to get pci address instead of call dpdk-devbind.py to reduce time cost */
  ret = MEMSET_S (&ifr, sizeof (ifr), 0, sizeof (ifr));
  if (EOK != ret)
    {
      NSHAL_LOGERR ("MEMSET_S fail");
      return -1;
    }
  edata.cmd = ETHTOOL_GDRVINFO;
  ret =
    STRCPY_S (ifr.ifr_name, sizeof (ifr.ifr_name),
              inst->data.dpdk_if.nic_name);
  if (EOK != ret)
    {
      NSHAL_LOGERR ("STRCPY_S fail");
      return -1;
    }

  ifr.ifr_data = (char *) (&edata);
  if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      NSHAL_LOGERR ("cannot init socket, errno=%d", errno);
      return -1;
    }
  if (ioctl (fd, SIOCETHTOOL, &ifr) < 0)
    {
      NSHAL_LOGERR ("ioctl to the device %s err, errno=%d",
                    inst->data.dpdk_if.nic_name, errno);
      close (fd);
      return -1;
    }
  close (fd);

  pci_len = strlen (edata.bus_info);
  if (pci_len == 0
      || pci_len > (int) sizeof (inst->data.dpdk_if.pci_addr) - 1)
    {
      NSHAL_LOGERR ("does't have a pci_addr");
      inst->data.dpdk_if.pci_addr[0] = '\0';
      return -1;
    }

  NSHAL_LOGINF ("nic_name=%s,nic_pci_addr=%s", inst->data.dpdk_if.nic_name,
                edata.bus_info);

  ret =
    STRNCPY_S (inst->data.dpdk_if.pci_addr,
               sizeof (inst->data.dpdk_if.pci_addr), edata.bus_info, pci_len);
  if (EOK != ret)
    {
      NSHAL_LOGERR ("STRNCPY_S failed]ret=%d.", ret);
      return -1;
    }

  if (!hal_is_script_valid (inst->data.dpdk_if.pci_addr))
    {
      NSHAL_LOGERR ("pci_addr is invalid]pci_addr=%s",
                    inst->data.dpdk_if.pci_addr);
      return -1;
    }

  if (dpdk_non_root_user && getuid ())
    {
      ret = dpdk_set_pci_permission (inst->data.dpdk_if.pci_addr);
      if (ret < 0)
        {
          NSHAL_LOGERR ("dpdk_set_pci_permission fail");
          return -1;
        }
    }

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_mlx_linkup
 Description  : linkup the port for mlx
                In bonding mode, mlx4 NICs should be set up manually,
                in order to make bond port up
 Input        : char* name
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_mlx_linkup (char *name)
{
  struct ifreq st_ifreq;
  int sock;
  int retVal;

  if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
      NSHAL_LOGERR ("socket fail]errno=%d", errno);
      return 2;
    }

  retVal = STRCPY_S (st_ifreq.ifr_name, sizeof (st_ifreq.ifr_name), name);

  if (EOK != retVal)
    {
      NSHAL_LOGERR ("STRCPY_S fail]");
      close (sock);
      return 1;
    }

  if (ioctl (sock, (uint64_t) SIOCGIFFLAGS, &st_ifreq) < 0)
    {
      NSHAL_LOGERR ("ioctl SIOCGIFFLAGS fail]errno=%d", errno);
      close (sock);
      return 3;
    }

  st_ifreq.ifr_flags |= IFF_UP;
  st_ifreq.ifr_flags |= IFF_RUNNING;

  if (ioctl (sock, (uint64_t) SIOCSIFFLAGS, &st_ifreq) < 0)
    {
      NSHAL_LOGERR ("ioctl SIOCSIFFLAGS fail]errno=%d", errno);
      close (sock);
      return 3;
    }

  close (sock);
  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_nonmlx_linkdown
 Description  : linkdown the port for nonmlx
 Input        : char* name
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_nonmlx_linkdown (char *name)
{
  int ret, ret_len;
  char script_cmmd[HAL_SCRIPT_LENGTH];
  char result_buf[HAL_SCRIPT_LENGTH];

  ret =
    hal_snprintf (script_cmmd, sizeof (script_cmmd), "sudo ifconfig %s down",
                  name);
  if (-1 == ret)
    {
      NSHAL_LOGERR ("spl_snprintf failed]");
      return -1;
    }

  ret_len = hal_run_script (script_cmmd, result_buf, sizeof (result_buf));
  NSHAL_LOGINF ("ifconfig]script_cmmd=%s,ret_len=%d", script_cmmd, ret_len);
  if (0 > ret_len)
    {
      NSHAL_LOGERR ("cannot able to ifconfig %s down", name);
      return -1;
    }

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_get_uio_by_pci_addr
 Description  : get uio
 Input        : char *pci_addr
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_get_uio_by_pci_addr (char *pci_addr)
{
  int i, ret, ret_len;
  char script_cmmd[HAL_SCRIPT_LENGTH];
  char result_buf[HAL_SCRIPT_LENGTH];

  for (i = 0; i < RTE_MAX_ETHPORTS; i++)
    {
      ret = hal_snprintf (script_cmmd, sizeof (script_cmmd),
                          "readlink ls /sys/class/uio/uio%d/device | awk -F '/' '{print $4}'",
                          i);
      if (-1 == ret)
        {
          NSHAL_LOGERR ("hal_snprintf fail]pci=%s,i=%d", pci_addr, i);
          return -1;
        }

      ret_len = hal_run_script (script_cmmd, result_buf, sizeof (result_buf));
      if (0 > ret_len)
        {
          NSHAL_LOGERR ("hal_run_script fail]pci=%s,i=%d", pci_addr, i);
          return -1;
        }
      if (strcmp (result_buf, pci_addr) == 0)
        return i;
    }

  return -1;
}

/*****************************************************************************
 Prototype    : dpdk_set_uio_permission
 Description  : set uio permission
 Input        : char *pci_addr
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_set_uio_permission (char *pci_addr)
{
  int ret, uio_idx;
  char file_path[HAL_SCRIPT_LENGTH / 2] = { 0 };

  /* get /dev/uio by pci addr */
  uio_idx = dpdk_get_uio_by_pci_addr (pci_addr);
  if (uio_idx < 0)
    {
      NSHAL_LOGERR ("dpdk_get_uio_by_pci_addr fail]pci_addr=%s", pci_addr);
      return -1;
    }

  NSHAL_LOGINF ("uio_idx]pci=%s,uio%d", pci_addr, uio_idx);

  /* change /dev/uio%u permission */
  if (SNPRINTF_S
      (file_path, sizeof (file_path), sizeof (file_path) - 1, "/dev/uio%d",
       uio_idx) < 0)
    {
      NSHAL_LOGERR ("SNPRINTF_S failed]uio%d", uio_idx);
      return -1;
    }
  sys_sleep_ns (0, 500000000);
  ret =
    chown (file_path, dpdk_non_root_user->pw_uid, dpdk_non_root_user->pw_gid);
  if (ret < 0)
    {
      NSHAL_LOGERR ("chown fail]file_path=%s,ret=%d,errno=%d", file_path, ret,
                    errno);
      return -1;
    }
  NSHAL_LOGWAR ("chown succ]file_path=%s,ret=%d", file_path, ret);

  ret = chmod (file_path, 0640);
  if (ret < 0)
    {
      NSHAL_LOGERR ("chmod fail]file_path=%s,ret=%d,errno=%d", file_path, ret,
                    errno);
      return -1;
    }
  NSHAL_LOGWAR ("chmod succ]file_path=%s,ret=%d", file_path, ret);

  /* change /sys/class/uio/uio%u/device/config permission */
  if (SNPRINTF_S
      (file_path, sizeof (file_path), sizeof (file_path) - 1,
       "/sys/class/uio/uio%d/device/config", uio_idx) < 0)
    {
      NSHAL_LOGERR ("SNPRINTF_S failed]uio%d", uio_idx);
      return -1;
    }

  ret =
    chown (file_path, dpdk_non_root_user->pw_uid, dpdk_non_root_user->pw_gid);
  if (ret < 0)
    {
      NSHAL_LOGERR ("chown fail]file_path=%s,ret=%d,errno=%d", file_path, ret,
                    errno);
      return -1;
    }
  NSHAL_LOGWAR ("chown succ]file_path=%s,ret=%d", file_path, ret);

  ret = chmod (file_path, 0640);
  if (ret < 0)
    {
      NSHAL_LOGERR ("chmod fail]file_path=%s,ret=%d,errno=%d", file_path, ret,
                    errno);
      return -1;
    }
  NSHAL_LOGWAR ("chmod succ]file_path=%s,ret=%d", file_path, ret);

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_get_nic_list_file
 Description  : get dpdk bind nic list file
 Input        : void
 Output       : char*
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC char *
dpdk_get_nic_list_file (void)
{
  int ret;
  static char buffer[HAL_MAX_PATH_LEN]; /* static so auto-zeroed */
  const char *directory = "/var/run";
  const char *home_dir = getenv ("HOME");

  if (getuid () != 0 && home_dir != NULL)
    directory = home_dir;

  ret =
    SNPRINTF_S (buffer, sizeof (buffer), sizeof (buffer) - 1,
                DPDK_NIC_LIST_FILE, directory);
  if (-1 == ret)
    {
      NSCOMM_LOGERR ("SNPRINTF_S failed]ret=%d", ret);
      return NULL;
    }

  return buffer;
}

/*****************************************************************************
 Prototype    : dpdk_bind_uio
 Description  : bind uio
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_bind_uio (netif_inst_t * inst)
{
  char script_cmmd[HAL_SCRIPT_LENGTH];
  char result_buf[HAL_SCRIPT_LENGTH];
  int ret, ret_len;

  result_buf[0] = '\0';

  if (strncmp ("mlx4_co", inst->data.dpdk_if.driver_name, (size_t) 7) == 0)
    {
      /*For MLX4: NIC should be set link up before rte_eth_dev_start() is called. */
      ret = dpdk_mlx_linkup (inst->data.dpdk_if.nic_name);
      if (0 != ret)
        {
          NSHAL_LOGERR ("set mlx linkup fail]nic_name=%s,ret=%d",
                        inst->data.dpdk_if.nic_name, ret);

          return -1;
        }
    }
  else
    {
      /*For other drivers: NIC should be set link down before bind uio. */
      ret = dpdk_nonmlx_linkdown (inst->data.dpdk_if.nic_name);
      if (-1 == ret)
        {
          NSHAL_LOGERR ("dpdk_nonmlx_linkdown fail]nic_name=%s",
                        inst->data.dpdk_if.nic_name);
          return -1;
        }

      /* save binded VF list to file /var/run/ip_module/.nstack_dpdk_nic_list */
      ret = hal_snprintf (script_cmmd, sizeof (script_cmmd),
                          "sudo %s"
                          "/dpdk-devbind.py -s | grep `ethtool -i %s| grep bus-info |awk '{print $2}'` >> %s 2>&1",
                          dpdk_tool_path, inst->data.dpdk_if.nic_name,
                          dpdk_get_nic_list_file ());

      if (-1 == ret)
        {
          NSHAL_LOGERR ("hal_snprintf fail]nic_name=%s",
                        inst->data.dpdk_if.nic_name);
          return -1;
        }

      ret_len =
        hal_run_script (script_cmmd, result_buf, sizeof (result_buf) - 1);
      NSHAL_LOGINF ("bind]script_cmmd=%s,ret_len=%d", script_cmmd, ret_len);
      if (0 > ret_len)
        {
          NSHAL_LOGERR ("hal_run_script fail]script_cmmd=%s", script_cmmd);
          return -1;
        }

      if (strncmp ("virtio", inst->data.dpdk_if.driver_name, (size_t) 6) == 0)
        {
          /* For Virtio NIC, should call "./devbind.sh ethX" to bind the VF */
          ret = hal_snprintf (script_cmmd, sizeof (script_cmmd),
                              "sudo %s"
                              "/dpdk-devbind.py  --bind=igb_uio `ethtool -i %s| grep bus-info |awk '{print $2}'`",
                              dpdk_tool_path, inst->data.dpdk_if.nic_name);
          //"sudo %s" "/devbind.sh " "%s", dpdk_tool_path, inst->data.dpdk_if.nic_name);

        }
      else
        {
          ret = hal_snprintf (script_cmmd, sizeof (script_cmmd),
                              "sudo %s" "/dpdk-devbind.py  --bind=igb_uio "
                              "%s", dpdk_tool_path,
                              inst->data.dpdk_if.nic_name);
        }

      if (-1 == ret)
        {
          NSHAL_LOGERR ("hal_snprintf failed");
          return -1;
        }

      ret_len =
        hal_run_script (script_cmmd, result_buf, sizeof (result_buf) - 1);
      NSHAL_LOGINF ("bind]script_cmmd=%s,retlen=%d", script_cmmd, ret_len);
      if (0 > ret_len)
        {
          NSHAL_LOGERR ("hal_run_script fail]script_cmmd=%s", script_cmmd);
          return -1;
        }

      if (dpdk_non_root_user && getuid ())
        {
          ret = dpdk_set_uio_permission (inst->data.dpdk_if.pci_addr);

          if (ret < 0)
            {
              NSHAL_LOGERR ("set_uio_permission fail]nic_name=%s",
                            inst->data.dpdk_if.nic_name);
              return -1;
            }
        }
    }

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_probe_pci
 Description  : probe pci
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_probe_pci (netif_inst_t * inst)
{
  int ret;
  uint16_t port_id;
  struct rte_eth_dev *eth_dev;
  char *pci_addr = inst->data.dpdk_if.pci_addr;

  ret = rte_eal_iopl_init ();
  if (0 != ret)
    {
      NSHAL_LOGERR ("rte_eal_iopl_init fail]pci_addr=%s,ret=%d", pci_addr,
                    ret);
      return -1;
    }

  ret = rte_eth_dev_attach (pci_addr, &port_id);
  if (0 != ret)
    {
      NSHAL_LOGWAR
        ("pci attach to DPDK fail, the pci may have attached, try to get port id]pci_addr=%s,ret=%d",
         pci_addr, ret);

      eth_dev = rte_eth_dev_allocated (inst->data.dpdk_if.nic_name);
      if (NULL != eth_dev && NULL != eth_dev->data)
        {
          port_id = eth_dev->data->port_id;
          ret = 0;
        }
    }

  if (!ret)
    {
      ret = dpdk_set_port (inst, port_id);

      if (0 == ret)
        {
          NSHAL_LOGINF ("set port success]pci_addr=%s,port_id=%u", pci_addr,
                        port_id);
        }
      else
        {
          NSHAL_LOGERR ("set port fail]pci_addr=%s,port_id=%u", pci_addr,
                        port_id);
          return -1;
        }
    }
  else
    {
      NSHAL_LOGERR ("get port fail]pci_addr=%s,ret=%d", pci_addr, ret);
      return -1;
    }

  /*[TA33635][2017-04-24][l00408818] Start: support bond mode */
  ret = rte_eal_devargs_add (RTE_DEVTYPE_WHITELISTED_PCI, pci_addr);
  if (!ret)
    {
      NSHAL_LOGINF ("pci attach to whitelist success]pci_addr=%s", pci_addr);
    }
  else
    {
      NSHAL_LOGERR ("pci attach to whitelist fail]pci_addr=%s", pci_addr);
      return -1;
    }
  /*[TA33635][2017-04-24][l00408818] End */

  return 0;
}

/*nic_name->driver_name->pci_addr->get port*/
/*****************************************************************************
 Prototype    : dpdk_open
 Description  : open the port
 Input        : netif_inst_t* inst
                const char* name
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_open (netif_inst_t * inst, const char *name)
{
  int ret;

  ret = dpdk_set_nic_name (inst, name);

  if (0 != ret)
    {
      NSHAL_LOGERR ("dpdk_set_nic_name fail]nic_name=%s, ret=%d", name, ret);
      return -1;
    }

  ret = dpdk_get_driver_name (inst);

  if (0 != ret)
    {
      NSHAL_LOGERR ("dpdk_get_driver_name fail]nic_name=%s, ret=%d", name,
                    ret);
      return -1;
    }

  ret = dpdk_get_pci_addr (inst);

  if (0 != ret)
    {
      NSHAL_LOGERR ("dpdk_get_pci_addr fail]nic_name=%s, ret=%d", name, ret);
      return -1;
    }

  ret = dpdk_bind_uio (inst);

  if (0 != ret)
    {
      NSHAL_LOGERR ("dpdk_bind_uio fail]nic_name=%s, ret=%d", name, ret);
      return -1;
    }

  ret = dpdk_probe_pci (inst);

  if (0 != ret)
    {
      NSHAL_LOGERR ("dpdk_probe_pci fail]nic_name=%s, ret=%d", name, ret);
      return -1;
    }

  NSHAL_LOGINF ("open port succ]port_id=%u, nic_name=%s",
                inst->data.dpdk_if.port_id, inst->data.dpdk_if.nic_name);

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_close
 Description  : close the port
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_close (netif_inst_t * inst)
{
  int i;

  /* close slave NIC first */
  for (i = 0; i < inst->data.dpdk_if.slave_num; i++)
    {
      rte_eth_dev_close (inst->data.dpdk_if.slave_port[i]);
      NSHAL_LOGINF ("close slave port succ]port_id=%u, nic_name=%s",
                    inst->data.dpdk_if.slave_port[i],
                    inst->data.dpdk_if.nic_name);
    }

  rte_eth_dev_close (inst->data.dpdk_if.port_id);
  NSHAL_LOGINF ("close port succ]port_id=%u, nic_name=%s",
                inst->data.dpdk_if.port_id, inst->data.dpdk_if.nic_name);
  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_get_queue_conf
 Description  : get the port queue configure
 Input        : netif_inst_t* inst
                struct rte_eth_rxconf** rx_conf
                struct rte_eth_txconf** tx_conf
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_get_queue_conf (netif_inst_t * inst,
                     struct rte_eth_rxconf **rx_conf,
                     struct rte_eth_txconf **tx_conf)
{
  static struct rte_eth_dev_info slave_dev_info;

  if (strncmp ("igb", inst->data.dpdk_if.driver_name, (size_t) 3) == 0)
    {
      *rx_conf = (struct rte_eth_rxconf *) &rx_conf_default_igb;
      *tx_conf = (struct rte_eth_txconf *) &tx_conf_default_igb;
      NSHAL_LOGINF ("igb config is enable]port_id=%u",
                    inst->data.dpdk_if.port_id);
    }
  else if (strncmp ("ixgbe", inst->data.dpdk_if.driver_name, (size_t) 5) == 0)
    {
      *rx_conf = (struct rte_eth_rxconf *) &rx_conf_default_ixgbe;
      *tx_conf = (struct rte_eth_txconf *) &tx_conf_default_ixgbe;
      NSHAL_LOGINF ("igxbe config is enable]port_id=%u",
                    inst->data.dpdk_if.port_id);
    }
  else if (strncmp ("bond", inst->data.dpdk_if.driver_name, (size_t) 4) == 0)
    {
      *rx_conf = NULL;
      rte_eth_dev_info_get (inst->data.dpdk_if.slave_port[0],
                            &slave_dev_info);
      *tx_conf = &(slave_dev_info.default_txconf);
      NSHAL_LOGINF ("bond config is enable]port_id=%u",
                    inst->data.dpdk_if.port_id);
    }
  else
    {
      *rx_conf = NULL;
      *tx_conf = NULL;
      NSHAL_LOGINF ("default config is enable]port_id=%u",
                    inst->data.dpdk_if.port_id);
    }
  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_get_port_conf
 Description  : get the port configure
 Input        : netif_inst_t* inst
                struct rte_eth_conf** port_conf
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC void
dpdk_get_port_conf (netif_inst_t * inst, struct rte_eth_conf **port_conf)
{
  if (strncmp ("virtio", inst->data.dpdk_if.driver_name, (size_t) 6) == 0)
    {
      *port_conf = &port_conf_default_virtio;
    }
  else if (strncmp ("bond", inst->data.dpdk_if.driver_name, (size_t) 4) == 0)
    {
      *port_conf = &port_conf_default_bond;
    }
  else
    {
      *port_conf = &port_conf_default_normal;
    }

  (*port_conf)->rxmode.hw_vlan_filter = inst->data.dpdk_if.hw_vlan_filter;
  (*port_conf)->rxmode.hw_vlan_strip = inst->data.dpdk_if.hw_vlan_strip;
}

/*****************************************************************************
 Prototype    : dpdk_setup_port
 Description  : setup the port
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_setup_port (netif_inst_t * inst)
{
  int ret;
  uint32_t i;
  struct rte_eth_conf *port_conf;
  struct rte_eth_rxconf *rx_conf;
  struct rte_eth_txconf *tx_conf;

  uint8_t port_id = inst->data.dpdk_if.port_id;
  struct rte_mempool **mp =
    (struct rte_mempool **) inst->data.dpdk_if.rx_pool;
  uint32_t *rx_ring_size = inst->data.dpdk_if.rx_ring_size;
  uint32_t *tx_ring_size = inst->data.dpdk_if.tx_ring_size;
  uint32_t rx_queue_num = inst->data.dpdk_if.rx_queue_num;
  uint32_t tx_queue_num = inst->data.dpdk_if.tx_queue_num;

  dpdk_get_port_conf (inst, &port_conf);

  ret =
    rte_eth_dev_configure (port_id, rx_queue_num, tx_queue_num, port_conf);
  if (ret < 0)
    {
      NSHAL_LOGERR ("rte_eth_dev_configure]port_id=%u,ret=%d", port_id, ret);
      return ret;
    }

  if (dpdk_get_queue_conf (inst, &rx_conf, &tx_conf) < 0)
    {
      NSHAL_LOGERR ("dpdk_get_queue_conf failed]inst=%p,rx_conf=%p", inst,
                    rx_conf);
      return -1;
    }
  /* fix "FORTIFY.Out-of-Bounds_Read" type codedex issue CID 33436 */
  if (rx_queue_num > HAL_ETH_MAX_QUEUE_NUM
      || tx_queue_num > HAL_ETH_MAX_QUEUE_NUM)
    {
      NSHAL_LOGERR
        ("queue num error]rx_queue_num=%u, tx_queue_num=%u, HAL_ETH_MAX_QUEUE_NUM=%d",
         rx_queue_num, tx_queue_num, HAL_ETH_MAX_QUEUE_NUM);

      return -1;
    }

  for (i = 0; i < rx_queue_num; i++)
    {
      ret =
        rte_eth_rx_queue_setup (port_id, i, rx_ring_size[i], SOCKET_ID_ANY,
                                rx_conf, mp[i]);
      if (ret < 0)
        {
          NSHAL_LOGERR ("rx queue setup fail]index=%u,port_id=%u,ret=%d", i,
                        port_id, ret);
          return ret;
        }
    }

  for (i = 0; i < tx_queue_num; i++)
    {
      ret =
        rte_eth_tx_queue_setup (port_id, i, tx_ring_size[i], SOCKET_ID_ANY,
                                tx_conf);

      if (ret < 0)
        {
          NSHAL_LOGERR ("tx queue setup fail]q=%u,ret=%d", i, ret);
          return ret;
        }
    }

  rte_eth_promiscuous_enable (port_id);

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_start
 Description  : start the port
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_start (netif_inst_t * inst)
{
  int ret;
  struct ether_addr eth_addr;

  ret = dpdk_setup_port (inst);

  if (ret < 0)
    {
      NSHAL_LOGERR ("call dpdk_setup_port fail]ret=%d", ret);
      return ret;
    }

  ret = rte_eth_dev_start (inst->data.dpdk_if.port_id);
  if (ret < 0)
    {
      NSHAL_LOGERR ("rte_eth_dev_start fail]ret=%d", ret);
      return ret;
    }

  rte_eth_macaddr_get (inst->data.dpdk_if.port_id, &eth_addr);
  NSHAL_LOGINF ("port_id=%u,nic_name=%s,mac=%02X:%02X:%02X:%02X:%02X:%02X",
                inst->data.dpdk_if.port_id,
                inst->data.dpdk_if.nic_name,
                eth_addr.addr_bytes[0],
                eth_addr.addr_bytes[1],
                eth_addr.addr_bytes[2],
                eth_addr.addr_bytes[3],
                eth_addr.addr_bytes[4], eth_addr.addr_bytes[5]);

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_stop
 Description  : stop the port
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_stop (netif_inst_t * inst)
{
  int i;

  /* stop slave NIC first */
  for (i = 0; i < inst->data.dpdk_if.slave_num; i++)
    {
      rte_eth_dev_stop (inst->data.dpdk_if.slave_port[i]);
      NSHAL_LOGINF ("stop slave port succ]port_id=%u, nic_name=%s",
                    inst->data.dpdk_if.slave_port[i],
                    inst->data.dpdk_if.nic_name);
    }
  rte_eth_dev_stop (inst->data.dpdk_if.port_id);

  NSHAL_LOGINF ("stop port succ]port_id=%u, nic_name=%s",
                inst->data.dpdk_if.port_id, inst->data.dpdk_if.nic_name);
  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_get_mtu
 Description  : get the port mtu
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC uint32_t
dpdk_get_mtu (netif_inst_t * inst)
{
  uint32_t mtu;

  if (rte_eth_dev_get_mtu (inst->data.dpdk_if.port_id, (uint16_t *) & mtu))
    {
      return 0;
    }

  return mtu;
}

/*****************************************************************************
 Prototype    : dpdk_get_macaddr
 Description  : get the port mac addr
 Input        : netif_inst_t* inst
                void* mac_addr
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_get_macaddr (netif_inst_t * inst, void *mac_addr)
{
  /*bond port */
  int primary_port = rte_eth_bond_primary_get (inst->data.dpdk_if.port_id);
  if (0 <= primary_port)
    {
      rte_eth_macaddr_get ((uint8_t) primary_port,
                           (struct ether_addr *) mac_addr);

      NSHAL_LOGDBG
        ("primary_port_id=%u,nic_name=%s,mac=%02X:%02X:%02X:%02X:%02X:%02X",
         primary_port, inst->data.dpdk_if.nic_name,
         ((struct ether_addr *) mac_addr)->addr_bytes[0],
         ((struct ether_addr *) mac_addr)->addr_bytes[1],
         ((struct ether_addr *) mac_addr)->addr_bytes[2],
         ((struct ether_addr *) mac_addr)->addr_bytes[3],
         ((struct ether_addr *) mac_addr)->addr_bytes[4],
         ((struct ether_addr *) mac_addr)->addr_bytes[5]);
    }
  /*normal port */
  else
    {
      rte_eth_macaddr_get (inst->data.dpdk_if.port_id,
                           (struct ether_addr *) mac_addr);

      NSHAL_LOGDBG
        ("normal_port_id=%u,nic_name=%s,mac=%02X:%02X:%02X:%02X:%02X:%02X",
         inst->data.dpdk_if.port_id, inst->data.dpdk_if.nic_name,
         ((struct ether_addr *) mac_addr)->addr_bytes[0],
         ((struct ether_addr *) mac_addr)->addr_bytes[1],
         ((struct ether_addr *) mac_addr)->addr_bytes[2],
         ((struct ether_addr *) mac_addr)->addr_bytes[3],
         ((struct ether_addr *) mac_addr)->addr_bytes[4],
         ((struct ether_addr *) mac_addr)->addr_bytes[5]);
    }

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_capa_convert
 Description  : convert format from dpdk to hal
 Input        : const struct rte_eth_dev_info* dev_info
                hal_netif_capa_t* capa
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC inline void
dpdk_capa_convert (const struct rte_eth_dev_info *dev_info,
                   hal_netif_capa_t * capa)
{
  int retVal =
    MEMSET_S (capa, sizeof (hal_netif_capa_t), 0, sizeof (hal_netif_capa_t));
  if (EOK != retVal)
    {
      NSHAL_LOGERR ("MEMSET_S fail]retVal=%d", retVal);
    }

  capa->tx_offload_capa = dev_info->tx_offload_capa;
}

/*****************************************************************************
 Prototype    : dpdk_get_capability
 Description  : get the port capability
 Input        : netif_inst_t* inst
                hal_netif_capa_t* capa
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_get_capability (netif_inst_t * inst, hal_netif_capa_t * capa)
{
  struct rte_eth_dev_info dev_info;

  rte_eth_dev_info_get (inst->data.dpdk_if.port_id, &dev_info);
  dpdk_capa_convert (&dev_info, capa);
  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_recv
 Description  : recv packet from the port
 Input        : netif_inst_t* inst
                uint16_t queue_id
                struct common_mem_mbuf** rx_pkts
                uint16_t nb_pkts
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC uint16_t
dpdk_recv (netif_inst_t * inst, uint16_t queue_id,
           hal_mbuf_t ** rx_pkts, uint16_t nb_pkts)
{
  return hal_rte_eth_rx_burst (inst->data.dpdk_if.port_id, queue_id,
                               (struct rte_mbuf **) rx_pkts, nb_pkts);
}

/*****************************************************************************
 Prototype    : dpdk_send
 Description  : send packet to the port
 Input        : netif_inst_t* inst
                uint16_t queue_id
                struct common_mem_mbuf** tx_pkts
                uint16_t nb_pkts
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC uint16_t
dpdk_send (netif_inst_t * inst, uint16_t queue_id,
           hal_mbuf_t ** tx_pkts, uint16_t nb_pkts)
{
  return hal_rte_eth_tx_burst (inst->data.dpdk_if.port_id, queue_id,
                               (struct rte_mbuf **) tx_pkts, nb_pkts);
}

/*****************************************************************************
 Prototype    : dpdk_link_status
 Description  : get link status form the port
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC uint32_t
dpdk_link_status (netif_inst_t * inst)
{
  struct rte_eth_link eth_link;

  /* add log output when failed */
  int retVal = MEMSET_S (&eth_link, sizeof (struct rte_eth_link), 0,
                         sizeof (struct rte_eth_link));
  if (EOK != retVal)
    {
      NSHAL_LOGERR ("MEMSET_S fail]retVal=%d", retVal);
    }

  rte_eth_link_get (inst->data.dpdk_if.port_id, &eth_link);

  return eth_link.link_status;
}

/*****************************************************************************
 Prototype    : dpdk_stats_convert
 Description  : convert format from dpdk to hal
 Input        : const struct rte_eth_stats* rte_stats
                hal_netif_stats_t* stats
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC inline void
dpdk_stats_convert (const struct rte_eth_stats *rte_stats,
                    hal_netif_stats_t * stats)
{
  int i;

  /* give fail error number when failed */
  int retVal = MEMSET_S (stats, sizeof (hal_netif_stats_t), 0,
                         sizeof (hal_netif_stats_t));
  if (EOK != retVal)
    {
      NSHAL_LOGERR ("MEMSET_S fail]retVal=%d", retVal);
    }

  stats->ipackets = rte_stats->ipackets;
  stats->opackets = rte_stats->opackets;
  stats->ibytes = rte_stats->ibytes;
  stats->obytes = rte_stats->obytes;
  stats->imissed = rte_stats->imissed;
  stats->ierrors = rte_stats->ierrors;
  stats->oerrors = rte_stats->oerrors;
  stats->rx_nombuf = rte_stats->rx_nombuf;

  for (i = 0; i < HAL_ETH_QUEUE_STAT_CNTRS; i++)
    {
      stats->q_ipackets[i] = rte_stats->q_ipackets[i];
      stats->q_opackets[i] = rte_stats->q_opackets[i];
      stats->q_ibytes[i] = rte_stats->q_ibytes[i];
      stats->q_obytes[i] = rte_stats->q_obytes[i];
      stats->q_errors[i] = rte_stats->q_errors[i];
    }
}

/*****************************************************************************
 Prototype    : dpdk_stats
 Description  : get stats form the port
 Input        : netif_inst_t* inst
                hal_netif_stats_t* stats
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_stats (netif_inst_t * inst, hal_netif_stats_t * stats)
{
  int ret;
  struct rte_eth_stats rte_stats;

  ret = rte_eth_stats_get (inst->data.dpdk_if.port_id, &rte_stats);
  if (ret == 0)
    {
      dpdk_stats_convert (&rte_stats, stats);
      return 0;
    }

  return -1;
}

/*****************************************************************************
 Prototype    : dpdk_stats_reset
 Description  : reset stats to the port
 Input        : netif_inst_t* inst
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_stats_reset (netif_inst_t * inst)
{
  rte_eth_stats_reset (inst->data.dpdk_if.port_id);
  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_config
 Description  : config the port queue and ring
 Input        : netif_inst_t* inst
                hal_netif_config_t* conf
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_config (netif_inst_t * inst, hal_netif_config_t * conf)
{
  uint32_t i;

  inst->data.dpdk_if.hw_vlan_filter = conf->bit.hw_vlan_filter;
  inst->data.dpdk_if.hw_vlan_strip = conf->bit.hw_vlan_strip;

  inst->data.dpdk_if.rx_queue_num = conf->rx.queue_num;
  /* fix Buffer Overflow type code-dex issue */
  if (inst->data.dpdk_if.rx_queue_num > HAL_ETH_MAX_QUEUE_NUM)
    {
      NSHAL_LOGERR
        ("rx queue num error]rx_queue_num=%u, HAL_ETH_MAX_QUEUE_NUM=%d",
         inst->data.dpdk_if.rx_queue_num, HAL_ETH_MAX_QUEUE_NUM);

      return -1;
    }

  for (i = 0; i < inst->data.dpdk_if.rx_queue_num; i++)
    {
      inst->data.dpdk_if.rx_ring_size[i] = conf->rx.ring_size[i];
      inst->data.dpdk_if.rx_pool[i] =
        (struct rte_mempool *) conf->rx.ring_pool[i];
    }

  inst->data.dpdk_if.tx_queue_num = conf->tx.queue_num;
  /* fix "FORTIFY.Out-of-Bounds_Read--Off-by-One" type codedex issue */
  if (inst->data.dpdk_if.tx_queue_num > HAL_ETH_MAX_QUEUE_NUM)
    {
      NSHAL_LOGERR
        ("tx queue num error]rx_queue_num=%u, HAL_ETH_MAX_QUEUE_NUM=%d",
         inst->data.dpdk_if.tx_queue_num, HAL_ETH_MAX_QUEUE_NUM);

      return -1;
    }
  for (i = 0; i < inst->data.dpdk_if.tx_queue_num; i++)
    {
      inst->data.dpdk_if.tx_ring_size[i] = conf->tx.ring_size[i];
    }

  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_bond_config
 Description  : config the port for bond mode
 Input        : netif_inst_t* inst
                uint8_t slave_num
                netif_inst_t* slave_inst[]
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_bond_config (netif_inst_t * inst, uint8_t slave_num,
                  netif_inst_t * slave_inst[])
{
  int ret;
  uint32_t i, queue;

  inst->data.dpdk_if.slave_num = slave_num;

  for (i = 0; i < slave_num; i++)
    {
      inst->data.dpdk_if.slave_port[i] = slave_inst[i]->data.dpdk_if.port_id;

      if (0 == i)
        {
          inst->data.dpdk_if.hw_vlan_filter =
            slave_inst[i]->data.dpdk_if.hw_vlan_filter;
          inst->data.dpdk_if.hw_vlan_strip =
            slave_inst[i]->data.dpdk_if.hw_vlan_strip;
          inst->data.dpdk_if.rx_queue_num =
            slave_inst[i]->data.dpdk_if.rx_queue_num;
          inst->data.dpdk_if.tx_queue_num =
            slave_inst[i]->data.dpdk_if.tx_queue_num;

          /*will be used in function dpdk_get_queue_conf and dpdk_get_port_conf */
          ret =
            STRCPY_S (inst->data.dpdk_if.driver_name,
                      sizeof (inst->data.dpdk_if.driver_name), "bond");

          if (EOK != ret)
            {
              NSHAL_LOGERR ("STRCPY_S failed]ret=%d.", ret);
              return -1;
            }

          for (queue = 0; queue < HAL_ETH_MAX_QUEUE_NUM; queue++)
            {
              inst->data.dpdk_if.rx_pool[queue] =
                slave_inst[i]->data.dpdk_if.rx_pool[queue];
              inst->data.dpdk_if.rx_ring_size[queue] =
                slave_inst[i]->data.dpdk_if.rx_ring_size[queue];
              inst->data.dpdk_if.tx_ring_size[queue] =
                slave_inst[i]->data.dpdk_if.tx_ring_size[queue];
            }
        }
    }

  return 0;

}

/*****************************************************************************
 Prototype    : dpdk_bond
 Description  : bond port
 Input        : netif_inst_t* inst
                const char* bond_name
                uint8_t slave_num
                netif_inst_t* slave_inst[]
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_bond (netif_inst_t * inst, const char *bond_name, uint8_t slave_num,
           netif_inst_t * slave_inst[])
{
  int i, ret;
  int port;
  struct ether_addr eth_addr;

  /* check if all the slaves' drivers are same, not support different drivers */
  for (i = 1; i < slave_num; i++)
    {
      if (strncmp
          (slave_inst[0]->data.dpdk_if.driver_name,
           slave_inst[i]->data.dpdk_if.driver_name,
           strlen (slave_inst[0]->data.dpdk_if.driver_name)))
        {
          NSHAL_LOGERR
            ("dpdk does not support different types of network card]slave[0]=%s, slave[%i]=%s",
             slave_inst[0]->data.dpdk_if.driver_name, i,
             slave_inst[i]->data.dpdk_if.driver_name);
          return -1;
        }
    }

  ret = dpdk_set_nic_name (inst, bond_name);

  if (0 != ret)
    {
      NSHAL_LOGERR ("dpdk_set_nic_name fail]ret=%d", ret);
      return -1;
    }

  port =
    rte_eth_bond_create (bond_name, BONDING_MODE_ACTIVE_BACKUP, SOCKET_ID_0);
  if (port < 0)
    {
      NSHAL_LOGERR ("rte_eth_bond_create fail]ret=%i", ret);
      return -1;
    }

  ret = dpdk_set_port (inst, (uint8_t) port);

  if (ret < 0)
    {
      NSHAL_LOGERR ("dpdk_set_port fail]ret=%i", ret);
      return ret;
    }

  ret = dpdk_bond_config (inst, slave_num, slave_inst);

  if (ret < 0)
    {
      NSHAL_LOGERR ("dpdk_bond_config fail]ret=%i", ret);
      return ret;
    }

  ret = dpdk_setup_port (inst);

  if (ret < 0)
    {
      NSHAL_LOGERR ("dpdk_setup_port fail]ret=%i", ret);
      return ret;
    }

  for (i = 0; i < slave_num; i++)
    {
      NSHAL_LOGINF ("add slave port_id=%u, nic_name=%s",
                    slave_inst[i]->data.dpdk_if.port_id,
                    slave_inst[i]->data.dpdk_if.nic_name);

      if (rte_eth_bond_slave_add
          ((uint8_t) port, slave_inst[i]->data.dpdk_if.port_id) == -1)
        {
          NSHAL_LOGERR ("adding slave (%u) to bond (%u) failed]",
                        slave_inst[i]->data.dpdk_if.port_id, port);
          return -1;
        }
    }

  rte_eth_macaddr_get (slave_inst[0]->data.dpdk_if.port_id, &eth_addr);

  ret = rte_eth_bond_mac_address_set ((uint8_t) port, &eth_addr);
  if (ret < 0)
    {
      NSHAL_LOGERR ("rte_eth_bond_mac_address_set fail]ret=%i", ret);
      return ret;
    }

  ret = rte_eth_dev_start (inst->data.dpdk_if.port_id);
  if (ret < 0)
    {
      NSHAL_LOGERR ("rte_eth_dev_start fail]ret=%i, port_id=%d", ret,
                    inst->data.dpdk_if.port_id);
      return ret;
    }

  NSHAL_LOGINF ("port_id=%d,nic_name=%s,mac=%02X:%02X:%02X:%02X:%02X:%02X",
                port,
                inst->data.dpdk_if.nic_name,
                eth_addr.addr_bytes[0],
                eth_addr.addr_bytes[1],
                eth_addr.addr_bytes[2],
                eth_addr.addr_bytes[3],
                eth_addr.addr_bytes[4], eth_addr.addr_bytes[5]);
  return 0;
}

/*****************************************************************************
 Prototype    : dpdk_add_mcastaddr
 Description  : add mcastaddr to the port
 Input        : netif_inst_t* inst
                void* mc_addr_set
                void* mc_addr
                uint32_t nb_mc_addr
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_set_mcastaddr (netif_inst_t * inst, void *mc_addr_set,
                    void *mc_addr, uint32_t nb_mc_addr)
{
  uint8_t port = inst->data.dpdk_if.port_id;
  int iRetVal;

  NSHAL_LOGINF ("mc_addr_set number=%u", nb_mc_addr);
  iRetVal = rte_eth_dev_set_mc_addr_list (port, mc_addr_set, nb_mc_addr);
  if (iRetVal != 0)
    {
      NSHAL_LOGWAR ("fail to set_mc_addr_list]port=%u,ret=%d", port, iRetVal);
    }

  return iRetVal;
}

/*****************************************************************************
 Prototype    : dpdk_add_mac_addr
 Description  : add mcastaddr to the port
 Input        : netif_inst_t* inst
                void* mc_addr
 Output       : None
 Return Value : NSTACK_STATIC int
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_add_mac_addr (netif_inst_t * inst, void *mc_addr)
{
  uint8_t port = inst->data.dpdk_if.port_id;
  int iRetVal;

  /* for MLX: set_mc_addr_list() is not callback, so call mac_addr_add() instead */
  iRetVal = rte_eth_dev_mac_addr_add (port, mc_addr, 0);
  if (0 != iRetVal)
    {
      NSHAL_LOGWAR ("fail to add_mac_addr]port=%u,ret=%d", port, iRetVal);
    }

  return iRetVal;
}

/*****************************************************************************
 Prototype    : dpdk_rmv_mac_addr
 Description  : remove mcastaddr to the port
 Input        : netif_inst_t* inst
                void* mc_addr
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_rmv_mac_addr (netif_inst_t * inst, void *mc_addr)
{
  uint8_t port = inst->data.dpdk_if.port_id;
  int iRetVal;

  /* for MLX: set_mc_addr_list() is not callback, so call mac_addr_remove() instead */
  iRetVal = rte_eth_dev_mac_addr_remove (port, mc_addr);
  if (0 != iRetVal)
    {
      NSHAL_LOGWAR ("fail to rmv_mac_addr]port=%u,ret=%d", port, iRetVal);
    }

  return iRetVal;
}

/*****************************************************************************
 Prototype    : dpdk_allmcast
 Description  : set allmcast mode to the port
 Input        : netif_inst_t* inst
                uint8_t enable
 Output       : None
 Return Value : NSTACK_STATIC
 Calls        :
 Called By    :

*****************************************************************************/
NSTACK_STATIC int
dpdk_allmcast (netif_inst_t * inst, uint8_t enable)
{
  if (enable)
    {
      rte_eth_allmulticast_enable (inst->data.dpdk_if.port_id);
    }
  else
    {
      rte_eth_allmulticast_disable (inst->data.dpdk_if.port_id);
    }

  return 0;
}

const netif_ops_t dpdk_netif_ops = {
  .name = "dpdk",
  .init_global = dpdk_init_global,
  .init_local = dpdk_init_local,
  .open = dpdk_open,
  .close = dpdk_close,
  .start = dpdk_start,
  .stop = dpdk_stop,
  .bond = dpdk_bond,
  .mtu = dpdk_get_mtu,
  .macaddr = dpdk_get_macaddr,
  .capability = dpdk_get_capability,
  .recv = dpdk_recv,
  .send = dpdk_send,
  .link_status = dpdk_link_status,
  .stats = dpdk_stats,
  .stats_reset = dpdk_stats_reset,
  .config = dpdk_config,
  .mcastaddr = dpdk_set_mcastaddr,
  .add_mac = dpdk_add_mac_addr,
  .rmv_mac = dpdk_rmv_mac_addr,
  .allmcast = dpdk_allmcast
};

HAL_IO_REGISTER (dpdk, &dpdk_netif_ops);
