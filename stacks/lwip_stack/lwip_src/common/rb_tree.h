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

#ifndef _UNX_RBTREE_H
#define _UNX_RBTREE_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif
#include <stdint.h>
#include "types.h"
#include "common_pal_bitwide_adjust.h"

#define rb_parent(a) ((struct rb_node *)((a)->rb_parent_color & ~3))
#define rb_color(a) ((a)->rb_parent_color & 1)

struct rb_node
{
  union
  {
    unsigned long rb_parent_color;
    u64 rb_parent_color_a;
  };

#define RB_RED 0
#define RB_BLACK 1
  union
  {
    struct rb_node *rb_right;
    u64 rb_right_a;
  };

  union
  {
    struct rb_node *rb_left;
    u64 rb_left_a;
  };
};

#define rb_set_red(c) do { (c)->rb_parent_color &= ~1; } while (0)
#define rb_set_black(c) do { (c)->rb_parent_color |= 1; } while (0)

/* The alignment might seem pointless, but allegedly CRIS needs it */

struct rb_root
{
  union
  {
    struct rb_node *rb_node;
    u64 rb_node_a;
  };
};

#define rb_is_red(e) (!rb_color(e))
#define rb_is_black(e) rb_color(e)

static inline void
rb_set_color (struct rb_node *rb1, int color2)
{
  rb1->rb_parent_color = (rb1->rb_parent_color & ~1) | color2;
}

static inline void
rb_set_parent (struct rb_node *rb1, struct rb_node *pa)
{
  rb1->rb_parent_color = (rb1->rb_parent_color & 3) | (unsigned long) pa;
}

#define RB_ROOT (struct rb_root) { NULL, }

extern void rb_erase (struct rb_node *, struct rb_root *);

extern void rb_insert_color (struct rb_node *, struct rb_root *);

extern struct rb_node *rb_next (const struct rb_node *);

#define rb_entry(ptr, type, member) container_of(ptr, type, member)

#define RB_EMPTY_ROOT(root1) ((root1)->rb_node == NULL)
#define RB_CLEAR_NODE(node2) (rb_set_parent(node2, node2))
#define RB_EMPTY_NODE(node1) (rb_parent(node1) == node1)

static inline void
rb_link_node (struct rb_node *node1, struct rb_node *parent1,
              struct rb_node **rb_link1)
{
  node1->rb_left = node1->rb_right = NULL;
  node1->rb_parent_color = (unsigned long) parent1;
  *rb_link1 = node1;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#endif /* _UNX_RBTREE_H */
