/*
 * Copyright (c) 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author:
 *      Ikey Doherty <michael.i.doherty@intel.com>
 */

#include "config.h"

#include "ctkstacksidebar.h"

#include "ctklabel.h"
#include "ctklistbox.h"
#include "ctkscrolledwindow.h"
#include "ctkseparator.h"
#include "ctkstylecontext.h"
#include "ctkprivate.h"
#include "ctkintl.h"

/**
 * SECTION:ctkstacksidebar
 * @Title: GtkStackSidebar
 * @Short_description: An automatic sidebar widget
 *
 * A GtkStackSidebar enables you to quickly and easily provide a
 * consistent "sidebar" object for your user interface.
 *
 * In order to use a GtkStackSidebar, you simply use a GtkStack to
 * organize your UI flow, and add the sidebar to your sidebar area. You
 * can use ctk_stack_sidebar_set_stack() to connect the #GtkStackSidebar
 * to the #GtkStack.
 *
 * # CSS nodes
 *
 * GtkStackSidebar has a single CSS node with name stacksidebar and
 * style class .sidebar.
 *
 * When circumstances require it, GtkStackSidebar adds the
 * .needs-attention style class to the widgets representing the stack
 * pages.
 *
 * Since: 3.16
 */

struct _GtkStackSidebarPrivate
{
  GtkListBox *list;
  GtkStack *stack;
  GHashTable *rows;
  gboolean in_child_changed;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkStackSidebar, ctk_stack_sidebar, CTK_TYPE_BIN)

enum
{
  PROP_0,
  PROP_STACK,
  N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
ctk_stack_sidebar_set_property (GObject    *object,
                                guint       prop_id,
                                const       GValue *value,
                                GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_STACK:
      ctk_stack_sidebar_set_stack (CTK_STACK_SIDEBAR (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ctk_stack_sidebar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (CTK_STACK_SIDEBAR (object));

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, priv->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
update_header (GtkListBoxRow *row,
               GtkListBoxRow *before,
               gpointer       userdata)
{
  GtkWidget *ret = NULL;

  if (before && !ctk_list_box_row_get_header (row))
    {
      ret = ctk_separator_new (CTK_ORIENTATION_HORIZONTAL);
      ctk_list_box_row_set_header (row, ret);
    }
}

static gint
sort_list (GtkListBoxRow *row1,
           GtkListBoxRow *row2,
           gpointer       userdata)
{
  GtkStackSidebar *sidebar = CTK_STACK_SIDEBAR (userdata);
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *item;
  GtkWidget *widget;
  gint left = 0; gint right = 0;


  if (row1)
    {
      item = ctk_bin_get_child (CTK_BIN (row1));
      widget = g_object_get_data (G_OBJECT (item), "stack-child");
      ctk_container_child_get (CTK_CONTAINER (priv->stack), widget,
                               "position", &left,
                               NULL);
    }

  if (row2)
    {
      item = ctk_bin_get_child (CTK_BIN (row2));
      widget = g_object_get_data (G_OBJECT (item), "stack-child");
      ctk_container_child_get (CTK_CONTAINER (priv->stack), widget,
                               "position", &right,
                               NULL);
    }

  if (left < right)
    return  -1;

  if (left == right)
    return 0;

  return 1;
}

static void
ctk_stack_sidebar_row_selected (GtkListBox    *box,
                                GtkListBoxRow *row,
                                gpointer       userdata)
{
  GtkStackSidebar *sidebar = CTK_STACK_SIDEBAR (userdata);
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *item;
  GtkWidget *widget;

  if (priv->in_child_changed)
    return;

  if (!row)
    return;

  item = ctk_bin_get_child (CTK_BIN (row));
  widget = g_object_get_data (G_OBJECT (item), "stack-child");
  ctk_stack_set_visible_child (priv->stack, widget);
}

static void
ctk_stack_sidebar_init (GtkStackSidebar *sidebar)
{
  GtkStyleContext *style;
  GtkStackSidebarPrivate *priv;
  GtkWidget *sw;

  priv = ctk_stack_sidebar_get_instance_private (sidebar);

  sw = ctk_scrolled_window_new (NULL, NULL);
  ctk_widget_show (sw);
  ctk_widget_set_no_show_all (sw, TRUE);
  ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (sw),
                                  CTK_POLICY_NEVER,
                                  CTK_POLICY_AUTOMATIC);

  ctk_container_add (CTK_CONTAINER (sidebar), sw);

  priv->list = CTK_LIST_BOX (ctk_list_box_new ());
  ctk_widget_show (CTK_WIDGET (priv->list));

  ctk_container_add (CTK_CONTAINER (sw), CTK_WIDGET (priv->list));

  ctk_list_box_set_header_func (priv->list, update_header, sidebar, NULL);
  ctk_list_box_set_sort_func (priv->list, sort_list, sidebar, NULL);

  g_signal_connect (priv->list, "row-selected",
                    G_CALLBACK (ctk_stack_sidebar_row_selected), sidebar);

  style = ctk_widget_get_style_context (CTK_WIDGET (sidebar));
  ctk_style_context_add_class (style, "sidebar");

  priv->rows = g_hash_table_new (NULL, NULL);
}

static void
update_row (GtkStackSidebar *sidebar,
            GtkWidget       *widget,
            GtkWidget       *row)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *item;
  gchar *title;
  gboolean needs_attention;
  GtkStyleContext *context;

  ctk_container_child_get (CTK_CONTAINER (priv->stack), widget,
                           "title", &title,
                           "needs-attention", &needs_attention,
                           NULL);

  item = ctk_bin_get_child (CTK_BIN (row));
  ctk_label_set_text (CTK_LABEL (item), title);

  ctk_widget_set_visible (row, ctk_widget_get_visible (widget) && title != NULL);

  context = ctk_widget_get_style_context (row);
  if (needs_attention)
     ctk_style_context_add_class (context, CTK_STYLE_CLASS_NEEDS_ATTENTION);
  else
    ctk_style_context_remove_class (context, CTK_STYLE_CLASS_NEEDS_ATTENTION);

  g_free (title);
}

static void
on_position_updated (GtkWidget       *widget,
                     GParamSpec      *pspec,
                     GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);

  ctk_list_box_invalidate_sort (priv->list);
}

static void
on_child_updated (GtkWidget       *widget,
                  GParamSpec      *pspec,
                  GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *row;

  row = g_hash_table_lookup (priv->rows, widget);
  update_row (sidebar, widget, row);
}

static void
add_child (GtkWidget       *widget,
           GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *item;
  GtkWidget *row;

  /* Check we don't actually already know about this widget */
  if (g_hash_table_lookup (priv->rows, widget))
    return;

  /* Make a pretty item when we add kids */
  item = ctk_label_new ("");
  ctk_widget_set_halign (item, CTK_ALIGN_START);
  ctk_widget_set_valign (item, CTK_ALIGN_CENTER);
  row = ctk_list_box_row_new ();
  ctk_container_add (CTK_CONTAINER (row), item);
  ctk_widget_show (item);

  update_row (sidebar, widget, row);

  /* Hook up for events */
  g_signal_connect (widget, "child-notify::title",
                    G_CALLBACK (on_child_updated), sidebar);
  g_signal_connect (widget, "child-notify::needs-attention",
                    G_CALLBACK (on_child_updated), sidebar);
  g_signal_connect (widget, "notify::visible",
                    G_CALLBACK (on_child_updated), sidebar);
  g_signal_connect (widget, "child-notify::position",
                    G_CALLBACK (on_position_updated), sidebar);

  g_object_set_data (G_OBJECT (item), "stack-child", widget);
  g_hash_table_insert (priv->rows, widget, row);
  ctk_container_add (CTK_CONTAINER (priv->list), row);
}

static void
remove_child (GtkWidget       *widget,
              GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *row;

  row = g_hash_table_lookup (priv->rows, widget);
  if (!row)
    return;

  g_signal_handlers_disconnect_by_func (widget, on_child_updated, sidebar);
  g_signal_handlers_disconnect_by_func (widget, on_position_updated, sidebar);

  ctk_container_remove (CTK_CONTAINER (priv->list), row);
  g_hash_table_remove (priv->rows, widget);
}

static void
populate_sidebar (GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *widget, *row;

  ctk_container_foreach (CTK_CONTAINER (priv->stack), (GtkCallback)add_child, sidebar);

  widget = ctk_stack_get_visible_child (priv->stack);
  if (widget)
    {
      row = g_hash_table_lookup (priv->rows, widget);
      ctk_list_box_select_row (priv->list, CTK_LIST_BOX_ROW (row));
    }
}

static void
clear_sidebar (GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);

  ctk_container_foreach (CTK_CONTAINER (priv->stack), (GtkCallback)remove_child, sidebar);
}

static void
on_child_changed (GtkWidget       *widget,
                  GParamSpec      *pspec,
                  GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);
  GtkWidget *child;
  GtkWidget *row;

  child = ctk_stack_get_visible_child (CTK_STACK (widget));
  row = g_hash_table_lookup (priv->rows, child);
  if (row != NULL)
    {
      priv->in_child_changed = TRUE;
      ctk_list_box_select_row (priv->list, CTK_LIST_BOX_ROW (row));
      priv->in_child_changed = FALSE;
    }
}

static void
on_stack_child_added (GtkContainer    *container,
                      GtkWidget       *widget,
                      GtkStackSidebar *sidebar)
{
  add_child (widget, sidebar);
}

static void
on_stack_child_removed (GtkContainer    *container,
                        GtkWidget       *widget,
                        GtkStackSidebar *sidebar)
{
  remove_child (widget, sidebar);
}

static void
disconnect_stack_signals (GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);

  g_signal_handlers_disconnect_by_func (priv->stack, on_stack_child_added, sidebar);
  g_signal_handlers_disconnect_by_func (priv->stack, on_stack_child_removed, sidebar);
  g_signal_handlers_disconnect_by_func (priv->stack, on_child_changed, sidebar);
  g_signal_handlers_disconnect_by_func (priv->stack, disconnect_stack_signals, sidebar);
}

static void
connect_stack_signals (GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);

  g_signal_connect_after (priv->stack, "add",
                          G_CALLBACK (on_stack_child_added), sidebar);
  g_signal_connect_after (priv->stack, "remove",
                          G_CALLBACK (on_stack_child_removed), sidebar);
  g_signal_connect (priv->stack, "notify::visible-child",
                    G_CALLBACK (on_child_changed), sidebar);
  g_signal_connect_swapped (priv->stack, "destroy",
                            G_CALLBACK (disconnect_stack_signals), sidebar);
}

static void
ctk_stack_sidebar_dispose (GObject *object)
{
  GtkStackSidebar *sidebar = CTK_STACK_SIDEBAR (object);

  ctk_stack_sidebar_set_stack (sidebar, NULL);

  G_OBJECT_CLASS (ctk_stack_sidebar_parent_class)->dispose (object);
}

static void
ctk_stack_sidebar_finalize (GObject *object)
{
  GtkStackSidebar *sidebar = CTK_STACK_SIDEBAR (object);
  GtkStackSidebarPrivate *priv = ctk_stack_sidebar_get_instance_private (sidebar);

  g_hash_table_destroy (priv->rows);

  G_OBJECT_CLASS (ctk_stack_sidebar_parent_class)->finalize (object);
}

static void
ctk_stack_sidebar_class_init (GtkStackSidebarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = CTK_WIDGET_CLASS (klass);

  object_class->dispose = ctk_stack_sidebar_dispose;
  object_class->finalize = ctk_stack_sidebar_finalize;
  object_class->set_property = ctk_stack_sidebar_set_property;
  object_class->get_property = ctk_stack_sidebar_get_property;

  obj_properties[PROP_STACK] =
      g_param_spec_object (I_("stack"), P_("Stack"),
                           P_("Associated stack for this GtkStackSidebar"),
                           CTK_TYPE_STACK,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);

  ctk_widget_class_set_css_name (widget_class, "stacksidebar");
}

/**
 * ctk_stack_sidebar_new:
 *
 * Creates a new sidebar.
 *
 * Returns: the new #GtkStackSidebar
 *
 * Since: 3.16
 */
GtkWidget *
ctk_stack_sidebar_new (void)
{
  return CTK_WIDGET (g_object_new (CTK_TYPE_STACK_SIDEBAR, NULL));
}

/**
 * ctk_stack_sidebar_set_stack:
 * @sidebar: a #GtkStackSidebar
 * @stack: a #GtkStack
 *
 * Set the #GtkStack associated with this #GtkStackSidebar.
 *
 * The sidebar widget will automatically update according to the order
 * (packing) and items within the given #GtkStack.
 *
 * Since: 3.16
 */
void
ctk_stack_sidebar_set_stack (GtkStackSidebar *sidebar,
                             GtkStack        *stack)
{
  GtkStackSidebarPrivate *priv;

  g_return_if_fail (CTK_IS_STACK_SIDEBAR (sidebar));
  g_return_if_fail (CTK_IS_STACK (stack) || stack == NULL);

  priv = ctk_stack_sidebar_get_instance_private (sidebar);

  if (priv->stack == stack)
    return;

  if (priv->stack)
    {
      disconnect_stack_signals (sidebar);
      clear_sidebar (sidebar);
      g_clear_object (&priv->stack);
    }
  if (stack)
    {
      priv->stack = g_object_ref (stack);
      populate_sidebar (sidebar);
      connect_stack_signals (sidebar);
    }

  ctk_widget_queue_resize (CTK_WIDGET (sidebar));

  g_object_notify (G_OBJECT (sidebar), "stack");
}

/**
 * ctk_stack_sidebar_get_stack:
 * @sidebar: a #GtkStackSidebar
 *
 * Retrieves the stack.
 * See ctk_stack_sidebar_set_stack().
 *
 * Returns: (nullable) (transfer none): the associated #GtkStack or
 *     %NULL if none has been set explicitly
 *
 * Since: 3.16
 */
GtkStack *
ctk_stack_sidebar_get_stack (GtkStackSidebar *sidebar)
{
  GtkStackSidebarPrivate *priv;

  g_return_val_if_fail (CTK_IS_STACK_SIDEBAR (sidebar), NULL);

  priv = ctk_stack_sidebar_get_instance_private (sidebar);

  return CTK_STACK (priv->stack);
}
