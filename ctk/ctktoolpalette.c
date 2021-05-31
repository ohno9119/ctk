/* GtkToolPalette -- A tool palette with categories and DnD support
 * Copyright (C) 2008  Openismus GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Mathias Hasselmann
 */

#include "config.h"

#include <string.h>
#include <ctk/ctk.h>

#include "ctktoolpaletteprivate.h"
#include "ctkmarshalers.h"
#include "ctktypebuiltins.h"
#include "ctkprivate.h"
#include "ctkscrollable.h"
#include "ctkorientableprivate.h"
#include "ctkintl.h"

#define DEFAULT_ICON_SIZE       CTK_ICON_SIZE_SMALL_TOOLBAR
#define DEFAULT_ORIENTATION     CTK_ORIENTATION_VERTICAL
#define DEFAULT_TOOLBAR_STYLE   CTK_TOOLBAR_ICONS

#define DEFAULT_CHILD_EXCLUSIVE FALSE
#define DEFAULT_CHILD_EXPAND    FALSE

/**
 * SECTION:ctktoolpalette
 * @Short_description: A tool palette with categories
 * @Title: GtkToolPalette
 *
 * A #GtkToolPalette allows you to add #GtkToolItems to a palette-like
 * container with different categories and drag and drop support.
 *
 * A #GtkToolPalette is created with a call to ctk_tool_palette_new().
 *
 * #GtkToolItems cannot be added directly to a #GtkToolPalette -
 * instead they are added to a #GtkToolItemGroup which can than be added
 * to a #GtkToolPalette. To add a #GtkToolItemGroup to a #GtkToolPalette,
 * use ctk_container_add().
 *
 * |[<!-- language="C" -->
 * GtkWidget *palette, *group;
 * GtkToolItem *item;
 *
 * palette = ctk_tool_palette_new ();
 * group = ctk_tool_item_group_new (_("Test Category"));
 * ctk_container_add (CTK_CONTAINER (palette), group);
 *
 * item = ctk_tool_button_new (NULL, _("_Open"));
 * ctk_tool_button_set_icon_name (CTK_TOOL_BUTTON (item), "document-open");
 * ctk_tool_item_group_insert (CTK_TOOL_ITEM_GROUP (group), item, -1);
 * ]|
 *
 * The easiest way to use drag and drop with #GtkToolPalette is to call
 * ctk_tool_palette_add_drag_dest() with the desired drag source @palette
 * and the desired drag target @widget. Then ctk_tool_palette_get_drag_item()
 * can be used to get the dragged item in the #GtkWidget::drag-data-received
 * signal handler of the drag target.
 *
 * |[<!-- language="C" -->
 * static void
 * passive_canvas_drag_data_received (GtkWidget        *widget,
 *                                    GdkDragContext   *context,
 *                                    gint              x,
 *                                    gint              y,
 *                                    GtkSelectionData *selection,
 *                                    guint             info,
 *                                    guint             time,
 *                                    gpointer          data)
 * {
 *   GtkWidget *palette;
 *   GtkWidget *item;
 *
 *   // Get the dragged item
 *   palette = ctk_widget_get_ancestor (ctk_drag_get_source_widget (context),
 *                                      CTK_TYPE_TOOL_PALETTE);
 *   if (palette != NULL)
 *     item = ctk_tool_palette_get_drag_item (CTK_TOOL_PALETTE (palette),
 *                                            selection);
 *
 *   // Do something with item
 * }
 *
 * GtkWidget *target, palette;
 *
 * palette = ctk_tool_palette_new ();
 * target = ctk_drawing_area_new ();
 *
 * g_signal_connect (G_OBJECT (target), "drag-data-received",
 *                   G_CALLBACK (passive_canvas_drag_data_received), NULL);
 * ctk_tool_palette_add_drag_dest (CTK_TOOL_PALETTE (palette), target,
 *                                 CTK_DEST_DEFAULT_ALL,
 *                                 CTK_TOOL_PALETTE_DRAG_ITEMS,
 *                                 GDK_ACTION_COPY);
 * ]|
 *
 * # CSS nodes
 *
 * GtkToolPalette has a single CSS node named toolpalette.
 *
 * Since: 2.20
 */

typedef struct _GtkToolItemGroupInfo   GtkToolItemGroupInfo;
typedef struct _GtkToolPaletteDragData GtkToolPaletteDragData;

enum
{
  PROP_NONE,
  PROP_ICON_SIZE,
  PROP_ICON_SIZE_SET,
  PROP_ORIENTATION,
  PROP_TOOLBAR_STYLE,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY
};

enum
{
  CHILD_PROP_NONE,
  CHILD_PROP_EXCLUSIVE,
  CHILD_PROP_EXPAND,
};

struct _GtkToolItemGroupInfo
{
  GtkToolItemGroup *widget;

  gulong            notify_collapsed;
  guint             pos;
  guint             exclusive : 1;
  guint             expand : 1;
};

struct _GtkToolPalettePrivate
{
  GPtrArray* groups;

  GtkAdjustment        *hadjustment;
  GtkAdjustment        *vadjustment;

  GtkIconSize           icon_size;
  gboolean              icon_size_set;
  GtkOrientation        orientation;
  GtkToolbarStyle       style;
  gboolean              style_set;

  GtkWidget            *expanding_child;

  GtkSizeGroup         *text_size_group;

  guint                 drag_source : 2;

  /* GtkScrollablePolicy needs to be checked when
   * driving the scrollable adjustment values */
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;
};

struct _GtkToolPaletteDragData
{
  GtkToolPalette *palette;
  GtkWidget      *item;
};

static GdkAtom dnd_target_atom_item = GDK_NONE;
static GdkAtom dnd_target_atom_group = GDK_NONE;

static const GtkTargetEntry dnd_targets[] =
{
  { "application/x-ctk-tool-palette-item", CTK_TARGET_SAME_APP, 0 },
  { "application/x-ctk-tool-palette-group", CTK_TARGET_SAME_APP, 0 },
};

static void ctk_tool_palette_set_hadjustment (GtkToolPalette *palette,
                                              GtkAdjustment  *adjustment);
static void ctk_tool_palette_set_vadjustment (GtkToolPalette *palette,
                                              GtkAdjustment  *adjustment);


G_DEFINE_TYPE_WITH_CODE (GtkToolPalette,
                         ctk_tool_palette,
                         CTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkToolPalette)
                         G_IMPLEMENT_INTERFACE (CTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (CTK_TYPE_SCROLLABLE, NULL))

static void
ctk_tool_palette_init (GtkToolPalette *palette)
{
  palette->priv = ctk_tool_palette_get_instance_private (palette);
  palette->priv->groups = g_ptr_array_sized_new (4);
  g_ptr_array_set_free_func (palette->priv->groups, g_free);

  palette->priv->icon_size = DEFAULT_ICON_SIZE;
  palette->priv->icon_size_set = FALSE;
  palette->priv->orientation = DEFAULT_ORIENTATION;
  palette->priv->style = DEFAULT_TOOLBAR_STYLE;
  palette->priv->style_set = FALSE;

  palette->priv->text_size_group = ctk_size_group_new (CTK_SIZE_GROUP_BOTH);

  if (dnd_target_atom_item == GDK_NONE)
    {
      dnd_target_atom_item = gdk_atom_intern_static_string (dnd_targets[0].target);
      dnd_target_atom_group = gdk_atom_intern_static_string (dnd_targets[1].target);
    }
}

static void
ctk_tool_palette_reconfigured (GtkToolPalette *palette)
{
  guint i;

  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *info = g_ptr_array_index (palette->priv->groups, i);
      if (info->widget)
        _ctk_tool_item_group_palette_reconfigured (info->widget);
    }

  ctk_widget_queue_resize_no_redraw (CTK_WIDGET (palette));
}

static void
ctk_tool_palette_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (object);

  switch (prop_id)
    {
      case PROP_ICON_SIZE:
        if (palette->priv->icon_size != g_value_get_enum (value))
          {
            palette->priv->icon_size = g_value_get_enum (value);
            ctk_tool_palette_reconfigured (palette);
            g_object_notify_by_pspec (object, pspec);
          }
        break;

      case PROP_ICON_SIZE_SET:
        if (palette->priv->icon_size_set != g_value_get_boolean (value))
          {
            palette->priv->icon_size_set = g_value_get_boolean (value);
            ctk_tool_palette_reconfigured (palette);
            g_object_notify_by_pspec (object, pspec);
          }
        break;

      case PROP_ORIENTATION:
        if (palette->priv->orientation != g_value_get_enum (value))
          {
            palette->priv->orientation = g_value_get_enum (value);
            _ctk_orientable_set_style_classes (CTK_ORIENTABLE (palette));
            ctk_tool_palette_reconfigured (palette);
            g_object_notify_by_pspec (object, pspec);
          }
        break;

      case PROP_TOOLBAR_STYLE:
        if (palette->priv->style != g_value_get_enum (value))
          {
            palette->priv->style = g_value_get_enum (value);
            ctk_tool_palette_reconfigured (palette);
            g_object_notify_by_pspec (object, pspec);
          }
        break;

      case PROP_HADJUSTMENT:
        ctk_tool_palette_set_hadjustment (palette, g_value_get_object (value));
        break;

      case PROP_VADJUSTMENT:
        ctk_tool_palette_set_vadjustment (palette, g_value_get_object (value));
        break;

      case PROP_HSCROLL_POLICY:
        if (palette->priv->hscroll_policy != g_value_get_enum (value))
          {
	    palette->priv->hscroll_policy = g_value_get_enum (value);
	    ctk_widget_queue_resize (CTK_WIDGET (palette));
            g_object_notify_by_pspec (object, pspec);
          }
	break;

      case PROP_VSCROLL_POLICY:
        if (palette->priv->vscroll_policy != g_value_get_enum (value))
          {
	    palette->priv->vscroll_policy = g_value_get_enum (value);
	    ctk_widget_queue_resize (CTK_WIDGET (palette));
            g_object_notify_by_pspec (object, pspec);
          }
	break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
ctk_tool_palette_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (object);

  switch (prop_id)
    {
      case PROP_ICON_SIZE:
        g_value_set_enum (value, ctk_tool_palette_get_icon_size (palette));
        break;

      case PROP_ICON_SIZE_SET:
        g_value_set_boolean (value, palette->priv->icon_size_set);
        break;

      case PROP_ORIENTATION:
        g_value_set_enum (value, palette->priv->orientation);
        break;

      case PROP_TOOLBAR_STYLE:
        g_value_set_enum (value, ctk_tool_palette_get_style (palette));
        break;

      case PROP_HADJUSTMENT:
        g_value_set_object (value, palette->priv->hadjustment);
        break;

      case PROP_VADJUSTMENT:
        g_value_set_object (value, palette->priv->vadjustment);
        break;

      case PROP_HSCROLL_POLICY:
	g_value_set_enum (value, palette->priv->hscroll_policy);
	break;

      case PROP_VSCROLL_POLICY:
	g_value_set_enum (value, palette->priv->vscroll_policy);
	break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
ctk_tool_palette_dispose (GObject *object)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (object);
  guint i;

  if (palette->priv->hadjustment)
    {
      g_object_unref (palette->priv->hadjustment);
      palette->priv->hadjustment = NULL;
    }

  if (palette->priv->vadjustment)
    {
      g_object_unref (palette->priv->vadjustment);
      palette->priv->vadjustment = NULL;
    }

  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *group = g_ptr_array_index (palette->priv->groups, i);

      if (group->notify_collapsed)
        {
          g_signal_handler_disconnect (group->widget, group->notify_collapsed);
          group->notify_collapsed = 0;
        }
    }

  if (palette->priv->text_size_group)
    {
      g_object_unref (palette->priv->text_size_group);
      palette->priv->text_size_group = NULL;
    }

  G_OBJECT_CLASS (ctk_tool_palette_parent_class)->dispose (object);
}

static void
ctk_tool_palette_finalize (GObject *object)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (object);

  g_ptr_array_free (palette->priv->groups, TRUE);

  G_OBJECT_CLASS (ctk_tool_palette_parent_class)->finalize (object);
}

static void
ctk_tool_palette_size_request (GtkWidget      *widget,
                               GtkRequisition *requisition)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (widget);
  GtkRequisition child_requisition;
  guint border_width;
  guint i;

  border_width = ctk_container_get_border_width (CTK_CONTAINER (widget));

  requisition->width = 0;
  requisition->height = 0;

  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *group = g_ptr_array_index (palette->priv->groups, i);

      if (!group->widget)
        continue;

      ctk_widget_get_preferred_size (CTK_WIDGET (group->widget),
                                     &child_requisition, NULL);

      if (CTK_ORIENTATION_VERTICAL == palette->priv->orientation)
        {
          requisition->width = MAX (requisition->width, child_requisition.width);
          requisition->height += child_requisition.height;
        }
      else
        {
          requisition->width += child_requisition.width;
          requisition->height = MAX (requisition->height, child_requisition.height);
        }
    }

  requisition->width += border_width * 2;
  requisition->height += border_width * 2;
}

static void
ctk_tool_palette_get_preferred_width (GtkWidget *widget,
				      gint      *minimum,
				      gint      *natural)
{
  GtkRequisition requisition;

  ctk_tool_palette_size_request (widget, &requisition);

  *minimum = *natural = requisition.width;
}

static void
ctk_tool_palette_get_preferred_height (GtkWidget *widget,
				       gint      *minimum,
				       gint      *natural)
{
  GtkRequisition requisition;

  ctk_tool_palette_size_request (widget, &requisition);

  *minimum = *natural = requisition.height;
}


static void
ctk_tool_palette_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (widget);
  GtkAdjustment *adjustment = NULL;
  GtkAllocation child_allocation;

  gint n_expand_groups = 0;
  gint remaining_space = 0;
  gint expand_space = 0;

  gint total_size, page_size;
  gint offset = 0;
  guint i;
  guint border_width;

  gint min_offset = -1, max_offset = -1;

  gint x;

  gint *group_sizes = g_newa (gint, palette->priv->groups->len);
  GtkTextDirection direction;

  border_width = ctk_container_get_border_width (CTK_CONTAINER (widget));
  direction = ctk_widget_get_direction (widget);

  CTK_WIDGET_CLASS (ctk_tool_palette_parent_class)->size_allocate (widget, allocation);

  if (CTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    {
      adjustment = palette->priv->vadjustment;
      page_size = allocation->height;
    }
  else
    {
      adjustment = palette->priv->hadjustment;
      page_size = allocation->width;
    }

  if (adjustment)
    offset = ctk_adjustment_get_value (adjustment);
  if (CTK_ORIENTATION_HORIZONTAL == palette->priv->orientation &&
      CTK_TEXT_DIR_RTL == direction)
    offset = -offset;

  if (CTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    child_allocation.width = allocation->width - border_width * 2;
  else
    child_allocation.height = allocation->height - border_width * 2;

  if (CTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    remaining_space = allocation->height;
  else
    remaining_space = allocation->width;

  /* figure out the required size of all groups to be able to distribute the
   * remaining space on allocation
   */
  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *group = g_ptr_array_index (palette->priv->groups, i);
      gint size;

      group_sizes[i] = 0;

      if (!group->widget)
        continue;

      widget = CTK_WIDGET (group->widget);

      if (ctk_tool_item_group_get_n_items (group->widget))
        {
          if (CTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            size = _ctk_tool_item_group_get_height_for_width (group->widget, child_allocation.width);
          else
            size = _ctk_tool_item_group_get_width_for_height (group->widget, child_allocation.height);

          if (group->expand && !ctk_tool_item_group_get_collapsed (group->widget))
            n_expand_groups += 1;
        }
      else
        size = 0;

      remaining_space -= size;
      group_sizes[i] = size;

      /* if the widget is currently expanding an offset which allows to
       * display as much of the widget as possible is calculated
       */
      if (widget == palette->priv->expanding_child)
        {
          gint limit =
            CTK_ORIENTATION_VERTICAL == palette->priv->orientation ?
            child_allocation.width : child_allocation.height;

          gint real_size;
          guint j;

          min_offset = 0;

          for (j = 0; j < i; ++j)
            min_offset += group_sizes[j];

          max_offset = min_offset + group_sizes[i];

          real_size = _ctk_tool_item_group_get_size_for_limit
            (CTK_TOOL_ITEM_GROUP (widget), limit,
             CTK_ORIENTATION_VERTICAL == palette->priv->orientation,
             FALSE);

          if (size == real_size)
            palette->priv->expanding_child = NULL;
        }
    }

  if (n_expand_groups > 0)
    {
      remaining_space = MAX (0, remaining_space);
      expand_space = remaining_space / n_expand_groups;
    }

  if (max_offset != -1)
    {
      gint limit =
        CTK_ORIENTATION_VERTICAL == palette->priv->orientation ?
        allocation->height : allocation->width;

      offset = MIN (MAX (offset, max_offset - limit), min_offset);
    }

  if (remaining_space > 0)
    offset = 0;

  x = border_width;
  child_allocation.y = border_width;

  if (CTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    child_allocation.y -= offset;
  else
    x -= offset;

  /* allocate all groups at the calculated positions */
  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *group = g_ptr_array_index (palette->priv->groups, i);

      if (!group->widget)
        continue;

      if (ctk_tool_item_group_get_n_items (group->widget))
        {
          gint size = group_sizes[i];

          if (group->expand && !ctk_tool_item_group_get_collapsed (group->widget))
            {
              size += MIN (expand_space, remaining_space);
              remaining_space -= expand_space;
            }

          if (CTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            child_allocation.height = size;
          else
            child_allocation.width = size;

          if (CTK_ORIENTATION_HORIZONTAL == palette->priv->orientation &&
              CTK_TEXT_DIR_RTL == direction)
            child_allocation.x = allocation->width - x - child_allocation.width;
          else
            child_allocation.x = x;

          ctk_widget_size_allocate (CTK_WIDGET (group->widget), &child_allocation);
          ctk_widget_show (CTK_WIDGET (group->widget));

          if (CTK_ORIENTATION_VERTICAL == palette->priv->orientation)
            child_allocation.y += child_allocation.height;
          else
            x += child_allocation.width;
        }
      else
        ctk_widget_hide (CTK_WIDGET (group->widget));
    }

  if (CTK_ORIENTATION_VERTICAL == palette->priv->orientation)
    {
      child_allocation.y += border_width;
      child_allocation.y += offset;

      total_size = child_allocation.y;
    }
  else
    {
      x += border_width;
      x += offset;

      total_size = x;
    }

  /* update the scrollbar to match the displayed adjustment */
  if (adjustment)
    {
      gdouble lower, upper;

      total_size = MAX (0, total_size);
      page_size = MIN (total_size, page_size);

      if (CTK_ORIENTATION_VERTICAL == palette->priv->orientation ||
          CTK_TEXT_DIR_LTR == direction)
        {
          lower = 0;
          upper = total_size;
        }
      else
        {
          lower = page_size - total_size;
          upper = page_size;

          offset = -offset;
        }

      ctk_adjustment_configure (adjustment,
                                offset,
                                lower,
                                upper,
                                page_size * 0.1,
                                page_size * 0.9,
                                page_size);
    }
}

static void
ctk_tool_palette_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  guint border_width;

  ctk_widget_set_realized (widget, TRUE);

  border_width = ctk_container_get_border_width (CTK_CONTAINER (widget));

  ctk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x + border_width;
  attributes.y = allocation.y + border_width;
  attributes.width = allocation.width - border_width * 2;
  attributes.height = allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = ctk_widget_get_visual (widget);
  attributes.event_mask = ctk_widget_get_events (widget)
                         | GDK_VISIBILITY_NOTIFY_MASK
                         | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                         | GDK_BUTTON_MOTION_MASK
                         | GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK
                         | GDK_TOUCH_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (ctk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  ctk_widget_set_window (widget, window);
  ctk_widget_register_window (widget, window);

  ctk_container_forall (CTK_CONTAINER (widget),
                        (GtkCallback) ctk_widget_set_parent_window,
                        window);

  ctk_widget_queue_resize_no_redraw (widget);
}

static void
ctk_tool_palette_adjustment_value_changed (GtkAdjustment *adjustment,
                                           gpointer       data)
{
  GtkAllocation allocation;
  GtkWidget *widget = CTK_WIDGET (data);

  ctk_widget_get_allocation (widget, &allocation);
  ctk_tool_palette_size_allocate (widget, &allocation);
}

static gboolean
ctk_tool_palette_draw (GtkWidget *widget,
                       cairo_t   *cr)
{
  ctk_render_background (ctk_widget_get_style_context (widget), cr,
                         0, 0,
                         ctk_widget_get_allocated_width (widget),
                         ctk_widget_get_allocated_height (widget));

  return CTK_WIDGET_CLASS (ctk_tool_palette_parent_class)->draw (widget, cr);
}

static void
ctk_tool_palette_add (GtkContainer *container,
                      GtkWidget    *child)
{
  GtkToolPalette *palette;
  GtkToolItemGroupInfo *info = g_new0(GtkToolItemGroupInfo, 1);

  g_return_if_fail (CTK_IS_TOOL_PALETTE (container));
  g_return_if_fail (CTK_IS_TOOL_ITEM_GROUP (child));

  palette = CTK_TOOL_PALETTE (container);

  g_ptr_array_add (palette->priv->groups, info);
  info->pos = palette->priv->groups->len - 1;
  info->widget = (GtkToolItemGroup *) g_object_ref_sink (child);

  ctk_widget_set_parent (child, CTK_WIDGET (palette));
}

static void
ctk_tool_palette_remove (GtkContainer *container,
                         GtkWidget    *child)
{
  GtkToolPalette *palette;
  guint i;

  g_return_if_fail (CTK_IS_TOOL_PALETTE (container));
  palette = CTK_TOOL_PALETTE (container);

  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *info = g_ptr_array_index (palette->priv->groups, i);
      if (CTK_WIDGET(info->widget) == child)
        {
          g_object_unref (child);
          ctk_widget_unparent (child);

          g_ptr_array_remove_index (palette->priv->groups, i);
        }
    }
}

static void
ctk_tool_palette_forall (GtkContainer *container,
                         gboolean      internals,
                         GtkCallback   callback,
                         gpointer      callback_data)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (container);
  guint i, len;

  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *info = g_ptr_array_index (palette->priv->groups, i);

      len = palette->priv->groups->len;

      if (info->widget)
        callback (CTK_WIDGET (info->widget),
                  callback_data);

      /* At destroy time, 'callback' results in removing a widget,
       * here we just reset the current index to account for the removed widget. */
      i -= (len - palette->priv->groups->len);
    }
}

static GType
ctk_tool_palette_child_type (GtkContainer *container)
{
  return CTK_TYPE_TOOL_ITEM_GROUP;
}

static void
ctk_tool_palette_set_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (container);

  switch (prop_id)
    {
      case CHILD_PROP_EXCLUSIVE:
        ctk_tool_palette_set_exclusive (palette, CTK_TOOL_ITEM_GROUP (child),
          g_value_get_boolean (value));
        break;

      case CHILD_PROP_EXPAND:
        ctk_tool_palette_set_expand (palette, CTK_TOOL_ITEM_GROUP (child),
          g_value_get_boolean (value));
        break;

      default:
        CTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
        break;
    }
}

static void
ctk_tool_palette_get_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (container);

  switch (prop_id)
    {
      case CHILD_PROP_EXCLUSIVE:
        g_value_set_boolean (value,
          ctk_tool_palette_get_exclusive (palette, CTK_TOOL_ITEM_GROUP (child)));
        break;

      case CHILD_PROP_EXPAND:
        g_value_set_boolean (value,
          ctk_tool_palette_get_expand (palette, CTK_TOOL_ITEM_GROUP (child)));
        break;

      default:
        CTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
        break;
    }
}

static void
ctk_tool_palette_screen_changed (GtkWidget *widget,
                                 GdkScreen *previous_screen)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (widget);

  ctk_tool_palette_reconfigured (palette);
}


static void
ctk_tool_palette_class_init (GtkToolPaletteClass *cls)
{
  GObjectClass      *oclass   = G_OBJECT_CLASS (cls);
  GtkWidgetClass    *wclass   = CTK_WIDGET_CLASS (cls);
  GtkContainerClass *cclass   = CTK_CONTAINER_CLASS (cls);

  oclass->set_property        = ctk_tool_palette_set_property;
  oclass->get_property        = ctk_tool_palette_get_property;
  oclass->dispose             = ctk_tool_palette_dispose;
  oclass->finalize            = ctk_tool_palette_finalize;

  wclass->get_preferred_width = ctk_tool_palette_get_preferred_width;
  wclass->get_preferred_height= ctk_tool_palette_get_preferred_height;
  wclass->size_allocate       = ctk_tool_palette_size_allocate;
  wclass->realize             = ctk_tool_palette_realize;
  wclass->draw                = ctk_tool_palette_draw;

  cclass->add                 = ctk_tool_palette_add;
  cclass->remove              = ctk_tool_palette_remove;
  cclass->forall              = ctk_tool_palette_forall;
  cclass->child_type          = ctk_tool_palette_child_type;
  cclass->set_child_property  = ctk_tool_palette_set_child_property;
  cclass->get_child_property  = ctk_tool_palette_get_child_property;

  /* Handle screen-changed so we can update our configuration.
   */
  wclass->screen_changed      = ctk_tool_palette_screen_changed;

  g_object_class_override_property (oclass, PROP_ORIENTATION,    "orientation");

  g_object_class_override_property (oclass, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (oclass, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (oclass, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (oclass, PROP_VSCROLL_POLICY, "vscroll-policy");

  /**
   * GtkToolPalette:icon-size:
   *
   * The size of the icons in a tool palette. When this property is set,
   * it overrides the default setting.
   *
   * This should only be used for special-purpose tool palettes, normal
   * application tool palettes should respect the user preferences for the
   * size of icons.
   *
   * Since: 2.20
   */
  g_object_class_install_property (oclass,
                                   PROP_ICON_SIZE,
                                   g_param_spec_enum ("icon-size",
                                                      P_("Icon size"),
                                                      P_("Size of icons in this tool palette"),
                                                      CTK_TYPE_ICON_SIZE,
                                                      DEFAULT_ICON_SIZE,
                                                      CTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkToolPalette:icon-size-set:
   *
   * Is %TRUE if the #GtkToolPalette:icon-size property has been set.
   *
   * Since: 2.20
   */
  g_object_class_install_property (oclass,
                                   PROP_ICON_SIZE_SET,
                                   g_param_spec_boolean ("icon-size-set",
                                                         P_("Icon size set"),
                                                         P_("Whether the icon-size property has been set"),
                                                         FALSE,
                                                         CTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkToolPalette:toolbar-style:
   *
   * The style of items in the tool palette.
   *
   * Since: 2.20
   */
  g_object_class_install_property (oclass, PROP_TOOLBAR_STYLE,
                                   g_param_spec_enum ("toolbar-style",
                                                      P_("Toolbar Style"),
                                                      P_("Style of items in the tool palette"),
                                                      CTK_TYPE_TOOLBAR_STYLE,
                                                      DEFAULT_TOOLBAR_STYLE,
                                                      CTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  /**
   * GtkToolPalette:exclusive:
   *
   * Whether the item group should be the only one that is expanded
   * at a given time.
   *
   * Since: 2.20
   */
  ctk_container_class_install_child_property (cclass, CHILD_PROP_EXCLUSIVE,
                                              g_param_spec_boolean ("exclusive",
                                                                    P_("Exclusive"),
                                                                    P_("Whether the item group should be the only expanded at a given time"),
                                                                    DEFAULT_CHILD_EXCLUSIVE,
                                                                    CTK_PARAM_READWRITE));

  /**
   * GtkToolPalette:expand:
   *
   * Whether the item group should receive extra space when the palette grows.
   * at a given time.
   *
   * Since: 2.20
   */
  ctk_container_class_install_child_property (cclass, CHILD_PROP_EXPAND,
                                              g_param_spec_boolean ("expand",
                                                                    P_("Expand"),
                                                                    P_("Whether the item group should receive extra space when the palette grows"),
                                                                    DEFAULT_CHILD_EXPAND,
                                                                    CTK_PARAM_READWRITE));

  ctk_widget_class_set_css_name (wclass, "toolpalette");
}

/**
 * ctk_tool_palette_new:
 *
 * Creates a new tool palette.
 *
 * Returns: a new #GtkToolPalette
 *
 * Since: 2.20
 */
GtkWidget*
ctk_tool_palette_new (void)
{
  return g_object_new (CTK_TYPE_TOOL_PALETTE, NULL);
}

/**
 * ctk_tool_palette_set_icon_size:
 * @palette: a #GtkToolPalette
 * @icon_size: (type int): the #GtkIconSize that icons in the tool
 *     palette shall have
 *
 * Sets the size of icons in the tool palette.
 *
 * Since: 2.20
 */
void
ctk_tool_palette_set_icon_size (GtkToolPalette *palette,
                                GtkIconSize     icon_size)
{
  GtkToolPalettePrivate *priv;

  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (icon_size != CTK_ICON_SIZE_INVALID);

  priv = palette->priv;

  if (!priv->icon_size_set)
    {
      priv->icon_size_set = TRUE;
      g_object_notify (G_OBJECT (palette), "icon-size-set");
    }

  if (priv->icon_size == icon_size)
    return;

  priv->icon_size = icon_size;
  g_object_notify (G_OBJECT (palette), "icon-size");

  ctk_tool_palette_reconfigured (palette);

  ctk_widget_queue_resize (CTK_WIDGET (palette));
}

/**
 * ctk_tool_palette_unset_icon_size:
 * @palette: a #GtkToolPalette
 *
 * Unsets the tool palette icon size set with ctk_tool_palette_set_icon_size(),
 * so that user preferences will be used to determine the icon size.
 *
 * Since: 2.20
 */
void
ctk_tool_palette_unset_icon_size (GtkToolPalette *palette)
{
  GtkToolPalettePrivate* priv = palette->priv;
  GtkIconSize size;

  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));

  if (palette->priv->icon_size_set)
    {
      size = DEFAULT_ICON_SIZE;

      if (size != palette->priv->icon_size)
      {
        ctk_tool_palette_set_icon_size (palette, size);
        g_object_notify (G_OBJECT (palette), "icon-size");
      }

      priv->icon_size_set = FALSE;
      g_object_notify (G_OBJECT (palette), "icon-size-set");
    }
}

/* Set the "toolbar-style" property and do appropriate things.
 * GtkToolbar does this by emitting a signal instead of just
 * calling a function...
 */
static void
ctk_tool_palette_change_style (GtkToolPalette  *palette,
                               GtkToolbarStyle  style)
{
  GtkToolPalettePrivate* priv = palette->priv;

  if (priv->style != style)
    {
      priv->style = style;

      ctk_tool_palette_reconfigured (palette);

      ctk_widget_queue_resize (CTK_WIDGET (palette));
      g_object_notify (G_OBJECT (palette), "toolbar-style");
    }
}


/**
 * ctk_tool_palette_set_style:
 * @palette: a #GtkToolPalette
 * @style: the #GtkToolbarStyle that items in the tool palette shall have
 *
 * Sets the style (text, icons or both) of items in the tool palette.
 *
 * Since: 2.20
 */
void
ctk_tool_palette_set_style (GtkToolPalette  *palette,
                            GtkToolbarStyle  style)
{
  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));

  palette->priv->style_set = TRUE;
  ctk_tool_palette_change_style (palette, style);
}


/**
 * ctk_tool_palette_unset_style:
 * @palette: a #GtkToolPalette
 *
 * Unsets a toolbar style set with ctk_tool_palette_set_style(),
 * so that user preferences will be used to determine the toolbar style.
 *
 * Since: 2.20
 */
void
ctk_tool_palette_unset_style (GtkToolPalette *palette)
{
  GtkToolPalettePrivate* priv = palette->priv;
  GtkToolbarStyle style;

  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));

  if (priv->style_set)
    {
      style = DEFAULT_TOOLBAR_STYLE;

      if (style != priv->style)
        ctk_tool_palette_change_style (palette, style);

      priv->style_set = FALSE;
    }
}

/**
 * ctk_tool_palette_get_icon_size:
 * @palette: a #GtkToolPalette
 *
 * Gets the size of icons in the tool palette.
 * See ctk_tool_palette_set_icon_size().
 *
 * Returns: (type int): the #GtkIconSize of icons in the tool palette
 *
 * Since: 2.20
 */
GtkIconSize
ctk_tool_palette_get_icon_size (GtkToolPalette *palette)
{
  g_return_val_if_fail (CTK_IS_TOOL_PALETTE (palette), DEFAULT_ICON_SIZE);

  return palette->priv->icon_size;
}

/**
 * ctk_tool_palette_get_style:
 * @palette: a #GtkToolPalette
 *
 * Gets the style (icons, text or both) of items in the tool palette.
 *
 * Returns: the #GtkToolbarStyle of items in the tool palette.
 *
 * Since: 2.20
 */
GtkToolbarStyle
ctk_tool_palette_get_style (GtkToolPalette *palette)
{
  g_return_val_if_fail (CTK_IS_TOOL_PALETTE (palette), DEFAULT_TOOLBAR_STYLE);

  return palette->priv->style;
}

static gint
ctk_tool_palette_compare_groups (gconstpointer a,
                                 gconstpointer b)
{
  const GtkToolItemGroupInfo *group_a = *((GtkToolItemGroupInfo **) a);
  const GtkToolItemGroupInfo *group_b = *((GtkToolItemGroupInfo **) b);

  return group_a->pos - group_b->pos;
}

/**
 * ctk_tool_palette_set_group_position:
 * @palette: a #GtkToolPalette
 * @group: a #GtkToolItemGroup which is a child of palette
 * @position: a new index for group
 *
 * Sets the position of the group as an index of the tool palette.
 * If position is 0 the group will become the first child, if position is
 * -1 it will become the last child.
 *
 * Since: 2.20
 */
void
ctk_tool_palette_set_group_position (GtkToolPalette   *palette,
                                     GtkToolItemGroup *group,
                                     gint             position)
{
  GtkToolItemGroupInfo *group_new;
  GtkToolItemGroupInfo *group_old;
  gint old_position;

  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (CTK_IS_TOOL_ITEM_GROUP (group));
  g_return_if_fail (position >= -1);

  if (-1 == position)
    position = palette->priv->groups->len - 1;

  g_return_if_fail ((guint) position < palette->priv->groups->len);

  group_new = g_ptr_array_index (palette->priv->groups, position);

  if (CTK_TOOL_ITEM_GROUP (group) == group_new->widget)
    return;

  old_position = ctk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (old_position >= 0);

  group_old = g_ptr_array_index (palette->priv->groups, old_position);

  group_new->pos = position;
  group_old->pos = old_position;

  g_ptr_array_sort (palette->priv->groups, ctk_tool_palette_compare_groups);

  ctk_widget_queue_resize (CTK_WIDGET (palette));
}

static void
ctk_tool_palette_group_notify_collapsed (GtkToolItemGroup *group,
                                         GParamSpec       *pspec,
                                         gpointer          data)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (data);
  guint i;

  if (ctk_tool_item_group_get_collapsed (group))
    return;

  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *info = g_ptr_array_index (palette->priv->groups, i);
      GtkToolItemGroup *current_group = info->widget;

      if (current_group && current_group != group)
        ctk_tool_item_group_set_collapsed (current_group, TRUE);
    }
}

/**
 * ctk_tool_palette_set_exclusive:
 * @palette: a #GtkToolPalette
 * @group: a #GtkToolItemGroup which is a child of palette
 * @exclusive: whether the group should be exclusive or not
 *
 * Sets whether the group should be exclusive or not.
 * If an exclusive group is expanded all other groups are collapsed.
 *
 * Since: 2.20
 */
void
ctk_tool_palette_set_exclusive (GtkToolPalette   *palette,
                                GtkToolItemGroup *group,
                                gboolean          exclusive)
{
  GtkToolItemGroupInfo *group_info;
  gint position;

  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (CTK_IS_TOOL_ITEM_GROUP (group));

  position = ctk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (position >= 0);

  group_info = g_ptr_array_index (palette->priv->groups, position);

  if (exclusive == group_info->exclusive)
    return;

  group_info->exclusive = exclusive;

  if (group_info->exclusive != (0 != group_info->notify_collapsed))
    {
      if (group_info->exclusive)
        {
          group_info->notify_collapsed =
            g_signal_connect (group, "notify::collapsed",
                              G_CALLBACK (ctk_tool_palette_group_notify_collapsed),
                              palette);
        }
      else
        {
          g_signal_handler_disconnect (group, group_info->notify_collapsed);
          group_info->notify_collapsed = 0;
        }
    }

  ctk_tool_palette_group_notify_collapsed (group_info->widget, NULL, palette);
  ctk_widget_child_notify (CTK_WIDGET (group), "exclusive");
}

/**
 * ctk_tool_palette_set_expand:
 * @palette: a #GtkToolPalette
 * @group: a #GtkToolItemGroup which is a child of palette
 * @expand: whether the group should be given extra space
 *
 * Sets whether the group should be given extra space.
 *
 * Since: 2.20
 */
void
ctk_tool_palette_set_expand (GtkToolPalette   *palette,
                             GtkToolItemGroup *group,
                             gboolean        expand)
{
  GtkToolItemGroupInfo *group_info;
  gint position;

  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (CTK_IS_TOOL_ITEM_GROUP (group));

  position = ctk_tool_palette_get_group_position (palette, group);
  g_return_if_fail (position >= 0);

  group_info = g_ptr_array_index (palette->priv->groups, position);

  if (expand != group_info->expand)
    {
      group_info->expand = expand;
      ctk_widget_queue_resize (CTK_WIDGET (palette));
      ctk_widget_child_notify (CTK_WIDGET (group), "expand");
    }
}

/**
 * ctk_tool_palette_get_group_position:
 * @palette: a #GtkToolPalette
 * @group: a #GtkToolItemGroup
 *
 * Gets the position of @group in @palette as index.
 * See ctk_tool_palette_set_group_position().
 *
 * Returns: the index of group or -1 if @group is not a child of @palette
 *
 * Since: 2.20
 */
gint
ctk_tool_palette_get_group_position (GtkToolPalette   *palette,
                                     GtkToolItemGroup *group)
{
  guint i;

  g_return_val_if_fail (CTK_IS_TOOL_PALETTE (palette), -1);
  g_return_val_if_fail (CTK_IS_TOOL_ITEM_GROUP (group), -1);

  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *info = g_ptr_array_index (palette->priv->groups, i);
      if ((gpointer) group == info->widget)
        return i;
    }

  return -1;
}

/**
 * ctk_tool_palette_get_exclusive:
 * @palette: a #GtkToolPalette
 * @group: a #GtkToolItemGroup which is a child of palette
 *
 * Gets whether @group is exclusive or not.
 * See ctk_tool_palette_set_exclusive().
 *
 * Returns: %TRUE if @group is exclusive
 *
 * Since: 2.20
 */
gboolean
ctk_tool_palette_get_exclusive (GtkToolPalette   *palette,
                                GtkToolItemGroup *group)
{
  gint position;
  GtkToolItemGroupInfo *info;

  g_return_val_if_fail (CTK_IS_TOOL_PALETTE (palette), DEFAULT_CHILD_EXCLUSIVE);
  g_return_val_if_fail (CTK_IS_TOOL_ITEM_GROUP (group), DEFAULT_CHILD_EXCLUSIVE);

  position = ctk_tool_palette_get_group_position (palette, group);
  g_return_val_if_fail (position >= 0, DEFAULT_CHILD_EXCLUSIVE);

  info = g_ptr_array_index (palette->priv->groups, position);

  return info->exclusive;
}

/**
 * ctk_tool_palette_get_expand:
 * @palette: a #GtkToolPalette
 * @group: a #GtkToolItemGroup which is a child of palette
 *
 * Gets whether group should be given extra space.
 * See ctk_tool_palette_set_expand().
 *
 * Returns: %TRUE if group should be given extra space, %FALSE otherwise
 *
 * Since: 2.20
 */
gboolean
ctk_tool_palette_get_expand (GtkToolPalette   *palette,
                             GtkToolItemGroup *group)
{
  gint position;
  GtkToolItemGroupInfo *info;

  g_return_val_if_fail (CTK_IS_TOOL_PALETTE (palette), DEFAULT_CHILD_EXPAND);
  g_return_val_if_fail (CTK_IS_TOOL_ITEM_GROUP (group), DEFAULT_CHILD_EXPAND);

  position = ctk_tool_palette_get_group_position (palette, group);
  g_return_val_if_fail (position >= 0, DEFAULT_CHILD_EXPAND);

  info = g_ptr_array_index (palette->priv->groups, position);

  return info->expand;
}

/**
 * ctk_tool_palette_get_drop_item:
 * @palette: a #GtkToolPalette
 * @x: the x position
 * @y: the y position
 *
 * Gets the item at position (x, y).
 * See ctk_tool_palette_get_drop_group().
 *
 * Returns: (nullable) (transfer none): the #GtkToolItem at position or %NULL if there is no such item
 *
 * Since: 2.20
 */
GtkToolItem*
ctk_tool_palette_get_drop_item (GtkToolPalette *palette,
                                gint            x,
                                gint            y)
{
  GtkAllocation allocation;
  GtkToolItemGroup *group = ctk_tool_palette_get_drop_group (palette, x, y);
  GtkWidget *widget = CTK_WIDGET (group);

  if (group)
    {
      ctk_widget_get_allocation (widget, &allocation);
      return ctk_tool_item_group_get_drop_item (group,
                                                x - allocation.x,
                                                y - allocation.y);
    }

  return NULL;
}

/**
 * ctk_tool_palette_get_drop_group:
 * @palette: a #GtkToolPalette
 * @x: the x position
 * @y: the y position
 *
 * Gets the group at position (x, y).
 *
 * Returns: (nullable) (transfer none): the #GtkToolItemGroup at position
 * or %NULL if there is no such group
 *
 * Since: 2.20
 */
GtkToolItemGroup*
ctk_tool_palette_get_drop_group (GtkToolPalette *palette,
                                 gint            x,
                                 gint            y)
{
  GtkAllocation allocation;
  guint i;

  g_return_val_if_fail (CTK_IS_TOOL_PALETTE (palette), NULL);

  ctk_widget_get_allocation (CTK_WIDGET (palette), &allocation);

  g_return_val_if_fail (x >= 0 && x < allocation.width, NULL);
  g_return_val_if_fail (y >= 0 && y < allocation.height, NULL);

  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *group = g_ptr_array_index (palette->priv->groups, i);
      GtkWidget *widget;
      gint x0, y0;

      if (!group->widget)
        continue;

      widget = CTK_WIDGET (group->widget);
      ctk_widget_get_allocation (widget, &allocation);

      x0 = x - allocation.x;
      y0 = y - allocation.y;

      if (x0 >= 0 && x0 < allocation.width &&
          y0 >= 0 && y0 < allocation.height)
        return CTK_TOOL_ITEM_GROUP (widget);
    }

  return NULL;
}

/**
 * ctk_tool_palette_get_drag_item:
 * @palette: a #GtkToolPalette
 * @selection: a #GtkSelectionData
 *
 * Get the dragged item from the selection.
 * This could be a #GtkToolItem or a #GtkToolItemGroup.
 *
 * Returns: (transfer none): the dragged item in selection
 *
 * Since: 2.20
 */
GtkWidget*
ctk_tool_palette_get_drag_item (GtkToolPalette         *palette,
                                const GtkSelectionData *selection)
{
  GtkToolPaletteDragData *data;
  GdkAtom target;

  g_return_val_if_fail (CTK_IS_TOOL_PALETTE (palette), NULL);
  g_return_val_if_fail (NULL != selection, NULL);

  g_return_val_if_fail (ctk_selection_data_get_format (selection) == 8, NULL);
  g_return_val_if_fail (ctk_selection_data_get_length (selection) == sizeof (GtkToolPaletteDragData), NULL);
  target = ctk_selection_data_get_target (selection);
  g_return_val_if_fail (target == dnd_target_atom_item ||
                        target == dnd_target_atom_group,
                        NULL);

  data = (GtkToolPaletteDragData*) ctk_selection_data_get_data (selection);

  g_return_val_if_fail (data->palette == palette, NULL);

  if (dnd_target_atom_item == target)
    g_return_val_if_fail (CTK_IS_TOOL_ITEM (data->item), NULL);
  else if (dnd_target_atom_group == target)
    g_return_val_if_fail (CTK_IS_TOOL_ITEM_GROUP (data->item), NULL);

  return data->item;
}

/**
 * ctk_tool_palette_set_drag_source:
 * @palette: a #GtkToolPalette
 * @targets: the #GtkToolPaletteDragTargets
 *     which the widget should support
 *
 * Sets the tool palette as a drag source.
 * Enables all groups and items in the tool palette as drag sources
 * on button 1 and button 3 press with copy and move actions.
 * See ctk_drag_source_set().
 *
 * Since: 2.20
 */
void
ctk_tool_palette_set_drag_source (GtkToolPalette            *palette,
                                  GtkToolPaletteDragTargets  targets)
{
  guint i;

  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));

  if ((palette->priv->drag_source & targets) == targets)
    return;

  palette->priv->drag_source |= targets;

  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkToolItemGroupInfo *info = g_ptr_array_index (palette->priv->groups, i);
      if (info->widget)
        ctk_container_forall (CTK_CONTAINER (info->widget),
                              _ctk_tool_palette_child_set_drag_source,
                              palette);
    }
}

/**
 * ctk_tool_palette_add_drag_dest:
 * @palette: a #GtkToolPalette
 * @widget: a #GtkWidget which should be a drag destination for @palette
 * @flags: the flags that specify what actions GTK+ should take for drops
 *     on that widget
 * @targets: the #GtkToolPaletteDragTargets which the widget
 *     should support
 * @actions: the #GdkDragActions which the widget should suppport
 *
 * Sets @palette as drag source (see ctk_tool_palette_set_drag_source())
 * and sets @widget as a drag destination for drags from @palette.
 * See ctk_drag_dest_set().
 *
 * Since: 2.20
 */
void
ctk_tool_palette_add_drag_dest (GtkToolPalette            *palette,
                                GtkWidget                 *widget,
                                GtkDestDefaults            flags,
                                GtkToolPaletteDragTargets  targets,
                                GdkDragAction              actions)
{
  GtkTargetEntry entries[G_N_ELEMENTS (dnd_targets)];
  gint n_entries = 0;

  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (CTK_IS_WIDGET (widget));

  ctk_tool_palette_set_drag_source (palette,
                                    targets);

  if (targets & CTK_TOOL_PALETTE_DRAG_ITEMS)
    entries[n_entries++] = dnd_targets[0];
  if (targets & CTK_TOOL_PALETTE_DRAG_GROUPS)
    entries[n_entries++] = dnd_targets[1];

  ctk_drag_dest_set (widget, flags, entries, n_entries, actions);
}

void
_ctk_tool_palette_get_item_size (GtkToolPalette *palette,
                                 GtkRequisition *item_size,
                                 gboolean        homogeneous_only,
                                 gint           *requested_rows)
{
  GtkRequisition max_requisition;
  gint max_rows;
  guint i;

  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));
  g_return_if_fail (NULL != item_size);

  max_requisition.width = 0;
  max_requisition.height = 0;
  max_rows = 0;

  /* iterate over all groups and calculate the max item_size and max row request */
  for (i = 0; i < palette->priv->groups->len; ++i)
    {
      GtkRequisition requisition;
      gint rows;
      GtkToolItemGroupInfo *group = g_ptr_array_index (palette->priv->groups, i);

      if (!group->widget)
        continue;

      _ctk_tool_item_group_item_size_request (group->widget, &requisition, homogeneous_only, &rows);

      max_requisition.width = MAX (max_requisition.width, requisition.width);
      max_requisition.height = MAX (max_requisition.height, requisition.height);
      max_rows = MAX (max_rows, rows);
    }

  *item_size = max_requisition;
  if (requested_rows)
    *requested_rows = max_rows;
}

static void
ctk_tool_palette_item_drag_data_get (GtkWidget        *widget,
                                     GdkDragContext   *context,
                                     GtkSelectionData *selection,
                                     guint             info,
                                     guint             time,
                                     gpointer          data)
{
  GtkToolPaletteDragData drag_data = { CTK_TOOL_PALETTE (data), NULL };
  GdkAtom target;

  target = ctk_selection_data_get_target (selection);

  if (target == dnd_target_atom_item)
    drag_data.item = ctk_widget_get_ancestor (widget, CTK_TYPE_TOOL_ITEM);

  if (drag_data.item)
    ctk_selection_data_set (selection, target, 8,
                            (guchar*) &drag_data, sizeof (drag_data));
}

static void
ctk_tool_palette_child_drag_data_get (GtkWidget        *widget,
                                      GdkDragContext   *context,
                                      GtkSelectionData *selection,
                                      guint             info,
                                      guint             time,
                                      gpointer          data)
{
  GtkToolPaletteDragData drag_data = { CTK_TOOL_PALETTE (data), NULL };
  GdkAtom target;

  target = ctk_selection_data_get_target (selection);

  if (target == dnd_target_atom_group)
    drag_data.item = ctk_widget_get_ancestor (widget, CTK_TYPE_TOOL_ITEM_GROUP);

  if (drag_data.item)
    ctk_selection_data_set (selection, target, 8,
                            (guchar*) &drag_data, sizeof (drag_data));
}

void
_ctk_tool_palette_child_set_drag_source (GtkWidget *child,
                                         gpointer   data)
{
  GtkToolPalette *palette = CTK_TOOL_PALETTE (data);

  /* Check drag_source,
   * to work properly when called from ctk_tool_item_group_insert().
   */
  if (!palette->priv->drag_source)
    return;

  if (CTK_IS_TOOL_ITEM (child) &&
      (palette->priv->drag_source & CTK_TOOL_PALETTE_DRAG_ITEMS))
    {
      /* Connect to child instead of the item itself,
       * to work arround bug 510377.
       */
      if (CTK_IS_TOOL_BUTTON (child))
        child = ctk_bin_get_child (CTK_BIN (child));

      if (!child)
        return;

      ctk_drag_source_set (child, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           &dnd_targets[0], 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);

      g_signal_connect (child, "drag-data-get",
                        G_CALLBACK (ctk_tool_palette_item_drag_data_get),
                        palette);
    }
  else if (CTK_IS_BUTTON (child) &&
           (palette->priv->drag_source & CTK_TOOL_PALETTE_DRAG_GROUPS))
    {
      ctk_drag_source_set (child, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           &dnd_targets[1], 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);

      g_signal_connect (child, "drag-data-get",
                        G_CALLBACK (ctk_tool_palette_child_drag_data_get),
                        palette);
    }
}

/**
 * ctk_tool_palette_get_drag_target_item:
 *
 * Gets the target entry for a dragged #GtkToolItem.
 *
 * Returns: (transfer none): the #GtkTargetEntry for a dragged item.
 *
 * Since: 2.20
 */
const GtkTargetEntry*
ctk_tool_palette_get_drag_target_item (void)
{
  return &dnd_targets[0];
}

/**
 * ctk_tool_palette_get_drag_target_group:
 *
 * Get the target entry for a dragged #GtkToolItemGroup.
 *
 * Returns: (transfer none): the #GtkTargetEntry for a dragged group
 *
 * Since: 2.20
 */
const GtkTargetEntry*
ctk_tool_palette_get_drag_target_group (void)
{
  return &dnd_targets[1];
}

void
_ctk_tool_palette_set_expanding_child (GtkToolPalette *palette,
                                       GtkWidget      *widget)
{
  g_return_if_fail (CTK_IS_TOOL_PALETTE (palette));
  palette->priv->expanding_child = widget;
}

/**
 * ctk_tool_palette_get_hadjustment:
 * @palette: a #GtkToolPalette
 *
 * Gets the horizontal adjustment of the tool palette.
 *
 * Returns: (transfer none): the horizontal adjustment of @palette
 *
 * Since: 2.20
 *
 * Deprecated: 3.0: Use ctk_scrollable_get_hadjustment()
 */
GtkAdjustment*
ctk_tool_palette_get_hadjustment (GtkToolPalette *palette)
{
  g_return_val_if_fail (CTK_IS_TOOL_PALETTE (palette), NULL);

  return palette->priv->hadjustment;
}

static void
ctk_tool_palette_set_hadjustment (GtkToolPalette *palette,
                                  GtkAdjustment  *adjustment)
{
  GtkToolPalettePrivate *priv = palette->priv;

  if (adjustment && priv->hadjustment == adjustment)
    return;

  if (priv->hadjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->hadjustment,
                                            ctk_tool_palette_adjustment_value_changed,
                                            palette);
      g_object_unref (priv->hadjustment);
    }

  if (adjustment == NULL)
    adjustment = ctk_adjustment_new (0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (ctk_tool_palette_adjustment_value_changed),
                    palette);
  priv->hadjustment = g_object_ref_sink (adjustment);
  /* FIXME: Adjustment should probably have its values updated now */
  g_object_notify (G_OBJECT (palette), "hadjustment");
}

/**
 * ctk_tool_palette_get_vadjustment:
 * @palette: a #GtkToolPalette
 *
 * Gets the vertical adjustment of the tool palette.
 *
 * Returns: (transfer none): the vertical adjustment of @palette
 *
 * Since: 2.20
 *
 * Deprecated: 3.0: Use ctk_scrollable_get_vadjustment()
 */
GtkAdjustment*
ctk_tool_palette_get_vadjustment (GtkToolPalette *palette)
{
  g_return_val_if_fail (CTK_IS_TOOL_PALETTE (palette), NULL);

  return palette->priv->vadjustment;
}

static void
ctk_tool_palette_set_vadjustment (GtkToolPalette *palette,
                                  GtkAdjustment  *adjustment)
{
  GtkToolPalettePrivate *priv = palette->priv;

  if (adjustment && priv->vadjustment == adjustment)
    return;

  if (priv->vadjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->vadjustment,
                                            ctk_tool_palette_adjustment_value_changed,
                                            palette);
      g_object_unref (priv->vadjustment);
    }

  if (adjustment == NULL)
    adjustment = ctk_adjustment_new (0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (ctk_tool_palette_adjustment_value_changed),
                    palette);
  priv->vadjustment = g_object_ref_sink (adjustment);
  /* FIXME: Adjustment should probably have its values updated now */
  g_object_notify (G_OBJECT (palette), "vadjustment");
}

GtkSizeGroup *
_ctk_tool_palette_get_size_group (GtkToolPalette *palette)
{
  g_return_val_if_fail (CTK_IS_TOOL_PALETTE (palette), NULL);

  return palette->priv->text_size_group;
}
