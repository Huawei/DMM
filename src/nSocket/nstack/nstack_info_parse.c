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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "json.h"
#include "nsfw_base_linux_api.h"
#include "nstack_info_parse.h"

/*get string value*/
#define NSTACK_JSON_PARSE_STRING(obj, name, lent, reslt, index) do { \
     struct json_object* temp_obj1 = NULL; \
     (void)json_object_object_get_ex((obj), (name), &temp_obj1);  \
     if (temp_obj1)  \
     {  \
        const char *temp_value1 = json_object_get_string(temp_obj1); \
        if (!temp_value1) \
        { \
            NSSOC_LOGERR("can't get value from %s index:%d", name, (index));  \
            goto RETURN_ERROR; \
        }  \
        (void)STRNCPY_S((reslt), (lent), temp_value1, (lent));  \
     }  \
     else \
     {  \
        NSSOC_LOGERR("can't get obj from %s index:%d", name, (index));  \
        goto RETURN_ERROR; \
     }  \
} while ( 0 );

/*get int value*/
#define NSTACK_JSON_PARSE_INT(obj, name, lent, reslt, index) do { \
     struct json_object* temp_obj1 = NULL; \
     (void)json_object_object_get_ex((obj), (name), &temp_obj1);  \
     if (temp_obj1)  \
     {  \
        const char *temp_value1 = json_object_get_string(temp_obj1); \
        if (!temp_value1) \
        { \
            NSSOC_LOGERR("can't get value from %s index:%d", name, (index));  \
            goto RETURN_ERROR; \
        }  \
        (reslt) = atoi(temp_value1);  \
     }  \
     else \
     {  \
        NSSOC_LOGERR("can't get obj from %s index:%d", name, (index));  \
        goto RETURN_ERROR; \
     }  \
} while ( 0 );

/*parse module cfg*/
int
nstack_parse_module_cfg_json (char *param)
{
  struct json_object *obj = json_tokener_parse (param);
  struct json_object *module_list_obj = NULL;
  struct json_object *module_obj = NULL;
  struct json_object *temp_obj = NULL;
  const char *default_name = NULL;
  const char *temp_value = NULL;
  int module_num = 0;
  int ret = NSTACK_RETURN_FAIL;
  int index = 0;
  int icnt = 0;

  if (!obj)
    {
      NSSOC_LOGERR ("json parse fail");
      return NSTACK_RETURN_FAIL;
    }

  (void) MEMSET_S (&g_nstack_module_desc[0], sizeof (g_nstack_module_desc), 0,
                   sizeof (g_nstack_module_desc));

  (void) json_object_object_get_ex (obj, "default_stack_name", &temp_obj);
  if (!temp_obj)
    {
      NSSOC_LOGERR ("can't get module_list");
      goto RETURN_ERROR;
    }

  default_name = json_object_get_string (temp_obj);

  (void) json_object_object_get_ex (obj, "module_list", &module_list_obj);
  if (!module_list_obj)
    {
      NSSOC_LOGERR ("can't get module_list");
      goto RETURN_ERROR;
    }
  module_num = json_object_array_length (module_list_obj);
  if ((module_num <= 0) || (module_num >= NSTACK_MAX_MODULE_NUM))
    {
      NSSOC_LOGERR ("get module number:%d fail", module_num);
      goto RETURN_ERROR;
    }

  for (index = 0; index < module_num; index++)
    {
      module_obj = json_object_array_get_idx (module_list_obj, index);
      if (module_obj)
        {
          NSTACK_JSON_PARSE_STRING (module_obj, "stack_name", MODULE_NAME_MAX,
                                    &(g_nstack_module_desc[icnt].modName[0]),
                                    index);
          NSTACK_JSON_PARSE_STRING (module_obj, "function_name",
                                    MODULE_NAME_MAX,
                                    &(g_nstack_module_desc
                                      [icnt].registe_fn_name[0]), index);
          NSTACK_JSON_PARSE_STRING (module_obj, "libname", MODULE_NAME_MAX,
                                    &(g_nstack_module_desc[icnt].libPath[0]),
                                    index);

          (void) json_object_object_get_ex (module_obj, "loadtype",
                                            &temp_obj);
          if (temp_obj)
            {
              temp_value = json_object_get_string (temp_obj);
              if (strcmp (temp_value, "static") == 0)
                {
                  g_nstack_module_desc[icnt].libtype = NSTACK_LIB_LOAD_STATIC;
                }
              else
                {
                  g_nstack_module_desc[icnt].libtype = NSTACK_LIB_LOAD_DYN;
                }
            }
          else
            {
              NSSOC_LOGERR ("can't get value from loadtype index:%d", index);
              goto RETURN_ERROR;
            }
          NSTACK_JSON_PARSE_INT (module_obj, "deploytype", MODULE_NAME_MAX,
                                 g_nstack_module_desc[icnt].deploytype,
                                 index);
          NSTACK_JSON_PARSE_INT (module_obj, "maxfd", MODULE_NAME_MAX,
                                 g_nstack_module_desc[icnt].maxfdid, index);
          NSTACK_JSON_PARSE_INT (module_obj, "minfd", MODULE_NAME_MAX,
                                 g_nstack_module_desc[icnt].minfdid, index);
          NSTACK_JSON_PARSE_INT (module_obj, "priorty", MODULE_NAME_MAX,
                                 g_nstack_module_desc[icnt].priority, index);
          NSTACK_JSON_PARSE_INT (module_obj, "stackid", MODULE_NAME_MAX,
                                 g_nstack_module_desc[icnt].modInx, index);
          if (0 == strcmp (g_nstack_module_desc[icnt].modName, default_name))
            {
              g_nstack_module_desc[icnt].default_stack = 1;
            }
          icnt++;
          g_mudle_num = icnt;
        }
    }
  ret = NSTACK_RETURN_OK;

RETURN_ERROR:
  json_object_put (obj);
  return ret;
}

/*parse module cfg*/
int
nstack_parse_rd_cfg_json (char *param, rd_route_data ** data, int *num)
{
  struct json_object *obj = json_tokener_parse (param);
  struct json_object *ip_list_obj = NULL;
  struct json_object *proto_list_obj = NULL;
  struct json_object *module_obj = NULL;
  struct json_object *temp_obj = NULL;
  rd_route_data *rdtemp = NULL;
  rd_route_data *rddata = NULL;
  const char *sub = NULL;
  const char *temp_value = NULL;
  int ip_list_num = 0;
  int proto_list_num = 0;
  int totalnum = 0;
  int tlen = 0;
  int index = 0;
  int icnt = 0;
  char ipadd[32] = { 0 };

  if (!obj)
    {
      NSSOC_LOGERR ("json parse fail");
      return NSTACK_RETURN_FAIL;
    }

  (void) json_object_object_get_ex (obj, "ip_route", &ip_list_obj);
  if (!ip_list_obj)
    {
      NSSOC_LOGERR ("can't get module_list");
      goto RETURN_ERROR;
    }
  (void) json_object_object_get_ex (obj, "prot_route", &proto_list_obj);
  if (!proto_list_obj)
    {
      NSSOC_LOGERR ("can't get module_list");
      goto RETURN_ERROR;
    }

  ip_list_num = json_object_array_length (ip_list_obj);
  proto_list_num = json_object_array_length (proto_list_obj);
  totalnum = ip_list_num + proto_list_num;
  if (totalnum > NSTACK_RD_MAX)
    {
      NSSOC_LOGERR ("rd num is too more, and return fail");
      goto RETURN_ERROR;
    }
  tlen = sizeof (rd_route_data) * totalnum;
  rdtemp = (rd_route_data *) malloc (tlen);
  if (!rdtemp)
    {
      NSSOC_LOGERR ("malloc mem fail");
      goto RETURN_ERROR;
    }
  MEMSET_S (rdtemp, tlen, 0, tlen);
  for (index = 0; index < ip_list_num; index++)
    {
      module_obj = json_object_array_get_idx (ip_list_obj, index);
      if (module_obj)
        {
          rddata = rdtemp + icnt;
          rddata->type = RD_DATA_TYPE_IP;
          (void) json_object_object_get_ex (module_obj, "type", &temp_obj);
          if (temp_obj)
            {
              temp_value = json_object_get_string (temp_obj);
              if (!temp_value)
                {
                  NSSOC_LOGERR ("can't get value from subnet index:%d",
                                index);
                  goto RETURN_ERROR;
                }
              (void) STRNCPY_S (rddata->stack_name, RD_PLANE_NAMELEN,
                                temp_value, RD_PLANE_NAMELEN);
            }
          (void) json_object_object_get_ex (module_obj, "subnet", &temp_obj);
          if (temp_obj)
            {
              temp_value = json_object_get_string (temp_obj);
              if (!temp_value)
                {
                  NSSOC_LOGERR ("can't get value from subnet index:%d",
                                index);
                  goto RETURN_ERROR;
                }
              sub = strstr (temp_value, "/");
              if (!sub)
                {
                  NSSOC_LOGERR ("can't get maskklen from %s", temp_value);
                  goto RETURN_ERROR;
                }
              (void) MEMSET_S (ipadd, sizeof (ipadd), 0, sizeof (ipadd));
              (void) STRNCPY_S (ipadd, sizeof (ipadd), temp_value,
                                (size_t) (sub - temp_value));
              rddata->ipdata.masklen = atoi (sub + 1);
              rddata->ipdata.addr = ntohl (inet_addr (ipadd));
              icnt++;
            }
        }
    }

  for (index = 0; index < ip_list_num; index++)
    {
      module_obj = json_object_array_get_idx (proto_list_obj, index);
      if (module_obj)
        {
          rddata = rdtemp + icnt;
          rddata->type = RD_DATA_TYPE_PROTO;
          (void) json_object_object_get_ex (module_obj, "type", &temp_obj);
          if (temp_obj)
            {
              temp_value = json_object_get_string (temp_obj);
              if (!temp_value)
                {
                  NSSOC_LOGERR ("can't get value from subnet index:%d",
                                index);
                  goto RETURN_ERROR;
                }
              (void) STRNCPY_S (rddata->stack_name, RD_PLANE_NAMELEN,
                                temp_value, RD_PLANE_NAMELEN);
            }
          (void) json_object_object_get_ex (module_obj, "proto_type",
                                            &temp_obj);
          if (temp_obj)
            {
              temp_value = json_object_get_string (temp_obj);
              if (!temp_value)
                {
                  NSSOC_LOGERR ("can't get value from subnet index:%d",
                                index);
                  goto RETURN_ERROR;
                }
              rddata->proto_type = atoi (temp_value);
              icnt++;
            }
        }
    }
  *data = rdtemp;
  *num = icnt;
  json_object_put (obj);
  return NSTACK_RETURN_OK;
RETURN_ERROR:
  json_object_put (obj);
  if (rdtemp)
    {
      free (rdtemp);
    }
  return -1;
}

/*read json file, and return a buf, if return success, the caller need to free **buf*/
int
nstack_json_file_read (char *filename, char **buf)
{
  char *cfg_buf = NULL;
  int fp = 0;
  off_t file_len = 0;
  off_t buff_len = 0;
  int ret = NSTACK_RETURN_FAIL;

  if ((!filename) || (!buf))
    {
      return NSTACK_RETURN_FAIL;
    }

  fp = open (filename, O_RDONLY);
  if (fp < 0)
    {
      NSSOC_LOGERR ("open %s fail, error:%d!", filename, errno);
      ret = NSTACK_RETURN_FAIL;
      goto RETURN_RELEASE;
    }

  file_len = lseek (fp, 0, SEEK_END);
  if (file_len <= 0)
    {
      NSSOC_LOGERR ("failed to get file len]file name=%s", filename);
      ret = NSTACK_RETURN_FAIL;
      goto RETURN_RELEASE;
    }

  if (file_len > NSTACK_CFG_FILELEN_MAX)
    {
      NSSOC_LOGERR
        ("file len is too big]file len=%d, max len=%d, file name=%s",
         file_len, NSTACK_CFG_FILELEN_MAX, filename);
      ret = NSTACK_RETURN_FAIL;
      goto RETURN_RELEASE;
    }

  ret = lseek (fp, 0, SEEK_SET);
  if (ret < 0)
    {
      NSSOC_LOGERR ("seek to start failed]file name=%s", filename);
      ret = NSTACK_RETURN_FAIL;
      goto RETURN_RELEASE;
    }

  buff_len = file_len + 1;
  cfg_buf = (char *) malloc (buff_len);
  if (!cfg_buf)
    {
      NSSOC_LOGERR ("malloc buff failed]buff_len=%d", buff_len);
      ret = NSTACK_RETURN_FAIL;
      goto RETURN_RELEASE;
    }

  ret = MEMSET_S (cfg_buf, buff_len, 0, buff_len);
  if (NSTACK_RETURN_OK != ret)
    {
      NSSOC_LOGERR ("MEMSET_S failed]ret=%d.", ret);
      ret = NSTACK_RETURN_FAIL;
      goto RETURN_RELEASE;
    }

  ret = nsfw_base_read (fp, cfg_buf, buff_len - 1);
  if (ret <= 0)
    {
      NSSOC_LOGERR ("read failed]ret=%d, errno:%d", ret, errno);
      ret = NSTACK_RETURN_FAIL;
      goto RETURN_RELEASE;
    }

  *buf = cfg_buf;
  nsfw_base_close (fp);
  return NSTACK_RETURN_OK;
RETURN_RELEASE:
  if (fp >= 0)
    {
      nsfw_base_close (fp);
    }
  if (cfg_buf)
    {
      free (cfg_buf);
    }
  return ret;
}

/*parse module cfg file*/
int
nstack_module_parse ()
{
  char *modulecfg = NULL;
  char *tmp_config_path = NULL;
  char *cfg_buf = NULL;
  int ret = NSTACK_RETURN_FAIL;

  modulecfg = getenv (NSTACK_MOD_CFG_FILE);

  if (modulecfg)
    {
      tmp_config_path = realpath (modulecfg, NULL);
    }
  else
    {
      tmp_config_path = realpath (DEFALT_MODULE_CFG_FILE, NULL);
    }

  if (!tmp_config_path)
    {
      NSSOC_LOGERR ("nstack module file:%s get real path fail!",
                    modulecfg ? modulecfg : DEFALT_MODULE_CFG_FILE);
      return NSTACK_RETURN_FAIL;
    }

  ret = nstack_json_file_read (tmp_config_path, &cfg_buf);
  if (NSTACK_RETURN_OK == ret)
    {
      ret = nstack_parse_module_cfg_json (cfg_buf);
      free (cfg_buf);
    }

  free (tmp_config_path);
  return ret;
}

int
nstack_stack_rd_parse (rd_route_data ** data, int *num)
{
  char *modulecfg = NULL;
  char *tmp_config_path = NULL;
  char *cfg_buf = NULL;
  int ret = NSTACK_RETURN_FAIL;

  modulecfg = getenv (NSTACK_MOD_CFG_RD);

  if (modulecfg)
    {
      tmp_config_path = realpath (modulecfg, NULL);
    }
  else
    {
      tmp_config_path = realpath (DEFALT_RD_CFG_FILE, NULL);
    }

  if (!tmp_config_path)
    {
      NSSOC_LOGERR ("nstack rd file:%s get real path fail!",
                    modulecfg ? modulecfg : DEFALT_MODULE_CFG_FILE);
      return NSTACK_RETURN_FAIL;
    }

  ret = nstack_json_file_read (tmp_config_path, &cfg_buf);
  if (NSTACK_RETURN_OK == ret)
    {
      ret = nstack_parse_rd_cfg_json (cfg_buf, data, num);
      free (cfg_buf);
    }

  free (tmp_config_path);
  return ret;
}
