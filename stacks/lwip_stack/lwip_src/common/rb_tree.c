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

#include "rb_tree.h"

static void
__rb_rotate_right (struct rb_node *node, struct rb_root *root)
{
  struct rb_node *left = node->rb_left;
  struct rb_node *parent = rb_parent (node);

  if ((node->rb_left = left->rb_right))
    {
      rb_set_parent (left->rb_right, node);
    }

  left->rb_right = node;

  rb_set_parent (left, parent);

  if (parent)
    {
      if (node == parent->rb_right)
        {
          parent->rb_right = left;
        }
      else
        {
          parent->rb_left = left;
        }
    }
  else
    {
      root->rb_node = left;
    }

  rb_set_parent (node, left);
}

static void
__rb_rotate_left (struct rb_node *node, struct rb_root *root)
{
  struct rb_node *parent = rb_parent (node);

  struct rb_node *right = node->rb_right;

  if ((node->rb_right = right->rb_left))
    {
      rb_set_parent (right->rb_left, node);
    }

  right->rb_left = node;
  rb_set_parent (right, parent);

  if (parent)                   /* judge parent node */
    {
      if (node == parent->rb_left)
        {
          parent->rb_left = right;
        }
      else
        {
          parent->rb_right = right;
        }
    }
  else
    {
      root->rb_node = right;
    }

  rb_set_parent (node, right);
}

static void
__rb_erase_color (struct rb_node *rb_tree_node,
                  struct rb_node *rb_tree_parent,
                  struct rb_root *rb_tree_root)
{
  struct rb_node *rb_tree_other;

  while ((!rb_tree_node || rb_is_black (rb_tree_node))
         && (rb_tree_node != rb_tree_root->rb_node))
    {
      if (rb_tree_parent == NULL)
        {
          break;
        }

      if (rb_tree_parent->rb_left == rb_tree_node)
        {
          rb_tree_other = rb_tree_parent->rb_right;
          if (rb_is_red (rb_tree_other))
            {
              rb_set_black (rb_tree_other);
              rb_set_red (rb_tree_parent);
              __rb_rotate_left (rb_tree_parent, rb_tree_root);
              rb_tree_other = rb_tree_parent->rb_right;
            }

          if ((!rb_tree_other->rb_left
               || rb_is_black (rb_tree_other->rb_left))
              && (!rb_tree_other->rb_right
                  || rb_is_black (rb_tree_other->rb_right)))
            {
              rb_set_red (rb_tree_other);
              rb_tree_node = rb_tree_parent;
              rb_tree_parent = rb_parent (rb_tree_node);
            }
          else
            {
              if (!rb_tree_other->rb_right
                  || rb_is_black (rb_tree_other->rb_right))
                {
                  rb_set_black (rb_tree_other->rb_left);
                  rb_set_red (rb_tree_other);
                  __rb_rotate_right (rb_tree_other, rb_tree_root);
                  rb_tree_other = rb_tree_parent->rb_right;
                }

              rb_set_color (rb_tree_other, rb_color (rb_tree_parent));
              rb_set_black (rb_tree_parent);
              rb_set_black (rb_tree_other->rb_right);
              __rb_rotate_left (rb_tree_parent, rb_tree_root);
              rb_tree_node = rb_tree_root->rb_node;
              break;
            }
        }
      else
        {
          rb_tree_other = rb_tree_parent->rb_left;
          if (rb_is_red (rb_tree_other))
            {
              rb_set_black (rb_tree_other);
              rb_set_red (rb_tree_parent);
              __rb_rotate_right (rb_tree_parent, rb_tree_root);
              rb_tree_other = rb_tree_parent->rb_left;
            }

          if ((!rb_tree_other->rb_left
               || rb_is_black (rb_tree_other->rb_left))
              && (!rb_tree_other->rb_right
                  || rb_is_black (rb_tree_other->rb_right)))
            {
              rb_set_red (rb_tree_other);
              rb_tree_node = rb_tree_parent;
              rb_tree_parent = rb_parent (rb_tree_node);
            }
          else
            {
              if (!rb_tree_other->rb_left
                  || rb_is_black (rb_tree_other->rb_left))
                {
                  rb_set_black (rb_tree_other->rb_right);
                  rb_set_red (rb_tree_other);
                  __rb_rotate_left (rb_tree_other, rb_tree_root);
                  rb_tree_other = rb_tree_parent->rb_left;
                }

              rb_set_color (rb_tree_other, rb_color (rb_tree_parent));
              rb_set_black (rb_tree_parent);
              rb_set_black (rb_tree_other->rb_left);
              __rb_rotate_right (rb_tree_parent, rb_tree_root);
              rb_tree_node = rb_tree_root->rb_node;
              break;
            }
        }
    }

  if (rb_tree_node)
    {
      rb_set_black (rb_tree_node);
    }
}

void
rb_insert_color (struct rb_node *rb_tree_node, struct rb_root *rb_tree_root)
{
  struct rb_node *rb_tree_parent, *rb_tree_gparent;

  if (!rb_tree_node || !rb_tree_root)
    return;

  while ((rb_tree_parent = rb_parent (rb_tree_node))
         && rb_is_red (rb_tree_parent))
    {
      rb_tree_gparent = rb_parent (rb_tree_parent);

      if (rb_tree_parent == rb_tree_gparent->rb_left)
        {
          {
            register struct rb_node *rb_tree_uncle =
              rb_tree_gparent->rb_right;
            if (rb_tree_uncle && rb_is_red (rb_tree_uncle))
              {
                rb_set_black (rb_tree_uncle);
                rb_set_black (rb_tree_parent);
                rb_set_red (rb_tree_gparent);
                rb_tree_node = rb_tree_gparent;
                continue;
              }
          }

          if (rb_tree_parent->rb_right == rb_tree_node)
            {
              register struct rb_node *rb_tree_tmp;
              __rb_rotate_left (rb_tree_parent, rb_tree_root);
              rb_tree_tmp = rb_tree_parent;
              rb_tree_parent = rb_tree_node;
              rb_tree_node = rb_tree_tmp;
            }

          rb_set_black (rb_tree_parent);
          rb_set_red (rb_tree_gparent);
          __rb_rotate_right (rb_tree_gparent, rb_tree_root);
        }
      else
        {
          {
            register struct rb_node *rb_tree_uncle = rb_tree_gparent->rb_left;
            if (rb_tree_uncle && rb_is_red (rb_tree_uncle))
              {
                rb_set_black (rb_tree_uncle);
                rb_set_black (rb_tree_parent);
                rb_set_red (rb_tree_gparent);
                rb_tree_node = rb_tree_gparent;
                continue;
              }
          }

          if (rb_tree_parent->rb_left == rb_tree_node)
            {
              register struct rb_node *rb_tree_tmp;
              __rb_rotate_right (rb_tree_parent, rb_tree_root);
              rb_tree_tmp = rb_tree_parent;
              rb_tree_parent = rb_tree_node;
              rb_tree_node = rb_tree_tmp;
            }

          rb_set_black (rb_tree_parent);
          rb_set_red (rb_tree_gparent);
          __rb_rotate_left (rb_tree_gparent, rb_tree_root);
        }
    }

  rb_set_black (rb_tree_root->rb_node);
}

void
rb_erase (struct rb_node *node, struct rb_root *root)
{
  struct rb_node *child, *parent;
  int color;

  if (!node || !root)
    return;

  if (!node->rb_left)
    {
      child = node->rb_right;
    }
  else if (!node->rb_right)
    {
      child = node->rb_left;
    }
  else
    {
      struct rb_node *old = node, *left;

      node = node->rb_right;
      while ((left = node->rb_left) != NULL)
        {
          node = left;
        }

      if (rb_parent (old))
        {
          if (rb_parent (old)->rb_left == old)
            {
              rb_parent (old)->rb_left = node;
            }
          else
            {
              rb_parent (old)->rb_right = node;
            }
        }
      else
        {
          root->rb_node = node;
        }

      child = node->rb_right;
      parent = rb_parent (node);
      color = rb_color (node);

      if (parent == old)
        {
          parent = node;
        }
      else
        {
          if (child)
            {
              rb_set_parent (child, parent);
            }

          parent->rb_left = child;

          node->rb_right = old->rb_right;
          rb_set_parent (old->rb_right, node);
        }

      node->rb_parent_color = old->rb_parent_color;
      node->rb_left = old->rb_left;
      rb_set_parent (old->rb_left, node);

      goto color;
    }

  parent = rb_parent (node);
  color = rb_color (node);

  if (child)
    {
      rb_set_parent (child, parent);
    }

  if (parent)
    {
      if (parent->rb_left == node)
        {
          parent->rb_left = child;
        }
      else
        {
          parent->rb_right = child;
        }
    }
  else
    {
      root->rb_node = child;
    }

color:
  if (color == RB_BLACK)
    {
      __rb_erase_color (child, parent, root);
    }
}

struct rb_node *
rb_next (const struct rb_node *node)
{
  struct rb_node *parent;

  if (!node)
    return NULL;

  if (rb_parent (node) == node)
    {
      return NULL;
    }

  if (node->rb_right)
    {
      node = node->rb_right;
      while (node->rb_left)
        {
          node = node->rb_left;
        }

      return (struct rb_node *) node;
    }

  while ((parent = rb_parent (node)) && (node == parent->rb_right))
    {
      node = parent;
    }

  return parent;
}
