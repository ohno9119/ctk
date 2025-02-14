/* ctkliststore.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <gobject/gvaluecollector.h>
#include "ctktreemodel.h"
#include "ctkliststore.h"
#include "ctktreedatalist.h"
#include "ctktreednd.h"
#include "ctkintl.h"
#include "ctkbuildable.h"
#include "ctkbuilderprivate.h"


/**
 * SECTION:ctkliststore
 * @Short_description: A list-like data structure that can be used with the CtkTreeView
 * @Title: CtkListStore
 * @See_also:#CtkTreeModel, #CtkTreeStore
 *
 * The #CtkListStore object is a list model for use with a #CtkTreeView
 * widget.  It implements the #CtkTreeModel interface, and consequentialy,
 * can use all of the methods available there.  It also implements the
 * #CtkTreeSortable interface so it can be sorted by the view.
 * Finally, it also implements the tree
 * [drag and drop][ctk3-CtkTreeView-drag-and-drop]
 * interfaces.
 *
 * The #CtkListStore can accept most GObject types as a column type, though
 * it can’t accept all custom types.  Internally, it will keep a copy of
 * data passed in (such as a string or a boxed pointer).  Columns that
 * accept #GObjects are handled a little differently.  The
 * #CtkListStore will keep a reference to the object instead of copying the
 * value.  As a result, if the object is modified, it is up to the
 * application writer to call ctk_tree_model_row_changed() to emit the
 * #CtkTreeModel::row_changed signal.  This most commonly affects lists with
 * #GdkPixbufs stored.
 *
 * An example for creating a simple list store:
 * |[<!-- language="C" -->
 * enum {
 *   COLUMN_STRING,
 *   COLUMN_INT,
 *   COLUMN_BOOLEAN,
 *   N_COLUMNS
 * };
 *
 * {
 *   CtkListStore *list_store;
 *   CtkTreePath *path;
 *   CtkTreeIter iter;
 *   gint i;
 *
 *   list_store = ctk_list_store_new (N_COLUMNS,
 *                                    G_TYPE_STRING,
 *                                    G_TYPE_INT,
 *                                    G_TYPE_BOOLEAN);
 *
 *   for (i = 0; i < 10; i++)
 *     {
 *       gchar *some_data;
 *
 *       some_data = get_some_data (i);
 *
 *       // Add a new row to the model
 *       ctk_list_store_append (list_store, &iter);
 *       ctk_list_store_set (list_store, &iter,
 *                           COLUMN_STRING, some_data,
 *                           COLUMN_INT, i,
 *                           COLUMN_BOOLEAN,  FALSE,
 *                           -1);
 *
 *       // As the store will keep a copy of the string internally,
 *       // we free some_data.
 *       g_free (some_data);
 *     }
 *
 *   // Modify a particular row
 *   path = ctk_tree_path_new_from_string ("4");
 *   ctk_tree_model_get_iter (CTK_TREE_MODEL (list_store),
 *                            &iter,
 *                            path);
 *   ctk_tree_path_free (path);
 *   ctk_list_store_set (list_store, &iter,
 *                       COLUMN_BOOLEAN, TRUE,
 *                       -1);
 * }
 * ]|
 *
 * # Performance Considerations
 *
 * Internally, the #CtkListStore was implemented with a linked list with
 * a tail pointer prior to CTK+ 2.6.  As a result, it was fast at data
 * insertion and deletion, and not fast at random data access.  The
 * #CtkListStore sets the #CTK_TREE_MODEL_ITERS_PERSIST flag, which means
 * that #CtkTreeIters can be cached while the row exists.  Thus, if
 * access to a particular row is needed often and your code is expected to
 * run on older versions of CTK+, it is worth keeping the iter around.
 *
 * # Atomic Operations
 *
 * It is important to note that only the methods
 * ctk_list_store_insert_with_values() and ctk_list_store_insert_with_valuesv()
 * are atomic, in the sense that the row is being appended to the store and the
 * values filled in in a single operation with regard to #CtkTreeModel signaling.
 * In contrast, using e.g. ctk_list_store_append() and then ctk_list_store_set()
 * will first create a row, which triggers the #CtkTreeModel::row-inserted signal
 * on #CtkListStore. The row, however, is still empty, and any signal handler
 * connecting to #CtkTreeModel::row-inserted on this particular store should be prepared
 * for the situation that the row might be empty. This is especially important
 * if you are wrapping the #CtkListStore inside a #CtkTreeModelFilter and are
 * using a #CtkTreeModelFilterVisibleFunc. Using any of the non-atomic operations
 * to append rows to the #CtkListStore will cause the
 * #CtkTreeModelFilterVisibleFunc to be visited with an empty row first; the
 * function must be prepared for that.
 *
 * # CtkListStore as CtkBuildable
 *
 * The CtkListStore implementation of the CtkBuildable interface allows
 * to specify the model columns with a <columns> element that may contain
 * multiple <column> elements, each specifying one model column. The “type”
 * attribute specifies the data type for the column.
 *
 * Additionally, it is possible to specify content for the list store
 * in the UI definition, with the <data> element. It can contain multiple
 * <row> elements, each specifying to content for one row of the list model.
 * Inside a <row>, the <col> elements specify the content for individual cells.
 *
 * Note that it is probably more common to define your models in the code,
 * and one might consider it a layering violation to specify the content of
 * a list store in a UI definition, data, not presentation, and common wisdom
 * is to separate the two, as far as possible.
 *
 * An example of a UI Definition fragment for a list store:
 * |[<!-- language="C" -->
 * <object class="CtkListStore">
 *   <columns>
 *     <column type="gchararray"/>
 *     <column type="gchararray"/>
 *     <column type="gint"/>
 *   </columns>
 *   <data>
 *     <row>
 *       <col id="0">John</col>
 *       <col id="1">Doe</col>
 *       <col id="2">25</col>
 *     </row>
 *     <row>
 *       <col id="0">Johan</col>
 *       <col id="1">Dahlin</col>
 *       <col id="2">50</col>
 *     </row>
 *   </data>
 * </object>
 * ]|
 */


struct _CtkListStorePrivate
{
  CtkTreeIterCompareFunc default_sort_func;

  GDestroyNotify default_sort_destroy;
  GList *sort_list;
  GType *column_headers;

  gint stamp;
  gint n_columns;
  gint sort_column_id;
  gint length;

  CtkSortType order;

  guint columns_dirty : 1;

  gpointer default_sort_data;
  gpointer seq;         /* head of the list */
};

#define CTK_LIST_STORE_IS_SORTED(list) (((CtkListStore*)(list))->priv->sort_column_id != CTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
static void         ctk_list_store_tree_model_init (CtkTreeModelIface *iface);
static void         ctk_list_store_drag_source_init(CtkTreeDragSourceIface *iface);
static void         ctk_list_store_drag_dest_init  (CtkTreeDragDestIface   *iface);
static void         ctk_list_store_sortable_init   (CtkTreeSortableIface   *iface);
static void         ctk_list_store_buildable_init  (CtkBuildableIface      *iface);
static void         ctk_list_store_finalize        (GObject           *object);
static CtkTreeModelFlags ctk_list_store_get_flags  (CtkTreeModel      *tree_model);
static gint         ctk_list_store_get_n_columns   (CtkTreeModel      *tree_model);
static GType        ctk_list_store_get_column_type (CtkTreeModel      *tree_model,
						    gint               index);
static gboolean     ctk_list_store_get_iter        (CtkTreeModel      *tree_model,
						    CtkTreeIter       *iter,
						    CtkTreePath       *path);
static CtkTreePath *ctk_list_store_get_path        (CtkTreeModel      *tree_model,
						    CtkTreeIter       *iter);
static void         ctk_list_store_get_value       (CtkTreeModel      *tree_model,
						    CtkTreeIter       *iter,
						    gint               column,
						    GValue            *value);
static gboolean     ctk_list_store_iter_next       (CtkTreeModel      *tree_model,
						    CtkTreeIter       *iter);
static gboolean     ctk_list_store_iter_previous   (CtkTreeModel      *tree_model,
						    CtkTreeIter       *iter);
static gboolean     ctk_list_store_iter_children   (CtkTreeModel      *tree_model,
						    CtkTreeIter       *iter,
						    CtkTreeIter       *parent);
static gboolean     ctk_list_store_iter_has_child  (CtkTreeModel      *tree_model,
						    CtkTreeIter       *iter);
static gint         ctk_list_store_iter_n_children (CtkTreeModel      *tree_model,
						    CtkTreeIter       *iter);
static gboolean     ctk_list_store_iter_nth_child  (CtkTreeModel      *tree_model,
						    CtkTreeIter       *iter,
						    CtkTreeIter       *parent,
						    gint               n);
static gboolean     ctk_list_store_iter_parent     (CtkTreeModel      *tree_model,
						    CtkTreeIter       *iter,
						    CtkTreeIter       *child);


static void ctk_list_store_set_n_columns   (CtkListStore *list_store,
					    gint          n_columns);
static void ctk_list_store_set_column_type (CtkListStore *list_store,
					    gint          column,
					    GType         type);

static void ctk_list_store_increment_stamp (CtkListStore *list_store);


/* Drag and Drop */
static gboolean real_ctk_list_store_row_draggable (CtkTreeDragSource *drag_source,
                                                   CtkTreePath       *path);
static gboolean ctk_list_store_drag_data_delete   (CtkTreeDragSource *drag_source,
                                                   CtkTreePath       *path);
static gboolean ctk_list_store_drag_data_get      (CtkTreeDragSource *drag_source,
                                                   CtkTreePath       *path,
                                                   CtkSelectionData  *selection_data);
static gboolean ctk_list_store_drag_data_received (CtkTreeDragDest   *drag_dest,
                                                   CtkTreePath       *dest,
                                                   CtkSelectionData  *selection_data);
static gboolean ctk_list_store_row_drop_possible  (CtkTreeDragDest   *drag_dest,
                                                   CtkTreePath       *dest_path,
						   CtkSelectionData  *selection_data);


/* sortable */
static void     ctk_list_store_sort                  (CtkListStore           *list_store);
static void     ctk_list_store_sort_iter_changed     (CtkListStore           *list_store,
						      CtkTreeIter            *iter,
						      gint                    column);
static gboolean ctk_list_store_get_sort_column_id    (CtkTreeSortable        *sortable,
						      gint                   *sort_column_id,
						      CtkSortType            *order);
static void     ctk_list_store_set_sort_column_id    (CtkTreeSortable        *sortable,
						      gint                    sort_column_id,
						      CtkSortType             order);
static void     ctk_list_store_set_sort_func         (CtkTreeSortable        *sortable,
						      gint                    sort_column_id,
						      CtkTreeIterCompareFunc  func,
						      gpointer                data,
						      GDestroyNotify          destroy);
static void     ctk_list_store_set_default_sort_func (CtkTreeSortable        *sortable,
						      CtkTreeIterCompareFunc  func,
						      gpointer                data,
						      GDestroyNotify          destroy);
static gboolean ctk_list_store_has_default_sort_func (CtkTreeSortable        *sortable);


/* buildable */
static gboolean ctk_list_store_buildable_custom_tag_start (CtkBuildable  *buildable,
							   CtkBuilder    *builder,
							   GObject       *child,
							   const gchar   *tagname,
							   GMarkupParser *parser,
							   gpointer      *data);
static void     ctk_list_store_buildable_custom_tag_end (CtkBuildable *buildable,
							 CtkBuilder   *builder,
							 GObject      *child,
							 const gchar  *tagname,
							 gpointer     *data);

G_DEFINE_TYPE_WITH_CODE (CtkListStore, ctk_list_store, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (CtkListStore)
			 G_IMPLEMENT_INTERFACE (CTK_TYPE_TREE_MODEL,
						ctk_list_store_tree_model_init)
			 G_IMPLEMENT_INTERFACE (CTK_TYPE_TREE_DRAG_SOURCE,
						ctk_list_store_drag_source_init)
			 G_IMPLEMENT_INTERFACE (CTK_TYPE_TREE_DRAG_DEST,
						ctk_list_store_drag_dest_init)
			 G_IMPLEMENT_INTERFACE (CTK_TYPE_TREE_SORTABLE,
						ctk_list_store_sortable_init)
			 G_IMPLEMENT_INTERFACE (CTK_TYPE_BUILDABLE,
						ctk_list_store_buildable_init))


static void
ctk_list_store_class_init (CtkListStoreClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;

  object_class->finalize = ctk_list_store_finalize;
}

static void
ctk_list_store_tree_model_init (CtkTreeModelIface *iface)
{
  iface->get_flags = ctk_list_store_get_flags;
  iface->get_n_columns = ctk_list_store_get_n_columns;
  iface->get_column_type = ctk_list_store_get_column_type;
  iface->get_iter = ctk_list_store_get_iter;
  iface->get_path = ctk_list_store_get_path;
  iface->get_value = ctk_list_store_get_value;
  iface->iter_next = ctk_list_store_iter_next;
  iface->iter_previous = ctk_list_store_iter_previous;
  iface->iter_children = ctk_list_store_iter_children;
  iface->iter_has_child = ctk_list_store_iter_has_child;
  iface->iter_n_children = ctk_list_store_iter_n_children;
  iface->iter_nth_child = ctk_list_store_iter_nth_child;
  iface->iter_parent = ctk_list_store_iter_parent;
}

static void
ctk_list_store_drag_source_init (CtkTreeDragSourceIface *iface)
{
  iface->row_draggable = real_ctk_list_store_row_draggable;
  iface->drag_data_delete = ctk_list_store_drag_data_delete;
  iface->drag_data_get = ctk_list_store_drag_data_get;
}

static void
ctk_list_store_drag_dest_init (CtkTreeDragDestIface *iface)
{
  iface->drag_data_received = ctk_list_store_drag_data_received;
  iface->row_drop_possible = ctk_list_store_row_drop_possible;
}

static void
ctk_list_store_sortable_init (CtkTreeSortableIface *iface)
{
  iface->get_sort_column_id = ctk_list_store_get_sort_column_id;
  iface->set_sort_column_id = ctk_list_store_set_sort_column_id;
  iface->set_sort_func = ctk_list_store_set_sort_func;
  iface->set_default_sort_func = ctk_list_store_set_default_sort_func;
  iface->has_default_sort_func = ctk_list_store_has_default_sort_func;
}

void
ctk_list_store_buildable_init (CtkBuildableIface *iface)
{
  iface->custom_tag_start = ctk_list_store_buildable_custom_tag_start;
  iface->custom_tag_end = ctk_list_store_buildable_custom_tag_end;
}

static void
ctk_list_store_init (CtkListStore *list_store)
{
  CtkListStorePrivate *priv;

  list_store->priv = ctk_list_store_get_instance_private (list_store);
  priv = list_store->priv;

  priv->seq = g_sequence_new (NULL);
  priv->sort_list = NULL;
  priv->stamp = g_random_int ();
  priv->sort_column_id = CTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;
  priv->columns_dirty = FALSE;
  priv->length = 0;
}

static gboolean
iter_is_valid (CtkTreeIter  *iter,
               CtkListStore *list_store)
{
  return iter != NULL && 
         iter->user_data != NULL &&
         list_store->priv->stamp == iter->stamp &&
         !g_sequence_iter_is_end (iter->user_data) &&
         g_sequence_iter_get_sequence (iter->user_data) == list_store->priv->seq;
}

/**
 * ctk_list_store_new:
 * @n_columns: number of columns in the list store
 * @...: all #GType types for the columns, from first to last
 *
 * Creates a new list store as with @n_columns columns each of the types passed
 * in.  Note that only types derived from standard GObject fundamental types
 * are supported.
 *
 * As an example, `ctk_list_store_new (3, G_TYPE_INT, G_TYPE_STRING,
 * GDK_TYPE_PIXBUF);` will create a new #CtkListStore with three columns, of type
 * int, string and #GdkPixbuf respectively.
 *
 * Returns: a new #CtkListStore
 */
CtkListStore *
ctk_list_store_new (gint n_columns,
		    ...)
{
  CtkListStore *retval;
  va_list args;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = g_object_new (CTK_TYPE_LIST_STORE, NULL);
  ctk_list_store_set_n_columns (retval, n_columns);

  va_start (args, n_columns);

  for (i = 0; i < n_columns; i++)
    {
      GType type = va_arg (args, GType);
      if (! _ctk_tree_data_list_check_type (type))
        {
          g_warning ("%s: Invalid type %s", G_STRLOC, g_type_name (type));
          g_object_unref (retval);
          va_end (args);

          return NULL;
        }

      ctk_list_store_set_column_type (retval, i, type);
    }

  va_end (args);

  return retval;
}


/**
 * ctk_list_store_newv: (rename-to ctk_list_store_new)
 * @n_columns: number of columns in the list store
 * @types: (array length=n_columns): an array of #GType types for the columns, from first to last
 *
 * Non-vararg creation function.  Used primarily by language bindings.
 *
 * Returns: (transfer full): a new #CtkListStore
 **/
CtkListStore *
ctk_list_store_newv (gint   n_columns,
		     GType *types)
{
  CtkListStore *retval;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = g_object_new (CTK_TYPE_LIST_STORE, NULL);
  ctk_list_store_set_n_columns (retval, n_columns);

  for (i = 0; i < n_columns; i++)
    {
      if (! _ctk_tree_data_list_check_type (types[i]))
	{
	  g_warning ("%s: Invalid type %s", G_STRLOC, g_type_name (types[i]));
	  g_object_unref (retval);
	  return NULL;
	}

      ctk_list_store_set_column_type (retval, i, types[i]);
    }

  return retval;
}

/**
 * ctk_list_store_set_column_types:
 * @list_store: A #CtkListStore
 * @n_columns: Number of columns for the list store
 * @types: (array length=n_columns): An array length n of #GTypes
 *
 * This function is meant primarily for #GObjects that inherit from #CtkListStore,
 * and should only be used when constructing a new #CtkListStore.  It will not
 * function after a row has been added, or a method on the #CtkTreeModel
 * interface is called.
 **/
void
ctk_list_store_set_column_types (CtkListStore *list_store,
				 gint          n_columns,
				 GType        *types)
{
  CtkListStorePrivate *priv;
  gint i;

  g_return_if_fail (CTK_IS_LIST_STORE (list_store));

  priv = list_store->priv;

  g_return_if_fail (priv->columns_dirty == 0);

  ctk_list_store_set_n_columns (list_store, n_columns);
  for (i = 0; i < n_columns; i++)
    {
      if (! _ctk_tree_data_list_check_type (types[i]))
	{
	  g_warning ("%s: Invalid type %s", G_STRLOC, g_type_name (types[i]));
	  continue;
	}
      ctk_list_store_set_column_type (list_store, i, types[i]);
    }
}

static void
ctk_list_store_set_n_columns (CtkListStore *list_store,
			      gint          n_columns)
{
  CtkListStorePrivate *priv = list_store->priv;
  int i;

  if (priv->n_columns == n_columns)
    return;

  priv->column_headers = g_renew (GType, priv->column_headers, n_columns);
  for (i = priv->n_columns; i < n_columns; i++)
    priv->column_headers[i] = G_TYPE_INVALID;
  priv->n_columns = n_columns;

  if (priv->sort_list)
    _ctk_tree_data_list_header_free (priv->sort_list);
  priv->sort_list = _ctk_tree_data_list_header_new (n_columns, priv->column_headers);
}

static void
ctk_list_store_set_column_type (CtkListStore *list_store,
				gint          column,
				GType         type)
{
  CtkListStorePrivate *priv = list_store->priv;

  if (!_ctk_tree_data_list_check_type (type))
    {
      g_warning ("%s: Invalid type %s", G_STRLOC, g_type_name (type));
      return;
    }

  priv->column_headers[column] = type;
}

static void
ctk_list_store_finalize (GObject *object)
{
  CtkListStore *list_store = CTK_LIST_STORE (object);
  CtkListStorePrivate *priv = list_store->priv;

  g_sequence_foreach (priv->seq,
		      (GFunc) _ctk_tree_data_list_free, priv->column_headers);

  g_sequence_free (priv->seq);

  _ctk_tree_data_list_header_free (priv->sort_list);
  g_free (priv->column_headers);

  if (priv->default_sort_destroy)
    {
      GDestroyNotify d = priv->default_sort_destroy;

      priv->default_sort_destroy = NULL;
      d (priv->default_sort_data);
      priv->default_sort_data = NULL;
    }

  G_OBJECT_CLASS (ctk_list_store_parent_class)->finalize (object);
}

/* Fulfill the CtkTreeModel requirements */
static CtkTreeModelFlags
ctk_list_store_get_flags (CtkTreeModel *tree_model)
{
  return CTK_TREE_MODEL_ITERS_PERSIST | CTK_TREE_MODEL_LIST_ONLY;
}

static gint
ctk_list_store_get_n_columns (CtkTreeModel *tree_model)
{
  CtkListStore *list_store = CTK_LIST_STORE (tree_model);
  CtkListStorePrivate *priv = list_store->priv;

  priv->columns_dirty = TRUE;

  return priv->n_columns;
}

static GType
ctk_list_store_get_column_type (CtkTreeModel *tree_model,
				gint          index)
{
  CtkListStore *list_store = CTK_LIST_STORE (tree_model);
  CtkListStorePrivate *priv = list_store->priv;

  g_return_val_if_fail (index < priv->n_columns, G_TYPE_INVALID);

  priv->columns_dirty = TRUE;

  return priv->column_headers[index];
}

static gboolean
ctk_list_store_get_iter (CtkTreeModel *tree_model,
			 CtkTreeIter  *iter,
			 CtkTreePath  *path)
{
  CtkListStore *list_store = CTK_LIST_STORE (tree_model);
  CtkListStorePrivate *priv = list_store->priv;
  GSequence *seq;
  gint i;

  priv->columns_dirty = TRUE;

  seq = priv->seq;

  i = ctk_tree_path_get_indices (path)[0];

  if (i >= g_sequence_get_length (seq))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = priv->stamp;
  iter->user_data = g_sequence_get_iter_at_pos (seq, i);

  return TRUE;
}

static CtkTreePath *
ctk_list_store_get_path (CtkTreeModel *tree_model,
			 CtkTreeIter  *iter)
{
  CtkListStore *list_store = CTK_LIST_STORE (tree_model);
  CtkListStorePrivate *priv = list_store->priv;
  CtkTreePath *path;

  g_return_val_if_fail (iter->stamp == priv->stamp, NULL);

  if (g_sequence_iter_is_end (iter->user_data))
    return NULL;
	
  path = ctk_tree_path_new ();
  ctk_tree_path_append_index (path, g_sequence_iter_get_position (iter->user_data));
  
  return path;
}

static void
ctk_list_store_get_value (CtkTreeModel *tree_model,
			  CtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  CtkListStore *list_store = CTK_LIST_STORE (tree_model);
  CtkListStorePrivate *priv = list_store->priv;
  CtkTreeDataList *list;
  gint tmp_column = column;

  g_return_if_fail (column < priv->n_columns);
  g_return_if_fail (iter_is_valid (iter, list_store));
		    
  list = g_sequence_get (iter->user_data);

  while (tmp_column-- > 0 && list)
    list = list->next;

  if (list == NULL)
    g_value_init (value, priv->column_headers[column]);
  else
    _ctk_tree_data_list_node_to_value (list,
				       priv->column_headers[column],
				       value);
}

static gboolean
ctk_list_store_iter_next (CtkTreeModel  *tree_model,
			  CtkTreeIter   *iter)
{
  CtkListStore *list_store = CTK_LIST_STORE (tree_model);
  CtkListStorePrivate *priv = list_store->priv;
  gboolean retval;

  g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
  iter->user_data = g_sequence_iter_next (iter->user_data);

  retval = g_sequence_iter_is_end (iter->user_data);
  if (retval)
    iter->stamp = 0;

  return !retval;
}

static gboolean
ctk_list_store_iter_previous (CtkTreeModel *tree_model,
                              CtkTreeIter  *iter)
{
  CtkListStore *list_store = CTK_LIST_STORE (tree_model);
  CtkListStorePrivate *priv = list_store->priv;

  g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);

  if (g_sequence_iter_is_begin (iter->user_data))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->user_data = g_sequence_iter_prev (iter->user_data);

  return TRUE;
}

static gboolean
ctk_list_store_iter_children (CtkTreeModel *tree_model,
			      CtkTreeIter  *iter,
			      CtkTreeIter  *parent)
{
  CtkListStore *list_store = (CtkListStore *) tree_model;
  CtkListStorePrivate *priv = list_store->priv;

  /* this is a list, nodes have no children */
  if (parent)
    {
      iter->stamp = 0;
      return FALSE;
    }

  if (g_sequence_get_length (priv->seq) > 0)
    {
      iter->stamp = priv->stamp;
      iter->user_data = g_sequence_get_begin_iter (priv->seq);
      return TRUE;
    }
  else
    {
      iter->stamp = 0;
      return FALSE;
    }
}

static gboolean
ctk_list_store_iter_has_child (CtkTreeModel *tree_model,
			       CtkTreeIter  *iter)
{
  return FALSE;
}

static gint
ctk_list_store_iter_n_children (CtkTreeModel *tree_model,
				CtkTreeIter  *iter)
{
  CtkListStore *list_store = CTK_LIST_STORE (tree_model);
  CtkListStorePrivate *priv = list_store->priv;

  if (iter == NULL)
    return g_sequence_get_length (priv->seq);

  g_return_val_if_fail (priv->stamp == iter->stamp, -1);

  return 0;
}

static gboolean
ctk_list_store_iter_nth_child (CtkTreeModel *tree_model,
			       CtkTreeIter  *iter,
			       CtkTreeIter  *parent,
			       gint          n)
{
  CtkListStore *list_store = CTK_LIST_STORE (tree_model);
  CtkListStorePrivate *priv = list_store->priv;
  GSequenceIter *child;

  iter->stamp = 0;

  if (parent)
    return FALSE;

  child = g_sequence_get_iter_at_pos (priv->seq, n);

  if (g_sequence_iter_is_end (child))
    return FALSE;

  iter->stamp = priv->stamp;
  iter->user_data = child;

  return TRUE;
}

static gboolean
ctk_list_store_iter_parent (CtkTreeModel *tree_model,
			    CtkTreeIter  *iter,
			    CtkTreeIter  *child)
{
  iter->stamp = 0;
  return FALSE;
}

static gboolean
ctk_list_store_real_set_value (CtkListStore *list_store,
			       CtkTreeIter  *iter,
			       gint          column,
			       GValue       *value,
			       gboolean      sort)
{
  CtkListStorePrivate *priv = list_store->priv;
  CtkTreeDataList *list;
  CtkTreeDataList *prev;
  gint old_column = column;
  GValue real_value = G_VALUE_INIT;
  gboolean converted = FALSE;
  gboolean retval = FALSE;

  if (! g_type_is_a (G_VALUE_TYPE (value), priv->column_headers[column]))
    {
      if (! (g_value_type_transformable (G_VALUE_TYPE (value), priv->column_headers[column])))
	{
	  g_warning ("%s: Unable to convert from %s to %s",
		     G_STRLOC,
		     g_type_name (G_VALUE_TYPE (value)),
		     g_type_name (priv->column_headers[column]));
	  return retval;
	}

      g_value_init (&real_value, priv->column_headers[column]);
      if (!g_value_transform (value, &real_value))
	{
	  g_warning ("%s: Unable to make conversion from %s to %s",
		     G_STRLOC,
		     g_type_name (G_VALUE_TYPE (value)),
		     g_type_name (priv->column_headers[column]));
	  g_value_unset (&real_value);
	  return retval;
	}
      converted = TRUE;
    }

  prev = list = g_sequence_get (iter->user_data);

  while (list != NULL)
    {
      if (column == 0)
	{
	  if (converted)
	    _ctk_tree_data_list_value_to_node (list, &real_value);
	  else
	    _ctk_tree_data_list_value_to_node (list, value);
	  retval = TRUE;
	  if (converted)
	    g_value_unset (&real_value);
         if (sort && CTK_LIST_STORE_IS_SORTED (list_store))
            ctk_list_store_sort_iter_changed (list_store, iter, old_column);
	  return retval;
	}

      column--;
      prev = list;
      list = list->next;
    }

  if (g_sequence_get (iter->user_data) == NULL)
    {
      list = _ctk_tree_data_list_alloc();
      g_sequence_set (iter->user_data, list);
      list->next = NULL;
    }
  else
    {
      list = prev->next = _ctk_tree_data_list_alloc ();
      list->next = NULL;
    }

  while (column != 0)
    {
      list->next = _ctk_tree_data_list_alloc ();
      list = list->next;
      list->next = NULL;
      column --;
    }

  if (converted)
    _ctk_tree_data_list_value_to_node (list, &real_value);
  else
    _ctk_tree_data_list_value_to_node (list, value);

  retval = TRUE;
  if (converted)
    g_value_unset (&real_value);

  if (sort && CTK_LIST_STORE_IS_SORTED (list_store))
    ctk_list_store_sort_iter_changed (list_store, iter, old_column);

  return retval;
}


/**
 * ctk_list_store_set_value:
 * @list_store: A #CtkListStore
 * @iter: A valid #CtkTreeIter for the row being modified
 * @column: column number to modify
 * @value: new value for the cell
 *
 * Sets the data in the cell specified by @iter and @column.
 * The type of @value must be convertible to the type of the
 * column.
 *
 **/
void
ctk_list_store_set_value (CtkListStore *list_store,
			  CtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  CtkListStorePrivate *priv;

  g_return_if_fail (CTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter_is_valid (iter, list_store));
  g_return_if_fail (G_IS_VALUE (value));
  priv = list_store->priv;
  g_return_if_fail (column >= 0 && column < priv->n_columns);

  if (ctk_list_store_real_set_value (list_store, iter, column, value, TRUE))
    {
      CtkTreePath *path;

      path = ctk_list_store_get_path (CTK_TREE_MODEL (list_store), iter);
      ctk_tree_model_row_changed (CTK_TREE_MODEL (list_store), path, iter);
      ctk_tree_path_free (path);
    }
}

static CtkTreeIterCompareFunc
ctk_list_store_get_compare_func (CtkListStore *list_store)
{
  CtkListStorePrivate *priv = list_store->priv;
  CtkTreeIterCompareFunc func = NULL;

  if (CTK_LIST_STORE_IS_SORTED (list_store))
    {
      if (priv->sort_column_id != -1)
	{
	  CtkTreeDataSortHeader *header;
	  header = _ctk_tree_data_list_get_header (priv->sort_list,
						   priv->sort_column_id);
	  g_return_val_if_fail (header != NULL, NULL);
	  g_return_val_if_fail (header->func != NULL, NULL);
	  func = header->func;
	}
      else
	{
	  func = priv->default_sort_func;
	}
    }

  return func;
}

static void
ctk_list_store_set_vector_internal (CtkListStore *list_store,
				    CtkTreeIter  *iter,
				    gboolean     *emit_signal,
				    gboolean     *maybe_need_sort,
				    gint         *columns,
				    GValue       *values,
				    gint          n_values)
{
  CtkListStorePrivate *priv = list_store->priv;
  gint i;
  CtkTreeIterCompareFunc func = NULL;

  func = ctk_list_store_get_compare_func (list_store);
  if (func != _ctk_tree_data_list_compare_func)
    *maybe_need_sort = TRUE;

  for (i = 0; i < n_values; i++)
    {
      *emit_signal = ctk_list_store_real_set_value (list_store, 
					       iter, 
					       columns[i],
					       &values[i],
					       FALSE) || *emit_signal;

      if (func == _ctk_tree_data_list_compare_func &&
	  columns[i] == priv->sort_column_id)
	*maybe_need_sort = TRUE;
    }
}

static void
ctk_list_store_set_valist_internal (CtkListStore *list_store,
				    CtkTreeIter  *iter,
				    gboolean     *emit_signal,
				    gboolean     *maybe_need_sort,
				    va_list	  var_args)
{
  CtkListStorePrivate *priv = list_store->priv;
  gint column;
  CtkTreeIterCompareFunc func = NULL;

  column = va_arg (var_args, gint);

  func = ctk_list_store_get_compare_func (list_store);
  if (func != _ctk_tree_data_list_compare_func)
    *maybe_need_sort = TRUE;

  while (column != -1)
    {
      GValue value = G_VALUE_INIT;
      gchar *error = NULL;

      if (column < 0 || column >= priv->n_columns)
	{
	  g_warning ("%s: Invalid column number %d added to iter (remember to end your list of columns with a -1)", G_STRLOC, column);
	  break;
	}

      G_VALUE_COLLECT_INIT (&value, priv->column_headers[column],
                            var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

 	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  break;
	}

      /* FIXME: instead of calling this n times, refactor with above */
      *emit_signal = ctk_list_store_real_set_value (list_store,
						    iter,
						    column,
						    &value,
						    FALSE) || *emit_signal;
      
      if (func == _ctk_tree_data_list_compare_func &&
	  column == priv->sort_column_id)
	*maybe_need_sort = TRUE;

      g_value_unset (&value);

      column = va_arg (var_args, gint);
    }
}

/**
 * ctk_list_store_set_valuesv: (rename-to ctk_list_store_set)
 * @list_store: A #CtkListStore
 * @iter: A valid #CtkTreeIter for the row being modified
 * @columns: (array length=n_values): an array of column numbers
 * @values: (array length=n_values): an array of GValues
 * @n_values: the length of the @columns and @values arrays
 *
 * A variant of ctk_list_store_set_valist() which
 * takes the columns and values as two arrays, instead of
 * varargs. This function is mainly intended for 
 * language-bindings and in case the number of columns to
 * change is not known until run-time.
 *
 * Since: 2.12
 */
void
ctk_list_store_set_valuesv (CtkListStore *list_store,
			    CtkTreeIter  *iter,
			    gint         *columns,
			    GValue       *values,
			    gint          n_values)
{
  CtkListStorePrivate *priv;
  gboolean emit_signal = FALSE;
  gboolean maybe_need_sort = FALSE;

  g_return_if_fail (CTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter_is_valid (iter, list_store));

  priv = list_store->priv;

  ctk_list_store_set_vector_internal (list_store, iter,
				      &emit_signal,
				      &maybe_need_sort,
				      columns, values, n_values);

  if (maybe_need_sort && CTK_LIST_STORE_IS_SORTED (list_store))
    ctk_list_store_sort_iter_changed (list_store, iter, priv->sort_column_id);

  if (emit_signal)
    {
      CtkTreePath *path;

      path = ctk_list_store_get_path (CTK_TREE_MODEL (list_store), iter);
      ctk_tree_model_row_changed (CTK_TREE_MODEL (list_store), path, iter);
      ctk_tree_path_free (path);
    }
}

/**
 * ctk_list_store_set_valist:
 * @list_store: A #CtkListStore
 * @iter: A valid #CtkTreeIter for the row being modified
 * @var_args: va_list of column/value pairs
 *
 * See ctk_list_store_set(); this version takes a va_list for use by language
 * bindings.
 *
 **/
void
ctk_list_store_set_valist (CtkListStore *list_store,
                           CtkTreeIter  *iter,
                           va_list	 var_args)
{
  CtkListStorePrivate *priv;
  gboolean emit_signal = FALSE;
  gboolean maybe_need_sort = FALSE;

  g_return_if_fail (CTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter_is_valid (iter, list_store));

  priv = list_store->priv;

  ctk_list_store_set_valist_internal (list_store, iter, 
				      &emit_signal, 
				      &maybe_need_sort,
				      var_args);

  if (maybe_need_sort && CTK_LIST_STORE_IS_SORTED (list_store))
    ctk_list_store_sort_iter_changed (list_store, iter, priv->sort_column_id);

  if (emit_signal)
    {
      CtkTreePath *path;

      path = ctk_list_store_get_path (CTK_TREE_MODEL (list_store), iter);
      ctk_tree_model_row_changed (CTK_TREE_MODEL (list_store), path, iter);
      ctk_tree_path_free (path);
    }
}

/**
 * ctk_list_store_set:
 * @list_store: a #CtkListStore
 * @iter: row iterator
 * @...: pairs of column number and value, terminated with -1
 *
 * Sets the value of one or more cells in the row referenced by @iter.
 * The variable argument list should contain integer column numbers,
 * each column number followed by the value to be set.
 * The list is terminated by a -1. For example, to set column 0 with type
 * %G_TYPE_STRING to “Foo”, you would write `ctk_list_store_set (store, iter,
 * 0, "Foo", -1)`.
 *
 * The value will be referenced by the store if it is a %G_TYPE_OBJECT, and it
 * will be copied if it is a %G_TYPE_STRING or %G_TYPE_BOXED.
 */
void
ctk_list_store_set (CtkListStore *list_store,
		    CtkTreeIter  *iter,
		    ...)
{
  va_list var_args;

  va_start (var_args, iter);
  ctk_list_store_set_valist (list_store, iter, var_args);
  va_end (var_args);
}

/**
 * ctk_list_store_remove:
 * @list_store: A #CtkListStore
 * @iter: A valid #CtkTreeIter
 *
 * Removes the given row from the list store.  After being removed, 
 * @iter is set to be the next valid row, or invalidated if it pointed 
 * to the last row in @list_store.
 *
 * Returns: %TRUE if @iter is valid, %FALSE if not.
 **/
gboolean
ctk_list_store_remove (CtkListStore *list_store,
		       CtkTreeIter  *iter)
{
  CtkListStorePrivate *priv;
  CtkTreePath *path;
  GSequenceIter *ptr, *next;

  g_return_val_if_fail (CTK_IS_LIST_STORE (list_store), FALSE);
  g_return_val_if_fail (iter_is_valid (iter, list_store), FALSE);

  priv = list_store->priv;

  path = ctk_list_store_get_path (CTK_TREE_MODEL (list_store), iter);

  ptr = iter->user_data;
  next = g_sequence_iter_next (ptr);
  
  _ctk_tree_data_list_free (g_sequence_get (ptr), priv->column_headers);
  g_sequence_remove (iter->user_data);

  priv->length--;
  
  ctk_tree_model_row_deleted (CTK_TREE_MODEL (list_store), path);
  ctk_tree_path_free (path);

  if (g_sequence_iter_is_end (next))
    {
      iter->stamp = 0;
      return FALSE;
    }
  else
    {
      iter->stamp = priv->stamp;
      iter->user_data = next;
      return TRUE;
    }
}

/**
 * ctk_list_store_insert:
 * @list_store: A #CtkListStore
 * @iter: (out): An unset #CtkTreeIter to set to the new row
 * @position: position to insert the new row, or -1 for last
 *
 * Creates a new row at @position.  @iter will be changed to point to this new
 * row.  If @position is -1 or is larger than the number of rows on the list,
 * then the new row will be appended to the list. The row will be empty after
 * this function is called.  To fill in values, you need to call
 * ctk_list_store_set() or ctk_list_store_set_value().
 *
 **/
void
ctk_list_store_insert (CtkListStore *list_store,
		       CtkTreeIter  *iter,
		       gint          position)
{
  CtkListStorePrivate *priv;
  CtkTreePath *path;
  GSequence *seq;
  GSequenceIter *ptr;
  gint length;

  g_return_if_fail (CTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  priv = list_store->priv;

  priv->columns_dirty = TRUE;

  seq = priv->seq;

  length = g_sequence_get_length (seq);
  if (position > length || position < 0)
    position = length;

  ptr = g_sequence_get_iter_at_pos (seq, position);
  ptr = g_sequence_insert_before (ptr, NULL);

  iter->stamp = priv->stamp;
  iter->user_data = ptr;

  g_assert (iter_is_valid (iter, list_store));

  priv->length++;
  
  path = ctk_tree_path_new ();
  ctk_tree_path_append_index (path, position);
  ctk_tree_model_row_inserted (CTK_TREE_MODEL (list_store), path, iter);
  ctk_tree_path_free (path);
}

/**
 * ctk_list_store_insert_before:
 * @list_store: A #CtkListStore
 * @iter: (out): An unset #CtkTreeIter to set to the new row
 * @sibling: (allow-none): A valid #CtkTreeIter, or %NULL
 *
 * Inserts a new row before @sibling. If @sibling is %NULL, then the row will 
 * be appended to the end of the list. @iter will be changed to point to this 
 * new row. The row will be empty after this function is called. To fill in 
 * values, you need to call ctk_list_store_set() or ctk_list_store_set_value().
 *
 **/
void
ctk_list_store_insert_before (CtkListStore *list_store,
			      CtkTreeIter  *iter,
			      CtkTreeIter  *sibling)
{
  CtkListStorePrivate *priv;
  GSequenceIter *after;
  
  g_return_if_fail (CTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  priv = list_store->priv;

  if (sibling)
    g_return_if_fail (iter_is_valid (sibling, list_store));

  if (!sibling)
    after = g_sequence_get_end_iter (priv->seq);
  else
    after = sibling->user_data;

  ctk_list_store_insert (list_store, iter, g_sequence_iter_get_position (after));
}

/**
 * ctk_list_store_insert_after:
 * @list_store: A #CtkListStore
 * @iter: (out): An unset #CtkTreeIter to set to the new row
 * @sibling: (allow-none): A valid #CtkTreeIter, or %NULL
 *
 * Inserts a new row after @sibling. If @sibling is %NULL, then the row will be
 * prepended to the beginning of the list. @iter will be changed to point to
 * this new row. The row will be empty after this function is called. To fill
 * in values, you need to call ctk_list_store_set() or ctk_list_store_set_value().
 *
 **/
void
ctk_list_store_insert_after (CtkListStore *list_store,
			     CtkTreeIter  *iter,
			     CtkTreeIter  *sibling)
{
  CtkListStorePrivate *priv;
  GSequenceIter *after;

  g_return_if_fail (CTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  priv = list_store->priv;

  if (sibling)
    g_return_if_fail (iter_is_valid (sibling, list_store));

  if (!sibling)
    after = g_sequence_get_begin_iter (priv->seq);
  else
    after = g_sequence_iter_next (sibling->user_data);

  ctk_list_store_insert (list_store, iter, g_sequence_iter_get_position (after));
}

/**
 * ctk_list_store_prepend:
 * @list_store: A #CtkListStore
 * @iter: (out): An unset #CtkTreeIter to set to the prepend row
 *
 * Prepends a new row to @list_store. @iter will be changed to point to this new
 * row. The row will be empty after this function is called. To fill in
 * values, you need to call ctk_list_store_set() or ctk_list_store_set_value().
 *
 **/
void
ctk_list_store_prepend (CtkListStore *list_store,
			CtkTreeIter  *iter)
{
  g_return_if_fail (CTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  ctk_list_store_insert (list_store, iter, 0);
}

/**
 * ctk_list_store_append:
 * @list_store: A #CtkListStore
 * @iter: (out): An unset #CtkTreeIter to set to the appended row
 *
 * Appends a new row to @list_store.  @iter will be changed to point to this new
 * row.  The row will be empty after this function is called.  To fill in
 * values, you need to call ctk_list_store_set() or ctk_list_store_set_value().
 *
 **/
void
ctk_list_store_append (CtkListStore *list_store,
		       CtkTreeIter  *iter)
{
  g_return_if_fail (CTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  ctk_list_store_insert (list_store, iter, -1);
}

static void
ctk_list_store_increment_stamp (CtkListStore *list_store)
{
  CtkListStorePrivate *priv = list_store->priv;

  do
    {
      priv->stamp++;
    }
  while (priv->stamp == 0);
}

/**
 * ctk_list_store_clear:
 * @list_store: a #CtkListStore.
 *
 * Removes all rows from the list store.  
 *
 **/
void
ctk_list_store_clear (CtkListStore *list_store)
{
  CtkListStorePrivate *priv;
  CtkTreeIter iter;

  g_return_if_fail (CTK_IS_LIST_STORE (list_store));

  priv = list_store->priv;

  while (g_sequence_get_length (priv->seq) > 0)
    {
      iter.stamp = priv->stamp;
      iter.user_data = g_sequence_get_begin_iter (priv->seq);
      ctk_list_store_remove (list_store, &iter);
    }

  ctk_list_store_increment_stamp (list_store);
}

/**
 * ctk_list_store_iter_is_valid:
 * @list_store: A #CtkListStore.
 * @iter: A #CtkTreeIter.
 *
 * > This function is slow. Only use it for debugging and/or testing
 * > purposes.
 *
 * Checks if the given iter is a valid iter for this #CtkListStore.
 *
 * Returns: %TRUE if the iter is valid, %FALSE if the iter is invalid.
 *
 * Since: 2.2
 **/
gboolean
ctk_list_store_iter_is_valid (CtkListStore *list_store,
                              CtkTreeIter  *iter)
{
  g_return_val_if_fail (CTK_IS_LIST_STORE (list_store), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  return iter_is_valid (iter, list_store);
}

static gboolean real_ctk_list_store_row_draggable (CtkTreeDragSource *drag_source,
                                                   CtkTreePath       *path)
{
  return TRUE;
}
  
static gboolean
ctk_list_store_drag_data_delete (CtkTreeDragSource *drag_source,
                                 CtkTreePath       *path)
{
  CtkTreeIter iter;

  if (ctk_list_store_get_iter (CTK_TREE_MODEL (drag_source),
                               &iter,
                               path))
    {
      ctk_list_store_remove (CTK_LIST_STORE (drag_source), &iter);
      return TRUE;
    }
  return FALSE;
}

static gboolean
ctk_list_store_drag_data_get (CtkTreeDragSource *drag_source,
                              CtkTreePath       *path,
                              CtkSelectionData  *selection_data)
{
  /* Note that we don't need to handle the CTK_TREE_MODEL_ROW
   * target, because the default handler does it for us, but
   * we do anyway for the convenience of someone maybe overriding the
   * default handler.
   */

  if (ctk_tree_set_row_drag_data (selection_data,
				  CTK_TREE_MODEL (drag_source),
				  path))
    {
      return TRUE;
    }
  else
    {
      /* FIXME handle text targets at least. */
    }

  return FALSE;
}

static gboolean
ctk_list_store_drag_data_received (CtkTreeDragDest   *drag_dest,
                                   CtkTreePath       *dest,
                                   CtkSelectionData  *selection_data)
{
  CtkTreeModel *tree_model = CTK_TREE_MODEL (drag_dest);
  CtkListStore *list_store = CTK_LIST_STORE (tree_model);
  CtkListStorePrivate *priv = list_store->priv;
  CtkTreeModel *src_model = NULL;
  CtkTreePath *src_path = NULL;
  gboolean retval = FALSE;

  if (ctk_tree_get_row_drag_data (selection_data,
				  &src_model,
				  &src_path) &&
      src_model == tree_model)
    {
      /* Copy the given row to a new position */
      CtkTreeIter src_iter;
      CtkTreeIter dest_iter;
      CtkTreePath *prev;

      if (!ctk_list_store_get_iter (src_model,
                                    &src_iter,
                                    src_path))
        {
          goto out;
        }

      /* Get the path to insert _after_ (dest is the path to insert _before_) */
      prev = ctk_tree_path_copy (dest);

      if (!ctk_tree_path_prev (prev))
        {
          /* dest was the first spot in the list; which means we are supposed
           * to prepend.
           */
          ctk_list_store_prepend (list_store, &dest_iter);

          retval = TRUE;
        }
      else
        {
          if (ctk_list_store_get_iter (tree_model, &dest_iter, prev))
            {
              CtkTreeIter tmp_iter = dest_iter;

              ctk_list_store_insert_after (list_store, &dest_iter, &tmp_iter);

              retval = TRUE;
            }
        }

      ctk_tree_path_free (prev);

      /* If we succeeded in creating dest_iter, copy data from src
       */
      if (retval)
        {
          CtkTreeDataList *dl = g_sequence_get (src_iter.user_data);
          CtkTreeDataList *copy_head = NULL;
          CtkTreeDataList *copy_prev = NULL;
          CtkTreeDataList *copy_iter = NULL;
	  CtkTreePath *path;
          gint col;

          col = 0;
          while (dl)
            {
              copy_iter = _ctk_tree_data_list_node_copy (dl,
                                                         priv->column_headers[col]);

              if (copy_head == NULL)
                copy_head = copy_iter;

              if (copy_prev)
                copy_prev->next = copy_iter;

              copy_prev = copy_iter;

              dl = dl->next;
              ++col;
            }

	  dest_iter.stamp = priv->stamp;
          g_sequence_set (dest_iter.user_data, copy_head);

	  path = ctk_list_store_get_path (tree_model, &dest_iter);
	  ctk_tree_model_row_changed (tree_model, path, &dest_iter);
	  ctk_tree_path_free (path);
	}
    }
  else
    {
      /* FIXME maybe add some data targets eventually, or handle text
       * targets in the simple case.
       */
    }

 out:

  if (src_path)
    ctk_tree_path_free (src_path);

  return retval;
}

static gboolean
ctk_list_store_row_drop_possible (CtkTreeDragDest  *drag_dest,
                                  CtkTreePath      *dest_path,
				  CtkSelectionData *selection_data)
{
  gint *indices;
  CtkTreeModel *src_model = NULL;
  CtkTreePath *src_path = NULL;
  gboolean retval = FALSE;

  /* don't accept drops if the list has been sorted */
  if (CTK_LIST_STORE_IS_SORTED (drag_dest))
    return FALSE;

  if (!ctk_tree_get_row_drag_data (selection_data,
				   &src_model,
				   &src_path))
    goto out;

  if (src_model != CTK_TREE_MODEL (drag_dest))
    goto out;

  if (ctk_tree_path_get_depth (dest_path) != 1)
    goto out;

  /* can drop before any existing node, or before one past any existing. */

  indices = ctk_tree_path_get_indices (dest_path);

  if (indices[0] <= g_sequence_get_length (CTK_LIST_STORE (drag_dest)->priv->seq))
    retval = TRUE;

 out:
  if (src_path)
    ctk_tree_path_free (src_path);
  
  return retval;
}

/* Sorting and reordering */

/* Reordering */
static gint
ctk_list_store_reorder_func (GSequenceIter *a,
			     GSequenceIter *b,
			     gpointer       user_data)
{
  GHashTable *new_positions = user_data;
  gint apos = GPOINTER_TO_INT (g_hash_table_lookup (new_positions, a));
  gint bpos = GPOINTER_TO_INT (g_hash_table_lookup (new_positions, b));

  if (apos < bpos)
    return -1;
  if (apos > bpos)
    return 1;
  return 0;
}
  
/**
 * ctk_list_store_reorder:
 * @store: A #CtkListStore.
 * @new_order: (array zero-terminated=1): an array of integers mapping the new
 *      position of each child to its old position before the re-ordering,
 *      i.e. @new_order`[newpos] = oldpos`. It must have
 *      exactly as many items as the list store’s length.
 *
 * Reorders @store to follow the order indicated by @new_order. Note that
 * this function only works with unsorted stores.
 *
 * Since: 2.2
 **/
void
ctk_list_store_reorder (CtkListStore *store,
			gint         *new_order)
{
  CtkListStorePrivate *priv;
  gint i;
  CtkTreePath *path;
  GHashTable *new_positions;
  GSequenceIter *ptr;
  gint *order;
  
  g_return_if_fail (CTK_IS_LIST_STORE (store));
  g_return_if_fail (!CTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (new_order != NULL);

  priv = store->priv;

  order = g_new (gint, g_sequence_get_length (priv->seq));
  for (i = 0; i < g_sequence_get_length (priv->seq); i++)
    order[new_order[i]] = i;
  
  new_positions = g_hash_table_new (g_direct_hash, g_direct_equal);

  ptr = g_sequence_get_begin_iter (priv->seq);
  i = 0;
  while (!g_sequence_iter_is_end (ptr))
    {
      g_hash_table_insert (new_positions, ptr, GINT_TO_POINTER (order[i++]));

      ptr = g_sequence_iter_next (ptr);
    }
  g_free (order);
  
  g_sequence_sort_iter (priv->seq, ctk_list_store_reorder_func, new_positions);

  g_hash_table_destroy (new_positions);
  
  /* emit signal */
  path = ctk_tree_path_new ();
  ctk_tree_model_rows_reordered (CTK_TREE_MODEL (store),
				 path, NULL, new_order);
  ctk_tree_path_free (path);
}

static GHashTable *
save_positions (GSequence *seq)
{
  GHashTable *positions = g_hash_table_new (g_direct_hash, g_direct_equal);
  GSequenceIter *ptr;

  ptr = g_sequence_get_begin_iter (seq);
  while (!g_sequence_iter_is_end (ptr))
    {
      g_hash_table_insert (positions, ptr,
			   GINT_TO_POINTER (g_sequence_iter_get_position (ptr)));
      ptr = g_sequence_iter_next (ptr);
    }

  return positions;
}

static int *
generate_order (GSequence *seq,
		GHashTable *old_positions)
{
  GSequenceIter *ptr;
  int *order = g_new (int, g_sequence_get_length (seq));
  int i;

  i = 0;
  ptr = g_sequence_get_begin_iter (seq);
  while (!g_sequence_iter_is_end (ptr))
    {
      int old_pos = GPOINTER_TO_INT (g_hash_table_lookup (old_positions, ptr));
      order[i++] = old_pos;
      ptr = g_sequence_iter_next (ptr);
    }

  g_hash_table_destroy (old_positions);

  return order;
}

/**
 * ctk_list_store_swap:
 * @store: A #CtkListStore.
 * @a: A #CtkTreeIter.
 * @b: Another #CtkTreeIter.
 *
 * Swaps @a and @b in @store. Note that this function only works with
 * unsorted stores.
 *
 * Since: 2.2
 **/
void
ctk_list_store_swap (CtkListStore *store,
		     CtkTreeIter  *a,
		     CtkTreeIter  *b)
{
  CtkListStorePrivate *priv;
  GHashTable *old_positions;
  gint *order;
  CtkTreePath *path;

  g_return_if_fail (CTK_IS_LIST_STORE (store));
  g_return_if_fail (!CTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (iter_is_valid (a, store));
  g_return_if_fail (iter_is_valid (b, store));

  priv = store->priv;

  if (a->user_data == b->user_data)
    return;

  old_positions = save_positions (priv->seq);
  
  g_sequence_swap (a->user_data, b->user_data);

  order = generate_order (priv->seq, old_positions);
  path = ctk_tree_path_new ();
  
  ctk_tree_model_rows_reordered (CTK_TREE_MODEL (store),
				 path, NULL, order);

  ctk_tree_path_free (path);
  g_free (order);
}

static void
ctk_list_store_move_to (CtkListStore *store,
			CtkTreeIter  *iter,
			gint	      new_pos)
{
  CtkListStorePrivate *priv = store->priv;
  GHashTable *old_positions;
  CtkTreePath *path;
  gint *order;

  old_positions = save_positions (priv->seq);

  g_sequence_move (iter->user_data, g_sequence_get_iter_at_pos (priv->seq, new_pos));

  order = generate_order (priv->seq, old_positions);

  path = ctk_tree_path_new ();
  ctk_tree_model_rows_reordered (CTK_TREE_MODEL (store),
				 path, NULL, order);
  ctk_tree_path_free (path);
  g_free (order);
}

/**
 * ctk_list_store_move_before:
 * @store: A #CtkListStore.
 * @iter: A #CtkTreeIter.
 * @position: (allow-none): A #CtkTreeIter, or %NULL.
 *
 * Moves @iter in @store to the position before @position. Note that this
 * function only works with unsorted stores. If @position is %NULL, @iter
 * will be moved to the end of the list.
 *
 * Since: 2.2
 **/
void
ctk_list_store_move_before (CtkListStore *store,
                            CtkTreeIter  *iter,
			    CtkTreeIter  *position)
{
  gint pos;
  
  g_return_if_fail (CTK_IS_LIST_STORE (store));
  g_return_if_fail (!CTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (iter_is_valid (iter, store));
  if (position)
    g_return_if_fail (iter_is_valid (position, store));

  if (position)
    pos = g_sequence_iter_get_position (position->user_data);
  else
    pos = -1;
  
  ctk_list_store_move_to (store, iter, pos);
}

/**
 * ctk_list_store_move_after:
 * @store: A #CtkListStore.
 * @iter: A #CtkTreeIter.
 * @position: (allow-none): A #CtkTreeIter or %NULL.
 *
 * Moves @iter in @store to the position after @position. Note that this
 * function only works with unsorted stores. If @position is %NULL, @iter
 * will be moved to the start of the list.
 *
 * Since: 2.2
 **/
void
ctk_list_store_move_after (CtkListStore *store,
			   CtkTreeIter  *iter,
			   CtkTreeIter  *position)
{
  gint pos;
  
  g_return_if_fail (CTK_IS_LIST_STORE (store));
  g_return_if_fail (!CTK_LIST_STORE_IS_SORTED (store));
  g_return_if_fail (iter_is_valid (iter, store));
  if (position)
    g_return_if_fail (iter_is_valid (position, store));

  if (position)
    pos = g_sequence_iter_get_position (position->user_data) + 1;
  else
    pos = 0;
  
  ctk_list_store_move_to (store, iter, pos);
}
    
/* Sorting */
static gint
ctk_list_store_compare_func (GSequenceIter *a,
			     GSequenceIter *b,
			     gpointer      user_data)
{
  CtkListStore *list_store = user_data;
  CtkListStorePrivate *priv = list_store->priv;
  CtkTreeIter iter_a;
  CtkTreeIter iter_b;
  gint retval;
  CtkTreeIterCompareFunc func;
  gpointer data;

  if (priv->sort_column_id != -1)
    {
      CtkTreeDataSortHeader *header;

      header = _ctk_tree_data_list_get_header (priv->sort_list,
					       priv->sort_column_id);
      g_return_val_if_fail (header != NULL, 0);
      g_return_val_if_fail (header->func != NULL, 0);

      func = header->func;
      data = header->data;
    }
  else
    {
      g_return_val_if_fail (priv->default_sort_func != NULL, 0);
      func = priv->default_sort_func;
      data = priv->default_sort_data;
    }

  iter_a.stamp = priv->stamp;
  iter_a.user_data = (gpointer)a;
  iter_b.stamp = priv->stamp;
  iter_b.user_data = (gpointer)b;

  g_assert (iter_is_valid (&iter_a, list_store));
  g_assert (iter_is_valid (&iter_b, list_store));

  retval = (* func) (CTK_TREE_MODEL (list_store), &iter_a, &iter_b, data);

  if (priv->order == CTK_SORT_DESCENDING)
    {
      if (retval > 0)
        retval = -1;
      else if (retval < 0)
        retval = 1;
    }

  return retval;
}

static void
ctk_list_store_sort (CtkListStore *list_store)
{
  CtkListStorePrivate *priv = list_store->priv;
  gint *new_order;
  CtkTreePath *path;
  GHashTable *old_positions;

  if (!CTK_LIST_STORE_IS_SORTED (list_store) ||
      g_sequence_get_length (priv->seq) <= 1)
    return;

  old_positions = save_positions (priv->seq);

  g_sequence_sort_iter (priv->seq, ctk_list_store_compare_func, list_store);

  /* Let the world know about our new order */
  new_order = generate_order (priv->seq, old_positions);

  path = ctk_tree_path_new ();
  ctk_tree_model_rows_reordered (CTK_TREE_MODEL (list_store),
				 path, NULL, new_order);
  ctk_tree_path_free (path);
  g_free (new_order);
}

static gboolean
iter_is_sorted (CtkListStore *list_store,
                CtkTreeIter  *iter)
{
  GSequenceIter *cmp;

  if (!g_sequence_iter_is_begin (iter->user_data))
    {
      cmp = g_sequence_iter_prev (iter->user_data);
      if (ctk_list_store_compare_func (cmp, iter->user_data, list_store) > 0)
	return FALSE;
    }

  cmp = g_sequence_iter_next (iter->user_data);
  if (!g_sequence_iter_is_end (cmp))
    {
      if (ctk_list_store_compare_func (iter->user_data, cmp, list_store) > 0)
	return FALSE;
    }
  
  return TRUE;
}

static void
ctk_list_store_sort_iter_changed (CtkListStore *list_store,
				  CtkTreeIter  *iter,
				  gint          column)

{
  CtkListStorePrivate *priv = list_store->priv;
  CtkTreePath *path;

  path = ctk_list_store_get_path (CTK_TREE_MODEL (list_store), iter);
  ctk_tree_model_row_changed (CTK_TREE_MODEL (list_store), path, iter);
  ctk_tree_path_free (path);

  if (!iter_is_sorted (list_store, iter))
    {
      GHashTable *old_positions;
      gint *order;

      old_positions = save_positions (priv->seq);
      g_sequence_sort_changed_iter (iter->user_data,
				    ctk_list_store_compare_func,
				    list_store);
      order = generate_order (priv->seq, old_positions);
      path = ctk_tree_path_new ();
      ctk_tree_model_rows_reordered (CTK_TREE_MODEL (list_store),
                                     path, NULL, order);
      ctk_tree_path_free (path);
      g_free (order);
    }
}

static gboolean
ctk_list_store_get_sort_column_id (CtkTreeSortable  *sortable,
				   gint             *sort_column_id,
				   CtkSortType      *order)
{
  CtkListStore *list_store = CTK_LIST_STORE (sortable);
  CtkListStorePrivate *priv = list_store->priv;

  if (sort_column_id)
    * sort_column_id = priv->sort_column_id;
  if (order)
    * order = priv->order;

  if (priv->sort_column_id == CTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID ||
      priv->sort_column_id == CTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    return FALSE;

  return TRUE;
}

static void
ctk_list_store_set_sort_column_id (CtkTreeSortable  *sortable,
				   gint              sort_column_id,
				   CtkSortType       order)
{
  CtkListStore *list_store = CTK_LIST_STORE (sortable);
  CtkListStorePrivate *priv = list_store->priv;

  if ((priv->sort_column_id == sort_column_id) &&
      (priv->order == order))
    return;

  if (sort_column_id != CTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    {
      if (sort_column_id != CTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
	{
	  CtkTreeDataSortHeader *header = NULL;

	  header = _ctk_tree_data_list_get_header (priv->sort_list, 
						   sort_column_id);

	  /* We want to make sure that we have a function */
	  g_return_if_fail (header != NULL);
	  g_return_if_fail (header->func != NULL);
	}
      else
	{
	  g_return_if_fail (priv->default_sort_func != NULL);
	}
    }


  priv->sort_column_id = sort_column_id;
  priv->order = order;

  ctk_tree_sortable_sort_column_changed (sortable);

  ctk_list_store_sort (list_store);
}

static void
ctk_list_store_set_sort_func (CtkTreeSortable        *sortable,
			      gint                    sort_column_id,
			      CtkTreeIterCompareFunc  func,
			      gpointer                data,
			      GDestroyNotify          destroy)
{
  CtkListStore *list_store = CTK_LIST_STORE (sortable);
  CtkListStorePrivate *priv = list_store->priv;

  priv->sort_list = _ctk_tree_data_list_set_header (priv->sort_list, 
							  sort_column_id, 
							  func, data, destroy);

  if (priv->sort_column_id == sort_column_id)
    ctk_list_store_sort (list_store);
}

static void
ctk_list_store_set_default_sort_func (CtkTreeSortable        *sortable,
				      CtkTreeIterCompareFunc  func,
				      gpointer                data,
				      GDestroyNotify          destroy)
{
  CtkListStore *list_store = CTK_LIST_STORE (sortable);
  CtkListStorePrivate *priv = list_store->priv;

  if (priv->default_sort_destroy)
    {
      GDestroyNotify d = priv->default_sort_destroy;

      priv->default_sort_destroy = NULL;
      d (priv->default_sort_data);
    }

  priv->default_sort_func = func;
  priv->default_sort_data = data;
  priv->default_sort_destroy = destroy;

  if (priv->sort_column_id == CTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
    ctk_list_store_sort (list_store);
}

static gboolean
ctk_list_store_has_default_sort_func (CtkTreeSortable *sortable)
{
  CtkListStore *list_store = CTK_LIST_STORE (sortable);
  CtkListStorePrivate *priv = list_store->priv;

  return (priv->default_sort_func != NULL);
}


/**
 * ctk_list_store_insert_with_values:
 * @list_store: A #CtkListStore
 * @iter: (out) (allow-none): An unset #CtkTreeIter to set to the new row, or %NULL
 * @position: position to insert the new row, or -1 to append after existing
 *     rows
 * @...: pairs of column number and value, terminated with -1
 *
 * Creates a new row at @position. @iter will be changed to point to this new
 * row. If @position is -1, or larger than the number of rows in the list, then
 * the new row will be appended to the list. The row will be filled with the
 * values given to this function.
 *
 * Calling
 * `ctk_list_store_insert_with_values (list_store, iter, position...)`
 * has the same effect as calling
 * |[<!-- language="C" -->
 * static void
 * insert_value (CtkListStore *list_store,
 *               CtkTreeIter  *iter,
 *               int           position)
 * {
 *   ctk_list_store_insert (list_store, iter, position);
 *   ctk_list_store_set (list_store,
 *                       iter
 *                       // ...
 *                       );
 * }
 * ]|
 * with the difference that the former will only emit a row_inserted signal,
 * while the latter will emit row_inserted, row_changed and, if the list store
 * is sorted, rows_reordered. Since emitting the rows_reordered signal
 * repeatedly can affect the performance of the program,
 * ctk_list_store_insert_with_values() should generally be preferred when
 * inserting rows in a sorted list store.
 *
 * Since: 2.6
 */
void
ctk_list_store_insert_with_values (CtkListStore *list_store,
				   CtkTreeIter  *iter,
				   gint          position,
				   ...)
{
  CtkListStorePrivate *priv;
  CtkTreePath *path;
  GSequence *seq;
  GSequenceIter *ptr;
  CtkTreeIter tmp_iter;
  gint length;
  gboolean changed = FALSE;
  gboolean maybe_need_sort = FALSE;
  va_list var_args;

  /* FIXME: refactor to reduce overlap with ctk_list_store_set() */
  g_return_if_fail (CTK_IS_LIST_STORE (list_store));

  priv = list_store->priv;

  if (!iter)
    iter = &tmp_iter;

  priv->columns_dirty = TRUE;

  seq = priv->seq;

  length = g_sequence_get_length (seq);
  if (position > length || position < 0)
    position = length;

  ptr = g_sequence_get_iter_at_pos (seq, position);
  ptr = g_sequence_insert_before (ptr, NULL);

  iter->stamp = priv->stamp;
  iter->user_data = ptr;

  g_assert (iter_is_valid (iter, list_store));

  priv->length++;

  va_start (var_args, position);
  ctk_list_store_set_valist_internal (list_store, iter, 
				      &changed, &maybe_need_sort,
				      var_args);
  va_end (var_args);

  /* Don't emit rows_reordered here */
  if (maybe_need_sort && CTK_LIST_STORE_IS_SORTED (list_store))
    g_sequence_sort_changed_iter (iter->user_data,
				  ctk_list_store_compare_func,
				  list_store);

  /* Just emit row_inserted */
  path = ctk_list_store_get_path (CTK_TREE_MODEL (list_store), iter);
  ctk_tree_model_row_inserted (CTK_TREE_MODEL (list_store), path, iter);
  ctk_tree_path_free (path);
}


/**
 * ctk_list_store_insert_with_valuesv:
 * @list_store: A #CtkListStore
 * @iter: (out) (allow-none): An unset #CtkTreeIter to set to the new row, or %NULL.
 * @position: position to insert the new row, or -1 for last
 * @columns: (array length=n_values): an array of column numbers
 * @values: (array length=n_values): an array of GValues 
 * @n_values: the length of the @columns and @values arrays
 * 
 * A variant of ctk_list_store_insert_with_values() which
 * takes the columns and values as two arrays, instead of
 * varargs. This function is mainly intended for 
 * language-bindings.
 *
 * Since: 2.6
 */
void
ctk_list_store_insert_with_valuesv (CtkListStore *list_store,
				    CtkTreeIter  *iter,
				    gint          position,
				    gint         *columns, 
				    GValue       *values,
				    gint          n_values)
{
  CtkListStorePrivate *priv;
  CtkTreePath *path;
  GSequence *seq;
  GSequenceIter *ptr;
  CtkTreeIter tmp_iter;
  gint length;
  gboolean changed = FALSE;
  gboolean maybe_need_sort = FALSE;

  /* FIXME refactor to reduce overlap with 
   * ctk_list_store_insert_with_values() 
   */
  g_return_if_fail (CTK_IS_LIST_STORE (list_store));

  priv = list_store->priv;

  if (!iter)
    iter = &tmp_iter;

  priv->columns_dirty = TRUE;

  seq = priv->seq;

  length = g_sequence_get_length (seq);
  if (position > length || position < 0)
    position = length;

  ptr = g_sequence_get_iter_at_pos (seq, position);
  ptr = g_sequence_insert_before (ptr, NULL);

  iter->stamp = priv->stamp;
  iter->user_data = ptr;

  g_assert (iter_is_valid (iter, list_store));

  priv->length++;  

  ctk_list_store_set_vector_internal (list_store, iter,
				      &changed, &maybe_need_sort,
				      columns, values, n_values);

  /* Don't emit rows_reordered here */
  if (maybe_need_sort && CTK_LIST_STORE_IS_SORTED (list_store))
    g_sequence_sort_changed_iter (iter->user_data,
				  ctk_list_store_compare_func,
				  list_store);

  /* Just emit row_inserted */
  path = ctk_list_store_get_path (CTK_TREE_MODEL (list_store), iter);
  ctk_tree_model_row_inserted (CTK_TREE_MODEL (list_store), path, iter);
  ctk_tree_path_free (path);
}

/* CtkBuildable custom tag implementation
 *
 * <columns>
 *   <column type="..."/>
 *   <column type="..."/>
 * </columns>
 */
typedef struct {
  gboolean translatable;
  gchar *context;
  int id;
} ColInfo;

typedef struct {
  CtkBuilder *builder;
  GObject *object;
  GSList *column_type_names;
  GType *column_types;
  GValue *values;
  gint *colids;
  ColInfo **columns;
  gint last_row;
  gint n_columns;
  gint row_column;
  gboolean is_data;
  const gchar *domain;
} SubParserData;

static void
list_store_start_element (GMarkupParseContext  *context,
                          const gchar          *element_name,
                          const gchar         **names,
                          const gchar         **values,
                          gpointer              user_data,
                          GError              **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (strcmp (element_name, "col") == 0)
    {
      gint id = -1;
      const gchar *id_str;
      const gchar *msg_context = NULL;
      gboolean translatable = FALSE;
      ColInfo *info;
      GValue val = G_VALUE_INIT;

      if (!_ctk_builder_check_parent (data->builder, context, "row", error))
        return;

      if (data->row_column >= data->n_columns)
        {
	  g_set_error (error,
                       CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE,
	  	       "Too many columns, maximum is %d", data->n_columns - 1);
          _ctk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "id", &id_str,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &msg_context,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _ctk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (!ctk_builder_value_from_string_type (data->builder, G_TYPE_INT, id_str, &val, error))
        {
          _ctk_builder_prefix_error (data->builder, context, error);
          return;
        }

      id = g_value_get_int (&val);
      if (id < 0 || id >= data->n_columns)
        {
          g_set_error (error,
                       CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE,
                       "id value %d out of range", id);
          _ctk_builder_prefix_error (data->builder, context, error);
          return;
        }

      info = g_slice_new0 (ColInfo);
      info->translatable = translatable;
      info->context = g_strdup (msg_context);
      info->id = id;

      data->colids[data->row_column] = id;
      data->columns[data->row_column] = info;
      data->row_column++;
      data->is_data = TRUE;
    }
  else if (strcmp (element_name, "row") == 0)
    {
      if (!_ctk_builder_check_parent (data->builder, context, "data", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _ctk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "columns") == 0 ||
           strcmp (element_name, "data") == 0)
    {
      if (!_ctk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _ctk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "column") == 0)
    {
      const gchar *type;

      if (!_ctk_builder_check_parent (data->builder, context, "columns", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "type", &type,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _ctk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->column_type_names = g_slist_prepend (data->column_type_names, g_strdup (type));
    }
  else
    {
      _ctk_builder_error_unhandled_tag (data->builder, context,
                                        "CtkListStore", element_name,
                                        error);
    }
}

static void
list_store_end_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        gpointer              user_data,
                        GError              **error)
{
  SubParserData *data = (SubParserData*)user_data;

  g_assert (data->builder);

  if (strcmp (element_name, "row") == 0)
    {
      CtkTreeIter iter;
      int i;

      ctk_list_store_insert_with_valuesv (CTK_LIST_STORE (data->object),
                                          &iter,
                                          data->last_row,
                                          data->colids,
                                          data->values,
                                          data->row_column);
      for (i = 0; i < data->row_column; i++)
        {
          ColInfo *info = data->columns[i];
          g_free (info->context);
          g_slice_free (ColInfo, info);
          data->columns[i] = NULL;
          g_value_unset (&data->values[i]);
        }
      g_free (data->values);
      data->values = g_new0 (GValue, data->n_columns);
      data->last_row++;
      data->row_column = 0;
    }
  else if (strcmp (element_name, "columns") == 0)
    {
      GType *column_types;
      GSList *l;
      int i;
      GType type;

      data->column_type_names = g_slist_reverse (data->column_type_names);
      column_types = g_new0 (GType, g_slist_length (data->column_type_names));

      for (l = data->column_type_names, i = 0; l; l = l->next, i++)
        {
          type = ctk_builder_get_type_from_name (data->builder, l->data);
          if (type == G_TYPE_INVALID)
            {
              g_warning ("Unknown type %s specified in treemodel %s",
                         (const gchar*)l->data,
                         ctk_buildable_get_name (CTK_BUILDABLE (data->object)));
              continue;
            }
          column_types[i] = type;

          g_free (l->data);
        }

      ctk_list_store_set_column_types (CTK_LIST_STORE (data->object), i, column_types);

      g_free (column_types);
    }
  else if (strcmp (element_name, "col") == 0)
    {
      data->is_data = FALSE;
    }
}

static void
list_store_text (GMarkupParseContext  *context,
                 const gchar          *text,
                 gsize                 text_len,
                 gpointer              user_data,
                 GError              **error)
{
  SubParserData *data = (SubParserData*)user_data;
  gint i;
  gchar *string;
  ColInfo *info;

  if (!data->is_data)
    return;

  i = data->row_column - 1;
  info = data->columns[i];

  string = g_strndup (text, text_len);
  if (info->translatable && text_len)
    {
      gchar *translated;

      /* FIXME: This will not use the domain set in the .ui file,
       * since the parser is not telling the builder about the domain.
       * However, it will work for ctk_builder_set_translation_domain() calls.
       */
      translated = g_strdup (_ctk_builder_parser_translate (data->domain,
                                                            info->context,
                                                            string));
      g_free (string);
      string = translated;
    }

  if (!ctk_builder_value_from_string_type (data->builder,
                                           data->column_types[info->id],
                                           string,
                                           &data->values[i],
                                           error))
    {
      _ctk_builder_prefix_error (data->builder, context, error);
    }
  g_free (string);
}

static const GMarkupParser list_store_parser =
  {
    .start_element = list_store_start_element,
    .end_element = list_store_end_element,
    .text = list_store_text
  };

static gboolean
ctk_list_store_buildable_custom_tag_start (CtkBuildable  *buildable,
                                           CtkBuilder    *builder,
                                           GObject       *child,
                                           const gchar   *tagname,
                                           GMarkupParser *parser,
                                           gpointer      *parser_data)
{
  SubParserData *data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "columns") == 0)
    {
      data = g_slice_new0 (SubParserData);
      data->builder = builder;
      data->object = G_OBJECT (buildable);
      data->column_type_names = NULL;

      *parser = list_store_parser;
      *parser_data = data;

      return TRUE;
    }
  else if (strcmp (tagname, "data") == 0)
    {
      gint n_columns = ctk_list_store_get_n_columns (CTK_TREE_MODEL (buildable));
      if (n_columns == 0)
        g_error ("Cannot append data to an empty model");

      data = g_slice_new0 (SubParserData);
      data->builder = builder;
      data->object = G_OBJECT (buildable);
      data->values = g_new0 (GValue, n_columns);
      data->colids = g_new0 (gint, n_columns);
      data->columns = g_new0 (ColInfo*, n_columns);
      data->column_types = CTK_LIST_STORE (buildable)->priv->column_headers;
      data->n_columns = n_columns;
      data->last_row = 0;
      data->domain = ctk_builder_get_translation_domain (builder);

      *parser = list_store_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
ctk_list_store_buildable_custom_tag_end (CtkBuildable *buildable,
                                         CtkBuilder   *builder,
                                         GObject      *child,
                                         const gchar  *tagname,
                                         gpointer     *parser_data)
{
  SubParserData *data = (SubParserData*)parser_data;

  if (strcmp (tagname, "columns") == 0)
    {
      g_slist_free (data->column_type_names);
      g_slice_free (SubParserData, data);
    }
  else if (strcmp (tagname, "data") == 0)
    {
      int i;
      for (i = 0; i < data->n_columns; i++)
        {
          ColInfo *info = data->columns[i];
          if (info)
            {
              g_free (info->context);
              g_slice_free (ColInfo, info);
            }
        }
      g_free (data->colids);
      g_free (data->columns);
      g_free (data->values);
      g_slice_free (SubParserData, data);
    }
}
