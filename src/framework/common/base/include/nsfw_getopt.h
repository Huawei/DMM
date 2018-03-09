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

#ifndef NSFW_GETOPT_H
#define NSFW_GETOPT_H 1

#if defined(__cplusplus)
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define nsfw_no_argument        0
#define nsfw_required_argument  1
#define nsfw_optional_argument  2

struct option
{
  const char *name;
  int has_arg;
  int *flag;
  int val;
};

int nsfw_getopt_long (int argc, char *const argv[], const char *optstring,
                      const struct option *longopts, int *longindex);

extern char *nsfw_optarg;
extern int nsfw_optind, nsfw_opterr, nsfw_optopt;

#if defined(__cplusplus)
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
