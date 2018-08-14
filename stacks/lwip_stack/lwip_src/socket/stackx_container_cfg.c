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
#include <fcntl.h>
#include <arpa/inet.h>
#include "stackx_cfg.h"
#include "json.h"
#include "nstack_log.h"
#include "spl_def.h"
#include "nstack_securec.h"
#include "stackx_ip_addr.h"

sbr_container_ip_group g_container_ip_group;

/*****************************************************************************
*   Prototype    : sbr_parse_container_ip_json
*   Description  : parse port json
*   Input        : char* param
*   Output       : None
*   Return Value : static void
*   Calls        :
*   Called By    :
*
*****************************************************************************/
static void
sbr_parse_container_ip_json (char *param)
{
  int retval;
  struct json_object *obj = json_tokener_parse (param);
  struct json_object *container_id_obj = NULL;
  struct json_object *ports_list_obj = NULL;
  struct json_object *ip_cidr_list_obj = NULL;

  if (!obj)
    {
      NSSBR_LOGERR ("json_tokener_parse failed");
      return;
    }

  json_object_object_get_ex (obj, "containerID", &container_id_obj);
  if (!container_id_obj)
    {
      NSSBR_LOGERR ("can't get containerID");
      goto RETURN_ERROR;
    }

  json_object_object_get_ex (obj, "ports_list", &ports_list_obj);
  if (ports_list_obj)
    {
      int i;
      int port_num = json_object_array_length (ports_list_obj);

      if (0 == port_num)
        {
          NSSBR_LOGERR ("port num is 0");
          goto RETURN_ERROR;
        }

      for (i = 0; i < port_num; i++)
        {
          struct json_object *port_obj =
            json_object_array_get_idx (ports_list_obj, i);
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
                      char tmp[32] = { 0 };
                      const char *ip_cidr =
                        json_object_get_string (ip_cidr_obj);
                      if ((NULL == ip_cidr) || (ip_cidr[0] == 0))
                        {
                          NSSBR_LOGERR ("ip is empty");
                          goto RETURN_ERROR;
                        }

                      const char *sub = strstr (ip_cidr, "/");
                      if ((NULL == sub)
                          || (sizeof (tmp) - 1 <
                              (unsigned int) (sub - ip_cidr))
                          || (strlen (sub) > sizeof (tmp) - 1))
                        {
                          NSSBR_LOGERR ("ip format is not ok");
                          goto RETURN_ERROR;
                        }

                      retval =
                        STRNCPY_S (tmp, sizeof (tmp), ip_cidr,
                                   (size_t) (sub - ip_cidr));
                      if (EOK != retval)
                        {
                          NSSBR_LOGERR ("STRNCPY_S failed]ret=%d", retval);
                          goto RETURN_ERROR;
                        }

                      struct in_addr addr;
                      retval =
                        MEMSET_S (&addr, sizeof (addr), 0, sizeof (addr));
                      if (EOK != retval)
                        {
                          NSSBR_LOGERR ("MEMSET_S failed]ret=%d", retval);
                          goto RETURN_ERROR;
                        }

                      retval = spl_inet_aton (tmp, &addr);
                      if (0 == retval)
                        {
                          NSSBR_LOGERR ("spl_inet_aton failed]ret=%d",
                                        retval);
                          goto RETURN_ERROR;
                        }

                      g_container_ip_group.
                        ip_array[g_container_ip_group.ip_num].ip =
                        addr.s_addr;
                      int mask_len = atoi (sub + 1);
                      if ((mask_len <= 0) || (mask_len > 32))
                        {
                          NSSBR_LOGERR ("mask len is not ok");
                          goto RETURN_ERROR;
                        }

                      g_container_ip_group.
                        ip_array[g_container_ip_group.ip_num].mask_len =
                        (u32) mask_len;
                      g_container_ip_group.ip_num++;

                      if (g_container_ip_group.ip_num >=
                          SBR_MAX_CONTAINER_IP_NUM)
                        {
                          NSSBR_LOGWAR ("container ip num is full]ip_num=%u",
                                        g_container_ip_group.ip_num);
                          goto RETURN_OK;
                        }
                    }
                }
            }
        }
    }
  else
    {
      NSSBR_LOGERR ("can't get ports_list");
      goto RETURN_ERROR;
    }

RETURN_OK:
  json_object_put (obj);
  NSSBR_LOGINF ("container ip num is %u", g_container_ip_group.ip_num);
  u32 idx;
  for (idx = 0; idx < g_container_ip_group.ip_num; ++idx)
    {
      NSSBR_LOGDBG ("container ip=0x%08x",
                    g_container_ip_group.ip_array[idx].ip);
    }
  return;

RETURN_ERROR:
  json_object_put (obj);
  if (MEMSET_S
      (&g_container_ip_group, sizeof (sbr_container_ip_group), 0,
       sizeof (sbr_container_ip_group)) != EOK)
    {
      NSSBR_LOGERR ("MEMSET_S failed");
    }
}

/*****************************************************************************
*   Prototype    : sbr_get_cfg_path
*   Description  : get cfg path
*   Input        : None
*   Output       : None
*   Return Value : NSTACK_STATIC const char*
*   Calls        :
*   Called By    :
*
*****************************************************************************/
NSTACK_STATIC const char *
sbr_get_cfg_path ()
{
  static char cfg_file[SBR_MAX_CFG_PATH_LEN] = "/canal/output/portinfo.json";
  return cfg_file;
}

/*****************************************************************************
*   Prototype    : sbr_init_cfg
*   Description  : init cfg from file
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_init_cfg ()
{
  int ret;
  off_t file_len = 0;
  off_t buff_len = 0;
  char *buff = NULL;
  const char *cfg_file = sbr_get_cfg_path ();   /* no need check ret */

  int fp = open (cfg_file, O_RDONLY);

  if (fp < 0)
    {
      NSSBR_LOGWAR ("failed to open file]file name=%s", cfg_file);
      goto RETURN_ERROR;
    }

  file_len = lseek (fp, 0, SEEK_END);
  if (file_len <= 0)
    {
      NSSBR_LOGWAR ("failed to get file len]file name=%s", cfg_file);
      goto RETURN_ERROR;
    }

  if (file_len > SBR_MAX_CFG_FILE_SIZE)
    {
      NSSBR_LOGWAR
        ("file len is too big]file len=%d, max len=%d, file name=%s",
         file_len, SBR_MAX_CFG_FILE_SIZE, cfg_file);
      goto RETURN_ERROR;
    }

  ret = lseek (fp, 0, SEEK_SET);
  if (ret < 0)
    {
      NSSBR_LOGWAR ("seek to start failed]file name=%s", cfg_file);
      goto RETURN_ERROR;
    }

  buff_len = file_len + 1;
  buff = (char *) malloc (buff_len);
  if (!buff)
    {
      NSSBR_LOGWAR ("malloc buff failed]buff_len=%d", buff_len);
      goto RETURN_ERROR;
    }

  ret = MEMSET_S (buff, buff_len, 0, buff_len);
  if (EOK != ret)
    {
      NSSBR_LOGWAR ("MEMSET_S failed]ret=%d.", ret);
      goto RETURN_ERROR;
    }

  ret = read (fp, buff, buff_len - 1);
  if (ret <= 0)
    {
      NSSBR_LOGWAR ("read failed]ret=%d", ret);
      goto RETURN_ERROR;
    }

  sbr_parse_container_ip_json (buff);
  close (fp);
  free (buff);
  return 0;

RETURN_ERROR:
  if (fp >= 0)
    {
      close (fp);
    }

  if (buff)
    {
      free (buff);
    }

  return -1;
}

/*****************************************************************************
*   Prototype    : sbr_get_src_ip
*   Description  : get src ip from cfg
*   Input        : u32 dst_ip
*                  u32* src_ip
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
sbr_get_src_ip (u32 dst_ip, u32 * src_ip)
{
  if (!src_ip)
    {
      NSSBR_LOGERR ("src_ip is NULL");
      return -1;
    }

  u32 i;
  for (i = 0; i < g_container_ip_group.ip_num; ++i)
    {
      unsigned int mask = ~0;
      mask = (mask << (32 - g_container_ip_group.ip_array[i].mask_len));
      mask = htonl (mask);
      if ((dst_ip & mask) == (g_container_ip_group.ip_array[i].ip & mask))
        {
          *src_ip = g_container_ip_group.ip_array[i].ip;
          NSSBR_LOGDBG ("find src ip]container_ip=0x%08x,dest_ip=0x%08x",
                        *src_ip, dst_ip);
          return 0;
        }
    }

  NSSBR_LOGDBG ("can't find src ip]dest_ip=0x%08x", dst_ip);
  return -1;
}
