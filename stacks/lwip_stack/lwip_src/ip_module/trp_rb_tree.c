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
#include <stdlib.h>
#include <string.h>
#include "trp_rb_tree.h"

NSTACK_STATIC void
__rb_rotate_left (struct trp_rb_node *X, struct trp_rb_root *root)
{
    /**************************
     *  rotate Node X to left *
     **************************/

  struct trp_rb_node *Y = X->rb_right;

  /* estblish X->Right link */
  X->rb_right = Y->rb_left;
  if (Y->rb_left != NULL)
    Y->rb_left->rb_parent = X;

  /* estblish Y->Parent link */
  Y->rb_parent = X->rb_parent;
  if (X->rb_parent)
    {
      if (X == X->rb_parent->rb_left)
        X->rb_parent->rb_left = Y;
      else
        X->rb_parent->rb_right = Y;
    }
  else
    {
      root->rb_node = Y;
    }

  /* link X and Y */
  Y->rb_left = X;
  X->rb_parent = Y;

  return;
}

NSTACK_STATIC void
__rb_rotate_right (struct trp_rb_node *X, struct trp_rb_root *root)
{
    /****************************
     *  rotate Node X to right  *
     ****************************/

  struct trp_rb_node *Y = X->rb_left;

  /* estblish X->Left link */
  X->rb_left = Y->rb_right;
  if (Y->rb_right != NULL)
    Y->rb_right->rb_parent = X;

  /* estblish Y->Parent link */
  Y->rb_parent = X->rb_parent;
  if (X->rb_parent)
    {
      if (X == X->rb_parent->rb_right)
        X->rb_parent->rb_right = Y;
      else
        X->rb_parent->rb_left = Y;
    }
  else
    {
      root->rb_node = Y;
    }

  /* link X and Y */
  Y->rb_right = X;
  X->rb_parent = Y;

  return;
}

/* X, Y are for application */
NSTACK_STATIC void
__rb_erase_color (struct trp_rb_node *X, struct trp_rb_node *Parent,
                  struct trp_rb_root *root)
{
    /*************************************
     *  maintain red-black tree balance  *
     *  after deleting node X            *
     *************************************/

  while (X != root->rb_node && (!X || X->color == RB_BLACK))
    {

      if (Parent == NULL)
        {
          break;
        }

      if (X == Parent->rb_left)
        {
          struct trp_rb_node *W = Parent->rb_right;
          if (W->color == RB_RED)
            {
              W->color = RB_BLACK;
              Parent->color = RB_RED;   /* Parent != NIL? */
              __rb_rotate_left (Parent, root);
              W = Parent->rb_right;
            }

          if ((!W->rb_left || W->rb_left->color == RB_BLACK)
              && (!W->rb_right || W->rb_right->color == RB_BLACK))
            {
              W->color = RB_RED;
              X = Parent;
              Parent = X->rb_parent;
            }
          else
            {
              if (!W->rb_right || W->rb_right->color == RB_BLACK)
                {
                  if (W->rb_left != NULL)
                    W->rb_left->color = RB_BLACK;
                  W->color = RB_RED;
                  __rb_rotate_right (W, root);
                  W = Parent->rb_right;
                }

              W->color = Parent->color;
              Parent->color = RB_BLACK;
              if (W->rb_right->color != RB_BLACK)
                {
                  W->rb_right->color = RB_BLACK;
                }
              __rb_rotate_left (Parent, root);
              X = root->rb_node;
              break;
            }
        }
      else
        {

          struct trp_rb_node *W = Parent->rb_left;
          if (W->color == RB_RED)
            {
              W->color = RB_BLACK;
              Parent->color = RB_RED;   /* Parent != NIL? */
              __rb_rotate_right (Parent, root);
              W = Parent->rb_left;
            }

          if ((!W->rb_left || (W->rb_left->color == RB_BLACK))
              && (!W->rb_right || (W->rb_right->color == RB_BLACK)))
            {
              W->color = RB_RED;
              X = Parent;
              Parent = X->rb_parent;
            }
          else
            {
              if (!W->rb_left || (W->rb_left->color == RB_BLACK))
                {
                  if (W->rb_right != NULL)
                    W->rb_right->color = RB_BLACK;
                  W->color = RB_RED;
                  __rb_rotate_left (W, root);
                  W = Parent->rb_left;
                }

              W->color = Parent->color;
              Parent->color = RB_BLACK;
              if (W->rb_left->color != RB_BLACK)
                {
                  W->rb_left->color = RB_BLACK;
                }
              __rb_rotate_right (Parent, root);
              X = root->rb_node;
              break;
            }
        }
    }

  if (X)
    {
      X->color = RB_BLACK;
    }

  return;
}

static void
rb_insert_color (struct trp_rb_node *X, struct trp_rb_root *root)
{
    /*************************************
     *  maintain red-black tree balance  *
     *  after inserting node X           *
     *************************************/

  /* check red-black properties */
  while (X != root->rb_node && X->rb_parent->color == RB_RED)
    {
      /* we have a violation */
      if (X->rb_parent == X->rb_parent->rb_parent->rb_left)
        {
          struct trp_rb_node *Y = X->rb_parent->rb_parent->rb_right;
          if (Y && Y->color == RB_RED)
            {

              /* uncle is red */
              X->rb_parent->color = RB_BLACK;
              Y->color = RB_BLACK;
              X->rb_parent->rb_parent->color = RB_RED;
              X = X->rb_parent->rb_parent;
            }
          else
            {

              /* uncle is black */
              if (X == X->rb_parent->rb_right)
                {
                  /* make X a left child */
                  X = X->rb_parent;
                  __rb_rotate_left (X, root);
                }

              /* recolor and rotate */
              X->rb_parent->color = RB_BLACK;
              X->rb_parent->rb_parent->color = RB_RED;
              __rb_rotate_right (X->rb_parent->rb_parent, root);
            }
        }
      else
        {

          /* miror image of above code */
          struct trp_rb_node *Y = X->rb_parent->rb_parent->rb_left;
          if (Y && (Y->color == RB_RED))
            {

              /* uncle is red */
              X->rb_parent->color = RB_BLACK;
              Y->color = RB_BLACK;
              X->rb_parent->rb_parent->color = RB_RED;
              X = X->rb_parent->rb_parent;
            }
          else
            {

              /* uncle is black */
              if (X == X->rb_parent->rb_left)
                {
                  X = X->rb_parent;
                  __rb_rotate_right (X, root);
                }
              X->rb_parent->color = RB_BLACK;
              X->rb_parent->rb_parent->color = RB_RED;
              __rb_rotate_left (X->rb_parent->rb_parent, root);
            }
        }
    }

  root->rb_node->color = RB_BLACK;

  return;
}

static void
rb_erase (struct trp_rb_node *node, struct trp_rb_root *root)
{
  struct trp_rb_node *child, *parent;
  unsigned int color;

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
      struct trp_rb_node *old = node, *left;

      node = node->rb_right;
      while ((left = node->rb_left) != NULL)
        {
          node = left;
        }

      if (old->rb_parent)
        {
          if (old->rb_parent->rb_left == old)
            {
              old->rb_parent->rb_left = node;
            }
          else
            {
              old->rb_parent->rb_right = node;
            }
        }
      else
        {
          root->rb_node = node;
        }

      child = node->rb_right;
      parent = node->rb_parent;
      color = node->color;

      if (parent == old)
        {
          parent = node;
        }
      else
        {
          if (child)
            {
              child->rb_parent = parent;
            }

          parent->rb_left = child;

          node->rb_right = old->rb_right;
          old->rb_right->rb_parent = node;
        }

      node->color = old->color;
      node->rb_parent = old->rb_parent;
      node->rb_left = old->rb_left;
      old->rb_left->rb_parent = node;

      if (color == RB_BLACK)
        {
          __rb_erase_color (child, parent, root);
        }

      return;

    }

  parent = node->rb_parent;
  color = node->color;

  if (child)
    {
      child->rb_parent = parent;
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

  if (color == RB_BLACK)
    {
      __rb_erase_color (child, parent, root);
    }

  return;
}

NSTACK_STATIC trp_rb_node_t *
rb_new_node (trp_key_t key, trp_data_t data,
             trp_rb_node_t * parent /*, key_compare key_compare_fn */ )
{
  trp_rb_node_t *node = (trp_rb_node_t *) malloc (sizeof (trp_rb_node_t));
  if (!node)
    {
      return NULL;
    }
  node->key = key;
  node->data = data;
  node->rb_parent = parent;
  node->rb_left = node->rb_right = NULL;
  node->color = RB_RED;
  /*node->key_compare_fn = key_compare_fn; */
  return node;
}

int
trp_rb_insert (trp_key_t key, trp_data_t data, trp_rb_root_t * root,
               key_compare key_compare_fn)
{
  trp_rb_node_t *node = root->rb_node, *parent = NULL;
  int ret = 0;                  /* CID 24640 */
  while (node)
    {
      parent = node;
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
          return -1;
        }
    }

  node = rb_new_node (key, data, parent /*, key_compare_fn */ );
  if (!node)
    {
      return -1;
    }

  if (parent)
    {
      if (ret > 0)
        {
          parent->rb_left = node;
        }
      else
        {
          parent->rb_right = node;
        }
    }
  else
    {
      root->rb_node = node;
    }

  rb_insert_color (node, root);
  return 0;
}

int
trp_rb_insert_allow_same_key (trp_key_t key, trp_data_t data,
                              trp_rb_root_t * root,
                              key_compare key_compare_fn)
{
  trp_rb_node_t *node = root->rb_node, *parent = NULL;
  int ret = 0;                  /*CID 24638 */
  while (node)
    {
      parent = node;
      ret = key_compare_fn (node->key, key);
      if (0 < ret)
        {
          node = node->rb_left;
        }
      else
        {
          node = node->rb_right;
        }
    }

  node = rb_new_node (key, data, parent /*, key_compare_fn */ );
  if (!node)
    {
      return -1;
    }

  if (parent)
    {
      if (ret > 0)
        {
          parent->rb_left = node;
        }
      else
        {
          parent->rb_right = node;
        }
    }
  else
    {
      root->rb_node = node;
    }

  rb_insert_color (node, root);
  return 0;
}

NSTACK_STATIC trp_rb_node_t *
trp_rb_search_inorder (trp_key_t key, trp_data_t data,
                       trp_rb_node_t * node, int *count,
                       key_compare key_compare_fn)
{
  if (!node)
    {
      return NULL;
    }

  int ret = key_compare_fn (node->key, key);;
  if (0 == ret && data == node->data)
    {
      return node;
    }

  if ((NULL == count) || (0 >= --(*count)))
    {
      return NULL;
    }

  trp_rb_node_t *ret_node =
    trp_rb_search_inorder (key, data, node->rb_left, count, key_compare_fn);
  if (ret_node)
    {
      return ret_node;
    }

  ret_node =
    trp_rb_search_inorder (key, data, node->rb_right, count, key_compare_fn);
  return ret_node;
}

void
trp_rb_erase_with_data (trp_key_t key, trp_data_t data, trp_rb_root_t * root,
                        int count, key_compare key_compare_fn)
{
  trp_rb_node_t *node;
  /* recursion operation need depth protect */
  if (!
      (node =
       trp_rb_search_inorder (key, data, root->rb_node, &count,
                              key_compare_fn)))
    {
      return;
    }

  rb_erase (node, root);
  free (node);
  node = NULL;
}

void
trp_rb_erase (trp_key_t key, trp_rb_root_t * root, key_compare key_compare_fn)
{
  trp_rb_node_t *node;
  if (!(node = trp_rb_search (key, root, key_compare_fn)))
    {
      return;
    }

  rb_erase (node, root);
  free (node);
  node = NULL;
}
