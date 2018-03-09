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

#ifndef _EPRB_TREE_H_
#define _EPRB_TREE_H_

#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "common_mem_pal.h"
#include "common_mem_buf.h"
#include "common_pal_bitwide_adjust.h"
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define EP_RB_RED 0
#define EP_RB_BLACK 1

struct ep_rb_node
{
  int color;

  struct ep_rb_node *rb_parent;
  struct ep_rb_node *rb_right;
  struct ep_rb_node *rb_left;
};

/* The alignment might seem pointless, but allegedly CRIS needs it */

struct ep_rb_root
{
  struct ep_rb_node *rb_node;
};

#define ep_rb_parent(r) ((struct ep_rb_node *)((r)->rb_parent))

static inline void
ep_rb_set_parent (struct ep_rb_node *rb, struct ep_rb_node *p)
{
  rb->rb_parent = (struct ep_rb_node *) ADDR_LTOSH_EXT (p);
}

#define ep_rb_entry(ptr, type, member) container_of(ptr, type, member)

extern void ep_rb_insert_color (struct ep_rb_node *, struct ep_rb_root *);
extern void ep_rb_erase (struct ep_rb_node *, struct ep_rb_root *);
struct ep_rb_node *ep_rb_first (const struct ep_rb_root *);

static inline void
ep_rb_link_node (struct ep_rb_node *node,
                 struct ep_rb_node *parent, struct ep_rb_node **rb_link)
{

  node->rb_parent = (struct ep_rb_node *) ADDR_LTOSH_EXT (parent);
  node->rb_left = node->rb_right = NULL;

  *rb_link = (struct ep_rb_node *) ADDR_LTOSH_EXT (node);
  node->color = EP_RB_RED;

}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
