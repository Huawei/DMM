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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "nsfw_getopt.h"
#include "nstack_log.h"

char *nsfw_optarg = NULL;
int nsfw_optind = 1;
int nsfw_opterr = 1;
int nsfw_optopt = '?';
NSTACK_STATIC char *nsfw_optnext = NULL;
NSTACK_STATIC int posixly_correct = -1;
NSTACK_STATIC int handle_nonopt_argv = 0;
NSTACK_STATIC int start = 0;
NSTACK_STATIC int end = 0;

NSTACK_STATIC void check_gnu_extension (const char *optstring);
NSTACK_STATIC int nsfw_getopt_internal (int argc, char *const argv[],
                                        const char *optstring,
                                        const struct option *longopts,
                                        int *longindex, int long_only);
NSTACK_STATIC int nsfw_getopt_shortopts (int argc, char *const argv[],
                                         const char *optstring,
                                         int long_only);
NSTACK_STATIC int nsfw_getopt_longopts (int argc, char *const argv[],
                                        char *arg, const char *optstring,
                                        const struct option *longopts,
                                        int *longindex, int *long_only_flag);
NSTACK_STATIC inline int nsfw_getopt_internal_check_opts (const char
                                                          *optstring);
NSTACK_STATIC inline int nsfw_getopt_check_optind ();
NSTACK_STATIC inline int nsfw_getopt_internal_init (char *const argv[]);
NSTACK_STATIC inline int nsfw_getopt_longopts_check_longonly (int
                                                              *long_only_flag,
                                                              const char
                                                              *optstring,
                                                              char *const
                                                              argv[]);

NSTACK_STATIC void
check_gnu_extension (const char *optstring)
{
  if (optstring[0] == '+' || getenv ("POSIXLY_CORRECT") != NULL)
    {
      posixly_correct = 1;      /* assign 1 to posixly_correct   */
    }
  else
    {
      posixly_correct = 0;      /* assign 0 to posixly_correct   */
    }
  if (optstring[0] == '-')
    {
      handle_nonopt_argv = 1;   /* assign 1 to handle_nonopt_argv */
    }
  else
    {
      handle_nonopt_argv = 0;   /* assign 0 to handle_nonopt_argv */
    }
}

int
nsfw_getopt_long (int argc, char *const argv[], const char *optstring,
                  const struct option *longopts, int *longindex)
{
  return nsfw_getopt_internal (argc, argv, optstring, longopts, longindex, 0);
}

NSTACK_STATIC inline int
nsfw_getopt_internal_check_opts (const char *optstr)
{
  if (NULL == optstr)
    {
      return -1;                /* return -1 */
    }

  if (nsfw_optopt == '?')
    {
      nsfw_optopt = 0;
    }

  if (posixly_correct == -1)
    {
      check_gnu_extension (optstr);
    }

  if (nsfw_optind == 0)
    {
      check_gnu_extension (optstr);
      nsfw_optind = 1;
      nsfw_optnext = NULL;
    }

  switch (optstr[0])
    {
    case '-':
    case '+':
      optstr++;
      break;
    default:
      break;
    }
  return 0;
}

NSTACK_STATIC inline int
nsfw_getopt_check_optind ()
{
  if (nsfw_optind <= 0)
    nsfw_optind = 1;
  return 0;
}

NSTACK_STATIC inline int
nsfw_getopt_internal_init (char *const argv[])
{
  if (nsfw_optnext == NULL && start != 0)
    {
      int last_loc = nsfw_optind - 1;

      nsfw_optind -= end - start;
      (void) nsfw_getopt_check_optind ();

      while (start < end--)
        {
          int j;
          char *arg = argv[end];

          for (j = end; j < last_loc; j++)
            {
              int k = j + 1;
              ((char **) argv)[j] = argv[k];
            }
          ((char const **) argv)[j] = arg;
          last_loc--;
        }
      start = 0;                /*make start as zero */
    }
  return 0;
}

NSTACK_STATIC int
nsfw_getopt_internal (int argc, char *const argv[], const char *optstring,
                      const struct option *longopts, int *longindex,
                      int long_only)
{

  (void) nsfw_getopt_internal_check_opts (optstring);

  (void) nsfw_getopt_internal_init (argv);

  if (nsfw_optind >= argc)
    {
      nsfw_optarg = NULL;
      return -1;                /* return -1 */
    }
  if (nsfw_optnext == NULL)
    {
      const char *arg = argv[nsfw_optind];
      if (*arg != '-')
        {
          if (handle_nonopt_argv)
            {
              nsfw_optarg = argv[nsfw_optind++];
              start = 0;
              return 1;
            }
          else if (posixly_correct)
            {
              nsfw_optarg = NULL;
              return -1;
            }
          else
            {
              int k;

              start = nsfw_optind;
              for (k = nsfw_optind + 1; k < argc; k++)
                {
                  if (argv[k][0] == '-')
                    {
                      end = k;
                      break;
                    }
                }
              if (k == argc)
                {
                  nsfw_optarg = NULL;
                  return -1;
                }
              nsfw_optind = k;
              arg = argv[nsfw_optind];
            }
        }
      if (strcmp (arg, "--") == 0)
        {
          nsfw_optind++;
          return -1;
        }
      if (longopts != NULL && arg[1] == '-')
        {
          return nsfw_getopt_longopts (argc, argv, argv[nsfw_optind] + 2,
                                       optstring, longopts, longindex, NULL);
        }
    }

  if (nsfw_optnext == NULL)
    {
      nsfw_optnext = argv[nsfw_optind] + 1;
    }
  if (long_only)
    {
      int long_only_flag = 0;
      int rv =
        nsfw_getopt_longopts (argc, argv, nsfw_optnext, optstring, longopts,
                              longindex, &long_only_flag);
      if (!long_only_flag)
        {
          nsfw_optnext = NULL;
          return rv;
        }
    }

  return nsfw_getopt_shortopts (argc, argv, optstring, long_only);
}

NSTACK_STATIC int
nsfw_getopt_shortopts (int argc, char *const argv[], const char *optstring,
                       int long_only)
{
  int opt = *nsfw_optnext;
  const char *os;
  if (optstring != NULL)
    {
      os = strchr (optstring, opt);
    }
  else
    {
      /* here try to keep same with below behavior */
      return '?';
    }

  if (os == NULL)
    {
      nsfw_optarg = NULL;
      if (long_only)
        {
          if (':' != optstring[0])
            {
              NSFW_LOGERR ("unrecognized option] argv_0=%s, netopt=%s",
                           argv[0], nsfw_optnext);
            }
          nsfw_optind++;
          nsfw_optnext = NULL;
        }
      else
        {
          nsfw_optopt = opt;
          if (':' != optstring[0])
            {
              NSFW_LOGERR ("invalid option] argv_0=%s, opt=%c", argv[0], opt);
            }
          if (*(++nsfw_optnext) == 0)
            {
              nsfw_optind++;
              nsfw_optnext = NULL;
            }
        }
      return '?';
    }
  if (os[1] == ':')
    {
      if (nsfw_optnext[1] == 0)
        {
          nsfw_optind++;
          if (os[2] == ':')
            {
              nsfw_optarg = NULL;
            }
          else
            {
              if (nsfw_optind == argc)
                {
                  nsfw_optarg = NULL;
                  nsfw_optopt = opt;
                  if (':' != optstring[0])
                    {
                      NSFW_LOGERR
                        ("option requires an argument] argv_0=%s, opt=%c",
                         argv[0], opt);
                    }
                  return optstring[0] == ':' ? ':' : '?';
                }
              nsfw_optarg = argv[nsfw_optind];
              nsfw_optind++;
            }
        }
      else
        {
          nsfw_optarg = nsfw_optnext + 1;
          nsfw_optind++;
        }
      nsfw_optnext = NULL;
    }
  else
    {
      nsfw_optarg = NULL;
      if (nsfw_optnext[1] == 0)
        {
          nsfw_optnext = NULL;
          nsfw_optind++;
        }
      else
        {
          nsfw_optnext++;
        }
    }
  return opt;
}

NSTACK_STATIC inline int
nsfw_getopt_longopts_check_longonly (int *long_only_flag,
                                     const char *optstring,
                                     char *const argv[])
{
  if (long_only_flag)
    {
      *long_only_flag = 1;
    }
  else
    {
      if (':' != optstring[0])
        {
          NSFW_LOGERR ("unrecognized option] argv_0=%s, option=%s", argv[0],
                       argv[nsfw_optind]);
        }
      nsfw_optind++;
    }
  return 0;
}

NSTACK_STATIC int
nsfw_getopt_longopts (int argc, char *const argv[], char *arg,
                      const char *optstring, const struct option *longopts,
                      int *longindex, int *long_only_flag)
{
  char *value = NULL;
  const struct option *option;
  size_t namelength;
  int index;

  if ((longopts == NULL) || (arg == NULL))
    {
      return -1;
    }

  for (index = 0; longopts[index].name != NULL; index++)
    {
      option = &longopts[index];
      namelength = strlen (option->name);

      if (strncmp (arg, option->name, namelength) == 0)
        {
          switch (arg[namelength])
            {
            case '\0':
              switch (option->has_arg)
                {
                case nsfw_required_argument:
                  nsfw_optind++;

                  if (nsfw_optind == argc)
                    {
                      nsfw_optarg = NULL;
                      nsfw_optopt = option->val;
                      if (':' != optstring[0])
                        {
                          NSFW_LOGERR
                            ("requires an argument] argv_0=%s, opt name=%s",
                             argv[0], option->name);
                        }
                      return optstring[0] == ':' ? ':' : '?';
                    }

                  value = argv[nsfw_optind];
                  break;

                default:
                  break;
                }

              goto found;

            case '=':
              if (option->has_arg == nsfw_no_argument)
                {
                  const char *hyphens =
                    (argv[nsfw_optind][1] == '-') ? "--" : "-";
                  nsfw_optind++;
                  nsfw_optarg = NULL;
                  nsfw_optopt = option->val;
                  if (':' != optstring[0])
                    {
                      NSFW_LOGERR
                        ("doesn't allow an argument] argv_0=%s, hyphens=%s, opt name=%s",
                         argv[0], hyphens, option->name);
                    }
                  return '?';
                }

              value = arg + namelength + 1;
              goto found;

            default:
              break;
            }
        }
    }

  (void) nsfw_getopt_longopts_check_longonly (long_only_flag, optstring,
                                              argv);
  return '?';

found:
  nsfw_optarg = value;
  nsfw_optind++;

  if (option->flag)
    {
      *option->flag = option->val;
    }

  if (longindex)
    {
      *longindex = index;
    }

  return option->flag ? 0 : option->val;
}
