/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ctkmenutoolbutton.h"

#include "ctktogglebutton.h"
#include "ctkmenubutton.h"
#include "ctkmenubuttonprivate.h"
#include "ctkbox.h"
#include "ctkmenu.h"
#include "ctkmain.h"
#include "ctksizerequest.h"
#include "ctkbuildable.h"

#include "ctkprivate.h"
#include "ctkintl.h"


/**
 * SECTION:ctkmenutoolbutton
 * @Short_description: A GtkToolItem containing a button with an additional dropdown menu
 * @Title: GtkMenuToolButton
 * @See_also: #GtkToolbar, #GtkToolButton
 *
 * A #GtkMenuToolButton is a #GtkToolItem that contains a button and
 * a small additional button with an arrow. When clicked, the arrow
 * button pops up a dropdown menu.
 *
 * Use ctk_menu_tool_button_new() to create a new
 * #GtkMenuToolButton.
 *
 * # GtkMenuToolButton as GtkBuildable
 *
 * The GtkMenuToolButton implementation of the GtkBuildable interface
 * supports adding a menu by specifying “menu” as the “type” attribute
 * of a <child> element.
 *
 * An example for a UI definition fragment with menus:
 * |[
 * <object class="GtkMenuToolButton">
 *   <child type="menu">
 *     <object class="GtkMenu"/>
 *   </child>
 * </object>
 * ]|
 */


struct _GtkMenuToolButtonPrivate
{
  GtkWidget *button;
  GtkWidget *arrow_button;
  GtkWidget *box;
};

static void ctk_menu_tool_button_buildable_interface_init (GtkBuildableIface   *iface);
static void ctk_menu_tool_button_buildable_add_child      (GtkBuildable        *buildable,
							   GtkBuilder          *builder,
							   GObject             *child,
							   const gchar         *type);

enum
{
  SHOW_MENU,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_MENU
};

static gint signals[LAST_SIGNAL];

static GtkBuildableIface *parent_buildable_iface;

G_DEFINE_TYPE_WITH_CODE (GtkMenuToolButton, ctk_menu_tool_button, CTK_TYPE_TOOL_BUTTON,
                         G_ADD_PRIVATE (GtkMenuToolButton)
                         G_IMPLEMENT_INTERFACE (CTK_TYPE_BUILDABLE,
                                                ctk_menu_tool_button_buildable_interface_init))

static void
ctk_menu_tool_button_construct_contents (GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv = button->priv;
  GtkWidget *box;
  GtkWidget *parent;
  GtkOrientation orientation;

  orientation = ctk_tool_item_get_orientation (CTK_TOOL_ITEM (button));

  if (orientation == CTK_ORIENTATION_HORIZONTAL)
    {
      box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
      ctk_menu_button_set_direction (CTK_MENU_BUTTON (priv->arrow_button), CTK_ARROW_DOWN);
    }
  else
    {
      GtkTextDirection direction;
      GtkArrowType type;

      box = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
      direction = ctk_widget_get_direction (CTK_WIDGET (button));
      type = (direction == CTK_TEXT_DIR_LTR ? CTK_ARROW_RIGHT : CTK_ARROW_LEFT);
      ctk_menu_button_set_direction (CTK_MENU_BUTTON (priv->arrow_button), type);
    }

  parent = ctk_widget_get_parent (priv->button);
  if (priv->button && parent)
    {
      g_object_ref (priv->button);
      ctk_container_remove (CTK_CONTAINER (parent),
                            priv->button);
      ctk_container_add (CTK_CONTAINER (box), priv->button);
      g_object_unref (priv->button);
    }

  parent = ctk_widget_get_parent (priv->arrow_button);
  if (priv->arrow_button && parent)
    {
      g_object_ref (priv->arrow_button);
      ctk_container_remove (CTK_CONTAINER (parent),
                            priv->arrow_button);
      ctk_box_pack_end (CTK_BOX (box), priv->arrow_button,
                        FALSE, FALSE, 0);
      g_object_unref (priv->arrow_button);
    }

  if (priv->box)
    {
      gchar *tmp;

      /* Transfer a possible tooltip to the new box */
      g_object_get (priv->box, "tooltip-markup", &tmp, NULL);

      if (tmp)
        {
	  g_object_set (box, "tooltip-markup", tmp, NULL);
	  g_free (tmp);
	}

      /* Note: we are not destroying the button and the arrow_button
       * here because they were removed from their container above
       */
      ctk_widget_destroy (priv->box);
    }

  priv->box = box;

  ctk_container_add (CTK_CONTAINER (button), priv->box);
  ctk_widget_show_all (priv->box);

  ctk_button_set_relief (CTK_BUTTON (priv->arrow_button),
			 ctk_tool_item_get_relief_style (CTK_TOOL_ITEM (button)));
  
  ctk_widget_queue_resize (CTK_WIDGET (button));
}

static void
ctk_menu_tool_button_toolbar_reconfigured (GtkToolItem *toolitem)
{
  ctk_menu_tool_button_construct_contents (CTK_MENU_TOOL_BUTTON (toolitem));

  /* chain up */
  CTK_TOOL_ITEM_CLASS (ctk_menu_tool_button_parent_class)->toolbar_reconfigured (toolitem);
}

static void
ctk_menu_tool_button_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkMenuToolButton *button = CTK_MENU_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_MENU:
      ctk_menu_tool_button_set_menu (button, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ctk_menu_tool_button_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkMenuToolButton *button = CTK_MENU_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_MENU:
      g_value_set_object (value, ctk_menu_button_get_popup (CTK_MENU_BUTTON (button->priv->arrow_button)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ctk_menu_tool_button_class_init (GtkMenuToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkToolItemClass *toolitem_class;

  object_class = (GObjectClass *)klass;
  toolitem_class = (GtkToolItemClass *)klass;

  object_class->set_property = ctk_menu_tool_button_set_property;
  object_class->get_property = ctk_menu_tool_button_get_property;

  toolitem_class->toolbar_reconfigured = ctk_menu_tool_button_toolbar_reconfigured;

  /**
   * GtkMenuToolButton::show-menu:
   * @button: the object on which the signal is emitted
   *
   * The ::show-menu signal is emitted before the menu is shown.
   *
   * It can be used to populate the menu on demand, using
   * ctk_menu_tool_button_set_menu().

   * Note that even if you populate the menu dynamically in this way,
   * you must set an empty menu on the #GtkMenuToolButton beforehand,
   * since the arrow is made insensitive if the menu is not set.
   */
  signals[SHOW_MENU] =
    g_signal_new (I_("show-menu"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuToolButtonClass, show_menu),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_MENU,
                                   g_param_spec_object ("menu",
                                                        P_("Menu"),
                                                        P_("The dropdown menu"),
                                                        CTK_TYPE_MENU,
                                                        CTK_PARAM_READWRITE));
}

static void
ctk_menu_tool_button_init (GtkMenuToolButton *button)
{
  GtkWidget *box;
  GtkWidget *arrow_button;
  GtkWidget *real_button;

  button->priv = ctk_menu_tool_button_get_instance_private (button);

  ctk_tool_item_set_homogeneous (CTK_TOOL_ITEM (button), FALSE);

  box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

  real_button = ctk_bin_get_child (CTK_BIN (button));
  g_object_ref (real_button);
  ctk_container_remove (CTK_CONTAINER (button), real_button);
  ctk_container_add (CTK_CONTAINER (box), real_button);
  g_object_unref (real_button);

  arrow_button = ctk_menu_button_new ();
  ctk_box_pack_end (CTK_BOX (box), arrow_button,
                    FALSE, FALSE, 0);

  /* the arrow button is insentive until we set a menu */
  ctk_widget_set_sensitive (arrow_button, FALSE);

  ctk_widget_show_all (box);

  ctk_container_add (CTK_CONTAINER (button), box);
  ctk_menu_button_set_align_widget (CTK_MENU_BUTTON (arrow_button),
                                    CTK_WIDGET (button));

  button->priv->button = real_button;
  button->priv->arrow_button = arrow_button;
  button->priv->box = box;
}

static void
ctk_menu_tool_button_buildable_add_child (GtkBuildable *buildable,
					  GtkBuilder   *builder,
					  GObject      *child,
					  const gchar  *type)
{
  if (type && strcmp (type, "menu") == 0)
    ctk_menu_tool_button_set_menu (CTK_MENU_TOOL_BUTTON (buildable),
                                   CTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
ctk_menu_tool_button_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = ctk_menu_tool_button_buildable_add_child;
}

/**
 * ctk_menu_tool_button_new:
 * @icon_widget: (allow-none): a widget that will be used as icon widget, or %NULL
 * @label: (allow-none): a string that will be used as label, or %NULL
 *
 * Creates a new #GtkMenuToolButton using @icon_widget as icon and
 * @label as label.
 *
 * Returns: the new #GtkMenuToolButton
 *
 * Since: 2.6
 **/
GtkToolItem *
ctk_menu_tool_button_new (GtkWidget   *icon_widget,
                          const gchar *label)
{
  GtkMenuToolButton *button;

  button = g_object_new (CTK_TYPE_MENU_TOOL_BUTTON, NULL);

  if (label)
    ctk_tool_button_set_label (CTK_TOOL_BUTTON (button), label);

  if (icon_widget)
    ctk_tool_button_set_icon_widget (CTK_TOOL_BUTTON (button), icon_widget);

  return CTK_TOOL_ITEM (button);
}

/**
 * ctk_menu_tool_button_new_from_stock:
 * @stock_id: the name of a stock item
 *
 * Creates a new #GtkMenuToolButton.
 * The new #GtkMenuToolButton will contain an icon and label from
 * the stock item indicated by @stock_id.
 *
 * Returns: the new #GtkMenuToolButton
 *
 * Since: 2.6
 *
 * Deprecated: 3.10: Use ctk_menu_tool_button_new() instead.
 **/
GtkToolItem *
ctk_menu_tool_button_new_from_stock (const gchar *stock_id)
{
  GtkMenuToolButton *button;

  g_return_val_if_fail (stock_id != NULL, NULL);

  button = g_object_new (CTK_TYPE_MENU_TOOL_BUTTON,
			 "stock-id", stock_id,
			 NULL);

  return CTK_TOOL_ITEM (button);
}

static void
_show_menu_emit (gpointer user_data)
{
  GtkMenuToolButton *button = (GtkMenuToolButton *) user_data;
  g_signal_emit (button, signals[SHOW_MENU], 0);
}

/**
 * ctk_menu_tool_button_set_menu:
 * @button: a #GtkMenuToolButton
 * @menu: the #GtkMenu associated with #GtkMenuToolButton
 *
 * Sets the #GtkMenu that is popped up when the user clicks on the arrow.
 * If @menu is NULL, the arrow button becomes insensitive.
 *
 * Since: 2.6
 **/
void
ctk_menu_tool_button_set_menu (GtkMenuToolButton *button,
                               GtkWidget         *menu)
{
  GtkMenuToolButtonPrivate *priv;

  g_return_if_fail (CTK_IS_MENU_TOOL_BUTTON (button));
  g_return_if_fail (CTK_IS_MENU (menu) || menu == NULL);

  priv = button->priv;

  _ctk_menu_button_set_popup_with_func (CTK_MENU_BUTTON (priv->arrow_button),
                                        menu,
                                        _show_menu_emit,
                                        button);

  g_object_notify (G_OBJECT (button), "menu");
}

/**
 * ctk_menu_tool_button_get_menu:
 * @button: a #GtkMenuToolButton
 *
 * Gets the #GtkMenu associated with #GtkMenuToolButton.
 *
 * Returns: (transfer none): the #GtkMenu associated
 *     with #GtkMenuToolButton
 *
 * Since: 2.6
 **/
GtkWidget *
ctk_menu_tool_button_get_menu (GtkMenuToolButton *button)
{
  GtkMenu *ret;

  g_return_val_if_fail (CTK_IS_MENU_TOOL_BUTTON (button), NULL);

  ret = ctk_menu_button_get_popup (CTK_MENU_BUTTON (button->priv->arrow_button));
  if (!ret)
    return NULL;

  return CTK_WIDGET (ret);
}

/**
 * ctk_menu_tool_button_set_arrow_tooltip_text:
 * @button: a #GtkMenuToolButton
 * @text: text to be used as tooltip text for button’s arrow button
 *
 * Sets the tooltip text to be used as tooltip for the arrow button which
 * pops up the menu.  See ctk_tool_item_set_tooltip_text() for setting a tooltip
 * on the whole #GtkMenuToolButton.
 *
 * Since: 2.12
 **/
void
ctk_menu_tool_button_set_arrow_tooltip_text (GtkMenuToolButton *button,
					     const gchar       *text)
{
  g_return_if_fail (CTK_IS_MENU_TOOL_BUTTON (button));

  ctk_widget_set_tooltip_text (button->priv->arrow_button, text);
}

/**
 * ctk_menu_tool_button_set_arrow_tooltip_markup:
 * @button: a #GtkMenuToolButton
 * @markup: markup text to be used as tooltip text for button’s arrow button
 *
 * Sets the tooltip markup text to be used as tooltip for the arrow button
 * which pops up the menu.  See ctk_tool_item_set_tooltip_text() for setting
 * a tooltip on the whole #GtkMenuToolButton.
 *
 * Since: 2.12
 **/
void
ctk_menu_tool_button_set_arrow_tooltip_markup (GtkMenuToolButton *button,
					       const gchar       *markup)
{
  g_return_if_fail (CTK_IS_MENU_TOOL_BUTTON (button));

  ctk_widget_set_tooltip_markup (button->priv->arrow_button, markup);
}
