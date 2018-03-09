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
      posixly_correct = 1;
    }
  else
    {
      posixly_correct = 0;
    }
  if (optstring[0] == '-')
    {
      handle_nonopt_argv = 1;
    }
  else
    {
      handle_nonopt_argv = 0;
    }
}

int
nsfw_getopt_long (int argc, char *const argv[], const char *optstring,
                  const struct option *longopts, int *longindex)
{
  return nsfw_getopt_internal (argc, argv, optstring, longopts, longindex, 0);
}

NSTACK_STATIC inline int
nsfw_getopt_internal_check_opts (const char *optstring)
{
  if (NULL == optstring)
    {
      return -1;
    }

  if (nsfw_optopt == '?')
    {
      nsfw_optopt = 0;
    }

  if (posixly_correct == -1)
    {
      check_gnu_extension (optstring);
    }

  if (nsfw_optind == 0)
    {
      check_gnu_extension (optstring);
      nsfw_optind = 1;
      nsfw_optnext = NULL;
    }

  switch (optstring[0])
    {
    case '+':
    case '-':
      optstring++;
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
      int last_pos = nsfw_optind - 1;

      nsfw_optind -= end - start;
      (void) nsfw_getopt_check_optind ();

      while (start < end--)
        {
          int i;
          char *arg = argv[end];

          for (i = end; i < last_pos; i++)
            {
              int j = i + 1;
              ((char **) argv)[i] = argv[j];
            }
          ((char const **) argv)[i] = arg;
          last_pos--;
        }
      start = 0;
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
      return -1;
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
              int i;

              start = nsfw_optind;
              for (i = nsfw_optind + 1; i < argc; i++)
                {
                  if (argv[i][0] == '-')
                    {
                      end = i;
                      break;
                    }
                }
              if (i == argc)
                {
                  nsfw_optarg = NULL;
                  return -1;
                }
              nsfw_optind = i;
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
  char *val = NULL;
  const struct option *opt;
  size_t namelen;
  int idx;

  if ((longopts == NULL) || (arg == NULL))
    {
      return -1;
    }

  for (idx = 0; longopts[idx].name != NULL; idx++)
    {
      opt = &longopts[idx];
      namelen = strlen (opt->name);

      if (strncmp (arg, opt->name, namelen) == 0)
        {
          switch (arg[namelen])
            {
            case '\0':
              switch (opt->has_arg)
                {
                case nsfw_required_argument:
                  nsfw_optind++;

                  if (nsfw_optind == argc)
                    {
                      nsfw_optarg = NULL;
                      nsfw_optopt = opt->val;
                      if (':' != optstring[0])
                        {
                          NSFW_LOGERR
                            ("requires an argument] argv_0=%s, opt name=%s",
                             argv[0], opt->name);
                        }
                      return optstring[0] == ':' ? ':' : '?';
                    }

                  val = argv[nsfw_optind];
                  break;

                default:
                  break;
                }

              goto found;

            case '=':
              if (opt->has_arg == nsfw_no_argument)
                {
                  const char *hyphens =
                    (argv[nsfw_optind][1] == '-') ? "--" : "-";
                  nsfw_optind++;
                  nsfw_optarg = NULL;
                  nsfw_optopt = opt->val;
                  if (':' != optstring[0])
                    {
                      NSFW_LOGERR
                        ("doesn't allow an argument] argv_0=%s, hyphens=%s, opt name=%s",
                         argv[0], hyphens, opt->name);
                    }
                  return '?';
                }

              val = arg + namelen + 1;
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
  nsfw_optarg = val;
  nsfw_optind++;

  if (opt->flag)
    {
      *opt->flag = opt->val;
    }

  if (longindex)
    {
      *longindex = idx;
    }

  return opt->flag ? 0 : opt->val;
}
