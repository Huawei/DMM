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

#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "configuration_reader.h"
#include "container_ip.h"
#include "network.h"
#include "nstack_log.h"
#include "nstack_securec.h"
#include "json.h"
#include "spl_tcpip.h"

#include <types.h>
#include <nsfw_mgr_com_api.h>
#include <nsfw_base_linux_api.h>
#include "nsfw_maintain_api.h"

NSTACK_STATIC struct config_data g_ip_module_buff;
NSTACK_STATIC struct config_data *g_config_data;
NSTACK_STATIC char ip_module_unix_socket[IP_MODULE_MAX_PATH_LEN + 1];
NSTACK_STATIC char ip_module_unix_socket_dir_path[IP_MODULE_MAX_PATH_LEN + 1];
//static unsigned long int g_thread_id = 0;

#define MAX_CONNECTION_NUMBER 5
#define TCP_OOS_LEN_MAX 250

NSTACK_STATIC int read_configuration ();
NSTACK_STATIC int unix_socket_listen (const char *servername);
NSTACK_STATIC int process_query ();

/*****************************************************************************
*   Prototype    : is_digit_str
*   Description  : check if a string contains only digit
*   Input        : char *input
*   Output       : 1 for yes, 0 for no
*   Return Value : int
*   Calls        :
*   Called By    : get_main_pid
*
*****************************************************************************/
NSTACK_STATIC int
is_digit_str (char *input)
{
  if (NULL == input || '\0' == input[0])
    {
      return 0;
    }

  while (*input)
    {
      if (*input > '9' || *input < '0')
        {
          return 0;
        }
      input++;
    }

  return 1;
}

/*****************************************************************************
*   Prototype    : process_query
*   Description  : ./nStackCtrl -a query
*   Input        : none
*   Output       : none
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
NSTACK_STATIC int
process_query ()
{
  int retval;

  if (0 == g_config_data->param.type[0])
    {
      return process_post (NULL, IP_MODULE_ALL, IP_MODULE_OPERATE_QUERY);
    }

  /*name & p are freed inside process_post */
  if (0 == strcmp (g_config_data->param.type, IP_MODULE_TYPE_PORT))
    {
      struct ip_action_param *p = malloc (sizeof (struct ip_action_param));
      if (p == NULL)
        {
          NSOPR_LOGERR ("name allocation failed!");
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "mem alloc error!");
          return -1;
        }

      retval =
        MEMSET_S (p, sizeof (struct ip_action_param), 0,
                  sizeof (struct ip_action_param));
      if (EOK != retval)
        {
          NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retval);
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "MEMSET_S error!");
          free (p);
          return -1;
        }

      retval =
        STRCPY_S (p->container_id, sizeof (p->container_id),
                  g_config_data->param.container_id);
      if (EOK != retval)
        {
          NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "STRCPY_S error!");
          free (p);
          return -1;
        }

      retval =
        STRCPY_S (p->port_name, sizeof (p->port_name),
                  g_config_data->param.name);
      if (EOK != retval)
        {
          NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "STRCPY_S error!");
          free (p);
          return -1;
        }

      return process_post ((void *) p, IP_MODULE_IP, IP_MODULE_OPERATE_QUERY);
    }
  else if (0 == strcmp (g_config_data->param.type, IP_MODULE_TYPE_NETWORK))
    {
      if (0 == g_config_data->param.name[0])
        {
          return process_post (NULL, IP_MODULE_NETWORK_ALL,
                               IP_MODULE_OPERATE_QUERY);
        }
      else
        {
          char *name = malloc (sizeof (g_config_data->param.name));
          if (NULL == name)
            {
              NSOPR_LOGERR ("name allocation failed!");
              NSOPR_SET_ERRINFO (NSCRTL_ERR, "mem alloc error!");
              return -1;
            }

          retval =
            MEMSET_S (name, sizeof (g_config_data->param.name), 0,
                      sizeof (g_config_data->param.name));
          if (EOK != retval)
            {
              NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retval);
              NSOPR_SET_ERRINFO (NSCRTL_ERR, "MEMSET_S error!");
              free (name);
              return -1;
            }

          retval =
            STRCPY_S (name, sizeof (g_config_data->param.name),
                      g_config_data->param.name);
          if (EOK != retval)
            {
              NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
              NSOPR_SET_ERRINFO (NSCRTL_ERR, "STRCPY_S error!");
              free (name);
              return -1;
            }

          return process_post ((void *) name, IP_MODULE_NETWORK,
                               IP_MODULE_OPERATE_QUERY);
        }
    }
  else if (0 == strcmp (g_config_data->param.type, IP_MODULE_TYPE_IP))
    {
      if (0 == g_config_data->param.name[0])
        {
          return process_post (NULL, IP_MODULE_IP_ALL,
                               IP_MODULE_OPERATE_QUERY);
        }
      else
        {
          NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "input error!");
        }
    }
  else
    {
      NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "input error!");
    }

  return -1;
}

int
read_ipmoduleoperatesetnet_configuration ()
{
  if (strcmp (g_config_data->param.type, IP_MODULE_TYPE_SETLOG) == 0)
    {
      if (NSCRTL_OK ==
          setlog_level_value (g_config_data->param.name,
                              g_config_data->param.value))
        {
          NSOPR_LOGDBG ("set log level ok!");
        }
      else
        {
          NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "input error!");
        }
    }
  else if (strcmp (g_config_data->param.type, TCP_MODULE_TYPE_SET_OOS_LEN) ==
           0)
    {
      if (is_digit_str (g_config_data->param.value) == 0)
        {
          NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR,
                             "Invalid value:value must be digital and smaller than %u]value=\"%s\"",
                             TCP_OOS_LEN_MAX, g_config_data->param.value);
          return 0;
        }

      unsigned int value_len = strlen (g_config_data->param.value);
      if ((value_len >= 2) && (g_config_data->param.value[0] == '0'))
        {
          NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR,
                             "Invalid value:value cannot start with 0");
          return 0;
        }
    }
  else
    {
      NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "input error!");
    }

  return 0;
}

/*****************************************************************************
*   Prototype    : read_version
*   Description  : Query Version by nStackCtrl
*   Input        : None
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
read_version ()
{
  int retVal;
  json_object *version = json_object_new_object ();

  if (NULL == version)
    {
      NSOPR_SET_ERRINFO (NSCRTL_ERR, "internal error for version=NULL!");
      return NSCRTL_ERR;
    }

  json_object_object_add (version, "moudle",
                          json_object_new_string (NSTACK_GETVER_MODULE));
  json_object_object_add (version, "version",
                          json_object_new_string (NSTACK_GETVER_VERSION));
  json_object_object_add (version, "buildtime",
                          json_object_new_string (NSTACK_GETVER_BUILDTIME));

  json_object *version_array = json_object_new_array ();
  if (NULL == version_array)
    {
      json_object_put (version);
      NSOPR_SET_ERRINFO (NSCRTL_ERR,
                         "internal error for version_array=NULL!");
      return NSCRTL_ERR;
    }

  retVal = json_object_array_add (version_array, version);

  if (0 != retVal)
    {
      json_object_put (version_array);
      json_object_put (version);
      NSOPR_SET_ERRINFO (NSCRTL_ERR,
                         "internal error for json_object_array_add failed!");
      return NSCRTL_ERR;
    }

  const char *str = json_object_to_json_string (version_array);

  if (NULL == str)
    {
      json_object_put (version_array);
      NSOPR_SET_ERRINFO (NSCRTL_ERR, "internal error for str=NULL!");
      return NSCRTL_ERR;
    }

  size_t str_len = strlen (str);
  if (str_len >= sizeof (get_config_data ()->json_buff))
    {
      json_object_put (version_array);
      NSOPR_SET_ERRINFO (NSCRTL_ERR, "internal error!");
      return NSCRTL_ERR;
    }

  retVal =
    STRNCPY_S (get_config_data ()->json_buff,
               sizeof (get_config_data ()->json_buff), str, str_len);
  if (EOK != retVal)
    {
      json_object_put (version_array);
      NSOPR_SET_ERRINFO (NSCRTL_ERR, "STRNCPY_S error!");
      return NSCRTL_ERR;
    }

  json_object_put (version_array);
  return NSCRTL_OK;
}

void
reset_config_data (void)
{
  int retval = MEMSET_S (g_config_data, sizeof (struct config_data), 0,
                         sizeof (struct config_data));
  if (EOK != retval)
    {
      printf ("MEMSET_S failed]retval=%d.\n", retval);
      exit (1);
    }
}

int
get_network_json_data ()
{
  STRCPY_S (g_config_data->param.type, sizeof (g_config_data->param.type),
            "network");
  g_config_data->param.action = IP_MODULE_OPERATE_ADD;

  char *tmp_config_path;
  tmp_config_path = realpath ("./network_data_tonStack.json", NULL);
  if (!tmp_config_path)
    {
      exit (1);
    }

  int fp = open (tmp_config_path, O_RDONLY);
  if (-1 == fp)
    {
      free (tmp_config_path);
      NSTCP_LOGINF ("network file open failed.\n");
      exit (1);
    }
  free (tmp_config_path);

  int nread = read (fp, g_config_data->json_buff,
                    sizeof (g_config_data->json_buff) - 1);
  if (nread <= 0)
    {
      close (fp);
      NSTCP_LOGINF ("read failed %d.\n", nread);
      exit (1);
    }

  /* though MEMSET_S is done above, MEMSET_S can be removed */
  g_config_data->json_buff[nread] = '\0';
  close (fp);

  struct network_configuration *network = NULL;
  struct network_configuration *tmp = NULL;

  /* input shouldnot be same with return */
  network = parse_network_json (g_config_data->json_buff, NULL);
  if (!network)
    {
      NSTCP_LOGINF ("Invalid network data!");
      NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "Invalid network data!");
      return -1;
    }

  /* run process_post for each network, not only the head node */
  while (network)
    {
      tmp = network;
      network = network->next;
      int retval =
        add_network_configuration ((struct network_configuration *) tmp);

      /* When network exceeds max number, just log warning at operation.log */
      if (retval == NSCRTL_NETWORK_COUNT_EXCEED)
        {
          NSTCP_LOGINF
            ("Warning!! Network count exceed max allowed number]max=%d",
             MAX_NETWORK_COUNT);
        }
      else
        {

          NSTCP_LOGINF ("add_network_configuration %d", retval);
          NSOPR_SET_ERRINFO (retval, "add_network_configuration return %d",
                             retval);
        }

      if (!retval)
        {
          /*init DPDK eth */
          if ((retval = init_new_network_configuration ()) != ERR_OK)
            {
              NSTCP_LOGINF ("process_configuration failed! %d", retval);
              free_network_configuration ((struct network_configuration *)
                                          tmp, IP_MODULE_TRUE);
              NSOPR_SET_ERRINFO (retval,
                                 "init_new_network_configuration return %d",
                                 retval);
              return -1;
            }
        }
    }
  NSTCP_LOGINF ("Get_network_json_data done!");

  return 0;
}

int
get_ip_json_data ()
{
  NSTCP_LOGINF ("get_ip_json_data start!");

  STRCPY_S (g_config_data->param.type, sizeof (g_config_data->param.type),
            "ip");
  g_config_data->param.action = IP_MODULE_OPERATE_ADD;

  char *tmp_config_path;
  tmp_config_path = realpath ("./ip_data.json", NULL);
  if (!tmp_config_path)
    {
      exit (1);
    }

  int fp = open (tmp_config_path, O_RDONLY);
  if (-1 == fp)
    {
      free (tmp_config_path);
      NSTCP_LOGINF ("network file open failed\n");
      exit (1);
    }
  free (tmp_config_path);

  int nread = read (fp, g_config_data->json_buff,
                    sizeof (g_config_data->json_buff) - 1);
  if (nread <= 0)
    {
      close (fp);
      NSTCP_LOGINF ("read failed %d.\n", nread);
      exit (1);
    }

  /* though MEMSET_S is done above, MEMSET_S can be removed */
  g_config_data->json_buff[nread] = '\0';
  close (fp);

  struct container_ip *container =
    parse_container_ip_json (g_config_data->json_buff);
  if (container)
    {
      int retval = add_container (container);

      NSTCP_LOGINF ("add_container %d", retval);
      NSOPR_SET_ERRINFO (retval, "add_container return %d", retval);
    }
  else
    {
      NSTCP_LOGINF ("Invalid IP config data!");
      NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "Invalid IP config data!");
      return -1;
    }
  NSTCP_LOGINF ("get_ip_json_data done!");

  return 0;
}

int
read_ipmoduleoperateadd_configuration ()
{
  struct network_configuration *tmp = NULL;
  if (strcmp (g_config_data->param.type, IP_MODULE_TYPE_IP) == 0)
    {
      struct container_ip *container =
        parse_container_ip_json (g_config_data->json_buff);
      if (container)
        {
          return process_post ((void *) container, IP_MODULE_IP,
                               IP_MODULE_OPERATE_ADD);
        }
      else
        {
          NSOPR_LOGERR ("Invalid IP config data!");
          NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "Invalid IP config data!");
          return -1;
        }
    }
  else if (strcmp (g_config_data->param.type, IP_MODULE_TYPE_NETWORK) == 0)
    {
      struct network_configuration *network = NULL;

      //Read network.json

      /* input shouldnot be same with return */
      network = parse_network_json (g_config_data->json_buff, NULL);
      if (!network)
        {
          NSOPR_LOGERR ("Invalid network data!");
          NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "Invalid network data!");
          return -1;
        }

      /* run process_post for each network, not only the head node */
      while (network)
        {
          tmp = network;
          network = network->next;
          int ret = process_post ((void *) tmp, IP_MODULE_NETWORK,
                                  IP_MODULE_OPERATE_ADD);
          if (ret == -1)
            {
              NSOPR_LOGERR ("process_configuration failed!");
              return -1;
            }
        }
      return 0;
    }
  else
    {
      NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "input error!");
      return -1;
    }
}

int
read_ipmoduleoperatedel_configuration ()
{
  int retval;

  if (strcmp (g_config_data->param.type, IP_MODULE_TYPE_IP) == 0)
    {
      struct ip_action_param *p = malloc (sizeof (struct ip_action_param));
      if (NULL == p)
        {
          NSOPR_LOGERR ("ip_action_param allocation failed!");
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "mem alloc error!");
          return -1;
        }

      retval =
        MEMSET_S (p, sizeof (struct ip_action_param), 0,
                  sizeof (struct ip_action_param));
      if (EOK != retval)
        {
          NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retval);
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "MEMSET_S error!");
          free (p);
          return -1;
        }

      retval =
        STRCPY_S (p->container_id, sizeof (p->container_id),
                  g_config_data->param.container_id);
      if (EOK != retval)
        {
          NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "STRCPY_S error!");
          free (p);
          return -1;
        }

      retval =
        STRCPY_S (p->port_name, sizeof (p->port_name),
                  g_config_data->param.name);
      if (EOK != retval)
        {
          NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "STRCPY_S error!");
          free (p);
          return -1;
        }

      return process_post ((void *) p, IP_MODULE_IP, IP_MODULE_OPERATE_DEL);
    }
  else if (strcmp (g_config_data->param.type, IP_MODULE_TYPE_NETWORK) == 0)
    {
      char *name = malloc (sizeof (g_config_data->param.name));
      if (name == NULL)
        {
          NSOPR_LOGERR ("name allocation failed!");
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "mem alloc error!");
          return -1;
        }

      retval =
        MEMSET_S (name, sizeof (g_config_data->param.name), 0,
                  sizeof (g_config_data->param.name));
      if (EOK != retval)
        {
          NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retval);
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "MEMSET_S error!");
          free (name);
          return -1;
        }

      retval =
        STRCPY_S (name, sizeof (g_config_data->param.name),
                  g_config_data->param.name);
      if (EOK != retval)
        {
          NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
          NSOPR_SET_ERRINFO (NSCRTL_ERR, "STRCPY_S error!");
          free (name);
          return -1;
        }

      return process_post ((void *) name, IP_MODULE_NETWORK,
                           IP_MODULE_OPERATE_DEL);
    }
  else
    {
      NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "input error!");
      return -1;
    }
}

NSTACK_STATIC int
read_configuration ()
{
  int retval = -1;
  //u64 traceid = 0;

  /* initialise default memory */
  g_config_data->param.error = NSCRTL_OK;

  /* Make sure error_desc is inited to null string */
  g_config_data->param.error_desc[0] = '\0';

  //traceid = g_config_data->param.traceid;

  NSOPR_LOGINF
    ("g_config_data]type=%s,name=%s,value=%s,container_id=%s,action=%d,Json_buf=%s, traceid=%llu",
     g_config_data->param.type, g_config_data->param.name,
     g_config_data->param.value, g_config_data->param.container_id,
     g_config_data->param.action, g_config_data->json_buff,
     g_config_data->param.traceid);

  retval =
    MEMSET_S (g_config_data->param.error_desc,
              sizeof (g_config_data->param.error_desc), 0,
              sizeof (g_config_data->param.error_desc));
  if (0 != retval)
    {
      NSOPR_SET_ERRINFO (NSCRTL_ERR, "ERR:internal error, MEMSET_S failed]");
      return -1;
    }

  switch (g_config_data->param.action)
    {
    case IP_MODULE_OPERATE_DEL:
      {
        retval = read_ipmoduleoperatedel_configuration ();
        break;
      }
    case IP_MODULE_OPERATE_QUERY:
      {
        retval = process_query ();
        break;
      }
    case IP_MODULE_OPERATE_ADD:
      {
        retval = read_ipmoduleoperateadd_configuration ();
        break;
      }
    case IP_MODULE_OPERATE_SET:
      retval = read_ipmoduleoperatesetnet_configuration ();
      break;
    case IP_MODULE_GET_VERSION:
      {
        retval = read_version ();
        break;
      }

    default:
      {
        retval = -1;            //here, must set retval to -1
        NSOPR_SET_ERRINFO (NSCRTL_INPUT_ERR, "input error!");
        break;
      }
    }

  return retval;
}

NSTACK_STATIC int
unix_socket_listen (const char *servername)
{
  int fd, retval;
  unsigned int len;
  struct stat st;
  struct sockaddr_un un;

  if (stat (ip_module_unix_socket_dir_path, &st) == 0)
    {
      NSOPR_LOGDBG (" /directory is present");
    }
  else
    {
      NSOPR_LOGERR (" /var/run/nStack/ directory is not present ");
      return (-1);
    }

  if ((fd = nsfw_base_socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
      return -1;
    }

  retval = unlink (servername); /* in case it already exists */
  if (0 != retval)
    {
      NSOPR_LOGWAR ("unlink failed]retval=%d,errno=%d", retval, errno);
    }

  retval = MEMSET_S (&un, sizeof (un), 0, sizeof (un));
  if (EOK != retval)
    {
      (void) nsfw_base_close (fd);
      NSOPR_LOGERR ("MEMSET_S failed]ret=%d", retval);
      return -1;
    }

  un.sun_family = AF_UNIX;
  retval = STRCPY_S (un.sun_path, sizeof (un.sun_path), servername);
  if (EOK != retval)
    {
      (void) nsfw_base_close (fd);
      NSOPR_LOGERR ("STRCPY_S failed]ret=%d", retval);
      return -1;
    }

  len =
    (unsigned int) (offsetof (struct sockaddr_un, sun_path) +
                    strlen (servername));

  if (nsfw_base_bind (fd, (struct sockaddr *) &un, len) < 0)
    {
      (void) nsfw_base_close (fd);
      return -1;
    }
  else
    {
      if (nsfw_base_listen (fd, MAX_CONNECTION_NUMBER) < 0)
        {
          (void) nsfw_base_close (fd);
          return -1;
        }
      else
        {
          return fd;
        }
    }
}

/*****************************************************************************
*   Prototype    : read_fn
*   Description  : process new ip module msg
*   Input        : i32 fd
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
void
read_fn (i32 fd)
{
  ssize_t size;
  ssize_t offset = 0;
  size_t left = MAX_IP_MODULE_BUFF_SIZE;
  while (left > 0)
    {
      size = nsfw_base_recv (fd, (char *) g_config_data + offset, left, 0);
      if (size > 0)
        {
          offset += size;
          left -= (size_t) size;
        }
      else
        {
          NSOPR_LOGERR ("Error when recieving]errno=%d,err_string=%s", errno,
                        strerror (errno));
          break;
        }
    }

  if (left != 0)
    {
      (void) nsfw_base_close (fd);
      return;
    }

  const char *old_hbt_cnt = "6";
  const char *new_hbt_cnt = "60";
  nsfw_set_soft_para (NSFW_PROC_MASTER, NSFW_HBT_COUNT_PARAM,
                      (void *) new_hbt_cnt, sizeof (u16));
  (void) read_configuration (); // if it returns -1, the err desc info will be wrote to g_config_data, so no need to check return value.
  nsfw_set_soft_para (NSFW_PROC_MASTER, NSFW_HBT_COUNT_PARAM,
                      (void *) old_hbt_cnt, sizeof (u16));

  offset = 0;
  left = MAX_IP_MODULE_BUFF_SIZE;
  while (left > 0)
    {
      size =
        nsfw_base_send (fd, (char *) g_config_data + offset, left,
                        MSG_NOSIGNAL);

      if (size > 0)
        {
          offset += size;
          left -= (size_t) size;
        }
      else
        {
          NSOPR_LOGERR ("Error when Sending data]errno=%d", errno);
          break;
        }
    }

  (void) nsfw_base_close (fd);
  return;
}

/*****************************************************************************
*   Prototype    : ip_module_new_msg
*   Description  : recv new config message
*   Input        : i32 epfd
*                  i32 fd
*                  u32 events
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
ip_module_new_msg (i32 epfd, i32 fd, u32 events)
{
  if ((events & EPOLLERR) || (events & EPOLLHUP) || (!(events & EPOLLIN)))
    {
      nsfw_mgr_unreg_sock_fun (fd);
      (void) nsfw_base_close (fd);
      return TRUE;
    }

  nsfw_mgr_unreg_sock_fun (fd);
  read_fn (fd);
  return TRUE;
}

int
init_ip_module_unix_socket_path ()
{
  const char *directory = "/var/log/nStack";
  const char *home_dir = getenv ("HOME");

  if (getuid () != 0 && home_dir != NULL)
    directory = home_dir;

  if (STRCPY_S
      (ip_module_unix_socket_dir_path, IP_MODULE_MAX_PATH_LEN, directory) < 0)
    {
      NSOPR_LOGERR ("STRCPY_S fail]");
      return -1;
    }

  if (STRCAT_S
      (ip_module_unix_socket_dir_path, IP_MODULE_MAX_PATH_LEN,
       "/ip_module") < 0)
    {
      NSOPR_LOGERR ("STRCAT_S fail]");
      return -1;
    }

  if (STRCPY_S
      (ip_module_unix_socket, IP_MODULE_MAX_PATH_LEN,
       ip_module_unix_socket_dir_path) < 0)
    {
      NSOPR_LOGERR ("STRCPY_S fail]");
      return -1;
    }

  if (STRCAT_S
      (ip_module_unix_socket, IP_MODULE_MAX_PATH_LEN,
       "/ip_module_unix_sock") < 0)
    {
      NSOPR_LOGERR ("STRCAT_S fail]");
      return -1;
    }

  NSOPR_LOGINF ("ip_module_unix_socket=%s", ip_module_unix_socket);
  NSOPR_LOGINF ("ip_module_unix_socket_dir_path=%s",
                ip_module_unix_socket_dir_path);
  return 0;
}

/*****************************************************************************
*   Prototype    : ip_module_new_connection
*   Description  : recv new connect for network config
*   Input        : i32 epfd
*                  i32 fd
*                  u32 events
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int
ip_module_new_connection (i32 epfd, i32 fd, u32 events)
{
  if ((events & EPOLLERR) || (events & EPOLLHUP) || (!(events & EPOLLIN)))
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGINF ("listen disconnect!]epfd=%d,listen=%d,event=0x%x", epfd,
                   fd, events);
      nsfw_mgr_unreg_sock_fun (fd);

      if (init_ip_module_unix_socket_path () < 0)
        {
          NSFW_LOGERR ("Error when init path]epfd=%d,listen_fd=%d,event=0x%x",
                       epfd, fd, events);
          return FALSE;
        }

      i32 listen_fd = unix_socket_listen (ip_module_unix_socket);
      if (listen_fd < 0)
        {
          NSFW_LOGERR ("get listen_fd faied!]epfd=%d,listen_fd=%d,event=0x%x",
                       epfd, fd, events);
          return FALSE;
        }

      if (FALSE ==
          nsfw_mgr_reg_sock_fun (listen_fd, ip_module_new_connection))
        {
          (void) nsfw_base_close (listen_fd);
          return FALSE;
        }
      return TRUE;
    }

  struct sockaddr in_addr;
  socklen_t in_len;
  int infd;
  in_len = sizeof in_addr;

  while (1)
    {
      infd = nsfw_base_accept (fd, &in_addr, &in_len);
      if (infd == -1)
        {
          break;
        }

      if (FALSE == nsfw_mgr_reg_sock_fun (infd, ip_module_new_msg))
        {
          NSFW_LOGINF ("accept new fd but reg failed]new_mgr_fd=%d", infd);
          return FALSE;
        }
      NSFW_LOGINF ("accept new fd]new_mgr_fd=%d", infd);
    }

  return TRUE;
}

int
init_configuration_reader ()
{
  int error_number = 0;
  INITPOL_LOGINF ("RTP", "init_configuration_reader", NULL_STRING,
                  LOG_INVALID_VALUE, MODULE_INIT_START);
  g_config_data = &g_ip_module_buff;

  if (init_ip_module_unix_socket_path () < 0)
    {
      INITPOL_LOGERR ("RTP", "init_configuration_reader",
                      "Error when init path", LOG_INVALID_VALUE,
                      MODULE_INIT_FAIL);
      return -1;
    }

  i32 listen_fd = unix_socket_listen (ip_module_unix_socket);
  if (listen_fd < 0)
    {
      error_number = errno;
      INITPOL_LOGERR ("RTP", "init_configuration_reader",
                      "when listening ip_module_unix_socket", error_number,
                      MODULE_INIT_FAIL);
      return -1;
    }

  NSOPR_LOGINF ("start mgr_com module!]listern_fd=%d", listen_fd);

  if (FALSE == nsfw_mgr_reg_sock_fun (listen_fd, ip_module_new_connection))
    {
      (void) nsfw_base_close (listen_fd);
      NSOPR_LOGERR ("nsfw_mgr_reg_sock_fun failed]listen_fd=%d", listen_fd);
      return -1;
    }

  INITPOL_LOGINF ("RTP", "init_configuration_reader", NULL_STRING,
                  LOG_INVALID_VALUE, MODULE_INIT_SUCCESS);
  return 0;
}

struct config_data *
get_config_data ()
{
  return g_config_data;
}
