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

#ifndef __TRP_RB_TREE_H__
#define __TRP_RB_TREE_H__

typedef void *trp_key_t;
typedef void *trp_data_t;

#define RB_RED 0
#define RB_BLACK 1

typedef int (*key_compare) (trp_key_t left, trp_key_t right);   // return > 0 left > right, return 0 left = right, return < 0 left < right

typedef struct trp_rb_node
{
  struct trp_rb_node *rb_parent;
  struct trp_rb_node *rb_right;
  struct trp_rb_node *rb_left;
  key_compare key_compare_fn;
  trp_key_t key;
  trp_data_t data;
  unsigned int color;
} trp_rb_node_t;

typedef struct trp_rb_root
{
  struct trp_rb_node *rb_node;
} trp_rb_root_t;

int trp_rb_insert (trp_key_t, trp_data_t, trp_rb_root_t *, key_compare);
int trp_rb_insert_allow_same_key (trp_key_t, trp_data_t, trp_rb_root_t *,
                                  key_compare);
void trp_rb_erase (trp_key_t, trp_rb_root_t *, key_compare key_compare_fn);
void trp_rb_erase_with_data (trp_key_t key, trp_data_t data,
                             trp_rb_root_t * root, int count,
                             key_compare key_compare_fn);

static inline trp_rb_node_t *
trp_rb_search (trp_key_t key, trp_rb_root_t * root,
               key_compare key_compare_fn)
{
  trp_rb_node_t *node = root->rb_node;
  int ret;
  while (node)
    {
      ret = key_compare_fn (node->key, key);
      if (0 < ret)
        {
          node = node->rb_left;
        }
      else if (0 > ret)
        {
          node = node->rb_right;
        }
      else
        {
          return node;
        }
    }

  return NULL;
}
#endif
