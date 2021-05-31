/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#include "ctkstylepropertiesprivate.h"

#include <stdlib.h>
#include <gobject/gvaluecollector.h>
#include <cairo-gobject.h>

#include "ctkstyleprovider.h"
#include "ctkcssshorthandpropertyprivate.h"
#include "ctkcsstypedvalueprivate.h"
#include "ctkcsstypesprivate.h"

#include "ctkprivatetypebuiltins.h"
#include "ctkstylepropertyprivate.h"
#include "ctkstyleproviderprivate.h"
#include "ctkintl.h"

#include "deprecated/ctkthemingengine.h"
#include "deprecated/ctkgradient.h"
#include "deprecated/ctksymboliccolorprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

/**
 * SECTION:ctkstyleproperties
 * @Short_description: Store for style property information
 * @Title: GtkStyleProperties
 *
 * GtkStyleProperties provides the storage for style information
 * that is used by #GtkStyleContext and other #GtkStyleProvider
 * implementations.
 *
 * Before style properties can be stored in GtkStyleProperties, they
 * must be registered with ctk_style_properties_register_property().
 *
 * Unless you are writing a #GtkStyleProvider implementation, you
 * are unlikely to use this API directly, as ctk_style_context_get()
 * and its variants are the preferred way to access styling information
 * from widget implementations and theming engine implementations
 * should use the APIs provided by #GtkThemingEngine instead.
 *
 * #GtkStyleProperties has been deprecated in GTK 3.16. The CSS
 * machinery does not use it anymore and all users of this object
 * have been deprecated.
 */

typedef struct PropertyData PropertyData;
typedef struct ValueData ValueData;

struct ValueData
{
  GtkStateFlags state;
  GtkCssValue *value;
};

struct PropertyData
{
  GArray *values;
};

struct _GtkStylePropertiesPrivate
{
  GHashTable *color_map;
  GHashTable *properties;
};

static void ctk_style_properties_provider_init         (GtkStyleProviderIface            *iface);
static void ctk_style_properties_provider_private_init (GtkStyleProviderPrivateInterface *iface);
static void ctk_style_properties_finalize              (GObject                          *object);


G_DEFINE_TYPE_WITH_CODE (GtkStyleProperties, ctk_style_properties, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkStyleProperties)
                         G_IMPLEMENT_INTERFACE (CTK_TYPE_STYLE_PROVIDER,
                                                ctk_style_properties_provider_init)
                         G_IMPLEMENT_INTERFACE (CTK_TYPE_STYLE_PROVIDER_PRIVATE,
                                                ctk_style_properties_provider_private_init));

static void
ctk_style_properties_class_init (GtkStylePropertiesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ctk_style_properties_finalize;
}

static PropertyData *
property_data_new (void)
{
  PropertyData *data;

  data = g_slice_new0 (PropertyData);
  data->values = g_array_new (FALSE, FALSE, sizeof (ValueData));

  return data;
}

static void
property_data_remove_values (PropertyData *data)
{
  guint i;

  for (i = 0; i < data->values->len; i++)
    {
      ValueData *value_data;

      value_data = &g_array_index (data->values, ValueData, i);

      _ctk_css_value_unref (value_data->value);
      value_data->value = NULL;
    }

  if (data->values->len > 0)
    g_array_remove_range (data->values, 0, data->values->len);
}

static void
property_data_free (PropertyData *data)
{
  property_data_remove_values (data);
  g_array_free (data->values, TRUE);
  g_slice_free (PropertyData, data);
}

static gboolean
property_data_find_position (PropertyData  *data,
                             GtkStateFlags  state,
                             guint         *pos)
{
  gint min, max, mid;
  gboolean found = FALSE;
  guint position;

  if (pos)
    *pos = 0;

  if (data->values->len == 0)
    return FALSE;

  /* Find position for the given state, or the position where
   * it would be if not found, the array is ordered by the
   * state flags.
   */
  min = 0;
  max = data->values->len - 1;

  do
    {
      ValueData *value_data;

      mid = (min + max) / 2;
      value_data = &g_array_index (data->values, ValueData, mid);

      if (value_data->state == state)
        {
          found = TRUE;
          position = mid;
        }
      else if (value_data->state < state)
          position = min = mid + 1;
      else
        {
          max = mid - 1;
          position = mid;
        }
    }
  while (!found && min <= max);

  if (pos)
    *pos = position;

  return found;
}

static ValueData *
property_data_get_value (PropertyData  *data,
                         GtkStateFlags  state)
{
  guint pos;

  if (!property_data_find_position (data, state, &pos))
    {
      ValueData new = { 0 };

      new.state = state;
      g_array_insert_val (data->values, pos, new);
    }

  return &g_array_index (data->values, ValueData, pos);
}

static GtkCssValue *
property_data_match_state (PropertyData  *data,
                           GtkStateFlags  state)
{
  guint pos;
  gint i;

  if (property_data_find_position (data, state, &pos))
    {
      ValueData *val_data;

      /* Exact match */
      val_data = &g_array_index (data->values, ValueData, pos);
      return val_data->value;
    }

  if (pos >= data->values->len)
    pos = data->values->len - 1;

  /* No exact match, go downwards the list to find
   * the closest match to the given state flags, as
   * a side effect, there is an implicit precedence
   * of higher flags over the smaller ones.
   */
  for (i = pos; i >= 0; i--)
    {
      ValueData *val_data;

      val_data = &g_array_index (data->values, ValueData, i);

       /* Check whether any of the requested
        * flags are set, and no other flags are.
        *
        * Also, no flags acts as a wildcard, such
        * value should be always in the first position
        * in the array (if present) anyways.
        */
      if (val_data->state == 0 ||
          ((val_data->state & state) != 0 &&
           (val_data->state & ~state) == 0))
        return val_data->value;
    }

  return NULL;
}

static void
ctk_style_properties_init (GtkStyleProperties *props)
{
  props->priv = ctk_style_properties_get_instance_private (props);
  props->priv->properties = g_hash_table_new_full (NULL, NULL, NULL,
                                                   (GDestroyNotify) property_data_free);
}

static void
ctk_style_properties_finalize (GObject *object)
{
  GtkStylePropertiesPrivate *priv;
  GtkStyleProperties *props;

  props = CTK_STYLE_PROPERTIES (object);
  priv = props->priv;
  g_hash_table_destroy (priv->properties);

  if (priv->color_map)
    g_hash_table_destroy (priv->color_map);

  G_OBJECT_CLASS (ctk_style_properties_parent_class)->finalize (object);
}

static void
ctk_style_properties_provider_init (GtkStyleProviderIface *iface)
{
}

static GtkCssValue *
ctk_style_properties_provider_get_color (GtkStyleProviderPrivate *provider,
                                         const char              *name)
{
  GtkSymbolicColor *symbolic;

  symbolic = ctk_style_properties_lookup_color (CTK_STYLE_PROPERTIES (provider), name);
  if (symbolic == NULL)
    return NULL;

  return _ctk_symbolic_color_get_css_value (symbolic);
}

static void
ctk_style_properties_provider_lookup (GtkStyleProviderPrivate *provider,
                                      const GtkCssMatcher     *matcher,
                                      GtkCssLookup            *lookup,
                                      GtkCssChange            *change)
{
  GtkStyleProperties *props;
  GtkStylePropertiesPrivate *priv;
  GHashTableIter iter;
  gpointer key, value;

  props = CTK_STYLE_PROPERTIES (provider);
  priv = props->priv;

  /* Merge symbolic style properties */
  g_hash_table_iter_init (&iter, priv->properties);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GtkCssStyleProperty *prop = key;
      PropertyData *data = value;
      GtkCssValue *val;
      guint id;

      id = _ctk_css_style_property_get_id (prop);

      if (!_ctk_css_lookup_is_missing (lookup, id))
          continue;

      val = property_data_match_state (data, _ctk_css_matcher_get_state (matcher));
      if (val == NULL)
        continue;

      _ctk_css_lookup_set (lookup, id, NULL, val);
    }

  if (change)
    *change = CTK_CSS_CHANGE_STATE;
}

static void
ctk_style_properties_provider_private_init (GtkStyleProviderPrivateInterface *iface)
{
  iface->get_color = ctk_style_properties_provider_get_color;
  iface->lookup = ctk_style_properties_provider_lookup;
}

/* GtkStyleProperties methods */

/**
 * ctk_style_properties_new:
 *
 * Returns a newly created #GtkStyleProperties
 *
 * Returns: a new #GtkStyleProperties
 *
 * Deprecated: 3.16: #GtkStyleProperties are deprecated.
 **/
GtkStyleProperties *
ctk_style_properties_new (void)
{
  return g_object_new (CTK_TYPE_STYLE_PROPERTIES, NULL);
}

/**
 * ctk_style_properties_map_color:
 * @props: a #GtkStyleProperties
 * @name: color name
 * @color: #GtkSymbolicColor to map @name to
 *
 * Maps @color so it can be referenced by @name. See
 * ctk_style_properties_lookup_color()
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
void
ctk_style_properties_map_color (GtkStyleProperties *props,
                                const gchar        *name,
                                GtkSymbolicColor   *color)
{
  GtkStylePropertiesPrivate *priv;

  g_return_if_fail (CTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (name != NULL);
  g_return_if_fail (color != NULL);

  priv = props->priv;

  if (G_UNLIKELY (!priv->color_map))
    priv->color_map = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             (GDestroyNotify) g_free,
                                             (GDestroyNotify) ctk_symbolic_color_unref);

  g_hash_table_replace (priv->color_map,
                        g_strdup (name),
                        ctk_symbolic_color_ref (color));

  _ctk_style_provider_private_changed (CTK_STYLE_PROVIDER_PRIVATE (props));
}

/**
 * ctk_style_properties_lookup_color:
 * @props: a #GtkStyleProperties
 * @name: color name to lookup
 *
 * Returns the symbolic color that is mapped
 * to @name.
 *
 * Returns: (transfer none): The mapped color
 *
 * Since: 3.0
 *
 * Deprecated: 3.8: #GtkSymbolicColor is deprecated.
 **/
GtkSymbolicColor *
ctk_style_properties_lookup_color (GtkStyleProperties *props,
                                   const gchar        *name)
{
  GtkStylePropertiesPrivate *priv;

  g_return_val_if_fail (CTK_IS_STYLE_PROPERTIES (props), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  priv = props->priv;

  if (!priv->color_map)
    return NULL;

  return g_hash_table_lookup (priv->color_map, name);
}

void
_ctk_style_properties_set_property_by_property (GtkStyleProperties  *props,
                                                GtkCssStyleProperty *style_prop,
                                                GtkStateFlags        state,
                                                GtkCssValue         *value)
{
  GtkStylePropertiesPrivate *priv;
  PropertyData *prop;
  ValueData *val;

  priv = props->priv;
  prop = g_hash_table_lookup (priv->properties, style_prop);

  if (!prop)
    {
      prop = property_data_new ();
      g_hash_table_insert (priv->properties, (gpointer) style_prop, prop);
    }

  val = property_data_get_value (prop, state);

  _ctk_css_value_unref (val->value);
  val->value = _ctk_css_value_ref (value);

  _ctk_style_provider_private_changed (CTK_STYLE_PROVIDER_PRIVATE (props));
}

/**
 * ctk_style_properties_set_property:
 * @props: a #GtkStyleProperties
 * @property: styling property to set
 * @state: state to set the value for
 * @value: new value for the property
 *
 * Sets a styling property in @props.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: #GtkStyleProperties are deprecated.
 **/
void
ctk_style_properties_set_property (GtkStyleProperties *props,
                                   const gchar        *property,
                                   GtkStateFlags       state,
                                   const GValue       *value)
{
  GtkStyleProperty *node;

  g_return_if_fail (CTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  node = _ctk_style_property_lookup (property);

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }
  if (_ctk_style_property_get_value_type (node) == G_TYPE_NONE)
    {
      g_warning ("Style property \"%s\" is not settable", property);
      return;
    }
  
  _ctk_style_property_assign (node, props, state, value);
}

/**
 * ctk_style_properties_set_valist:
 * @props: a #GtkStyleProperties
 * @state: state to set the values for
 * @args: va_list of property name/value pairs, followed by %NULL
 *
 * Sets several style properties on @props.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: #GtkStyleProperties are deprecated.
 **/
void
ctk_style_properties_set_valist (GtkStyleProperties *props,
                                 GtkStateFlags       state,
                                 va_list             args)
{
  const gchar *property_name;

  g_return_if_fail (CTK_IS_STYLE_PROPERTIES (props));

  property_name = va_arg (args, const gchar *);

  while (property_name)
    {
      GtkStyleProperty *node;
      gchar *error = NULL;
      GType val_type;
      GValue val = G_VALUE_INIT;

      node = _ctk_style_property_lookup (property_name);

      if (!node)
        {
          g_warning ("Style property \"%s\" is not registered", property_name);
          break;
        }

      val_type = _ctk_style_property_get_value_type (node);
      if (val_type == G_TYPE_NONE)
        {
          g_warning ("Style property \"%s\" is not settable", property_name);
          break;
        }

      G_VALUE_COLLECT_INIT (&val, _ctk_style_property_get_value_type (node),
                            args, 0, &error);
      if (error)
        {
          g_warning ("Could not set style property \"%s\": %s", property_name, error);
          g_value_unset (&val);
          g_free (error);
          break;
        }

      _ctk_style_property_assign (node, props, state, &val);
      g_value_unset (&val);

      property_name = va_arg (args, const gchar *);
    }
}

/**
 * ctk_style_properties_set:
 * @props: a #GtkStyleProperties
 * @state: state to set the values for
 * @...: property name/value pairs, followed by %NULL
 *
 * Sets several style properties on @props.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: #GtkStyleProperties are deprecated.
 **/
void
ctk_style_properties_set (GtkStyleProperties *props,
                          GtkStateFlags       state,
                          ...)
{
  va_list args;

  g_return_if_fail (CTK_IS_STYLE_PROPERTIES (props));

  va_start (args, state);
  ctk_style_properties_set_valist (props, state, args);
  va_end (args);
}

typedef struct {
  GtkStyleProperties *props;
  GtkStateFlags       state;
} StyleQueryData;

static GtkCssValue *
style_query_func (guint    id,
                  gpointer data)
{
  StyleQueryData *query = data;
  PropertyData *prop;

  prop = g_hash_table_lookup (query->props->priv->properties,
                              _ctk_css_style_property_lookup_by_id (id));
  if (prop == NULL)
    return NULL;

  return property_data_match_state (prop, query->state);
}

/**
 * ctk_style_properties_get_property:
 * @props: a #GtkStyleProperties
 * @property: style property name
 * @state: state to retrieve the property value for
 * @value: (out) (transfer full):  return location for the style property value.
 *
 * Gets a style property from @props for the given state. When done with @value,
 * g_value_unset() needs to be called to free any allocated memory.
 *
 * Returns: %TRUE if the property exists in @props, %FALSE otherwise
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: #GtkStyleProperties are deprecated.
 **/
gboolean
ctk_style_properties_get_property (GtkStyleProperties *props,
                                   const gchar        *property,
                                   GtkStateFlags       state,
                                   GValue             *value)
{
  StyleQueryData query = { props, state };
  GtkStyleProperty *node;

  g_return_val_if_fail (CTK_IS_STYLE_PROPERTIES (props), FALSE);
  g_return_val_if_fail (property != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  node = _ctk_style_property_lookup (property);
  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return FALSE;
    }
  if (_ctk_style_property_get_value_type (node) == G_TYPE_NONE)
    {
      g_warning ("Style property \"%s\" is not gettable", property);
      return FALSE;
    }

  _ctk_style_property_query (node,
                             value,
                             style_query_func,
                             &query);

  return TRUE;
}

/**
 * ctk_style_properties_get_valist:
 * @props: a #GtkStyleProperties
 * @state: state to retrieve the property values for
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several style property values from @props for a given state.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: #GtkStyleProperties are deprecated.
 **/
void
ctk_style_properties_get_valist (GtkStyleProperties *props,
                                 GtkStateFlags       state,
                                 va_list             args)
{
  const gchar *property_name;

  g_return_if_fail (CTK_IS_STYLE_PROPERTIES (props));

  property_name = va_arg (args, const gchar *);

  while (property_name)
    {
      gchar *error = NULL;
      GValue value = G_VALUE_INIT;

      if (!ctk_style_properties_get_property (props,
					      property_name,
					      state,
					      &value))
	break;

      G_VALUE_LCOPY (&value, args, 0, &error);
      g_value_unset (&value);

      if (error)
        {
          g_warning ("Could not get style property \"%s\": %s", property_name, error);
          g_free (error);
          break;
        }

      property_name = va_arg (args, const gchar *);
    }
}

/**
 * ctk_style_properties_get:
 * @props: a #GtkStyleProperties
 * @state: state to retrieve the property values for
 * @...: property name /return value pairs, followed by %NULL
 *
 * Retrieves several style property values from @props for a
 * given state.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: #GtkStyleProperties are deprecated.
 **/
void
ctk_style_properties_get (GtkStyleProperties *props,
                          GtkStateFlags       state,
                          ...)
{
  va_list args;

  g_return_if_fail (CTK_IS_STYLE_PROPERTIES (props));

  va_start (args, state);
  ctk_style_properties_get_valist (props, state, args);
  va_end (args);
}

/**
 * ctk_style_properties_unset_property:
 * @props: a #GtkStyleProperties
 * @property: property to unset
 * @state: state to unset
 *
 * Unsets a style property in @props.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: #GtkStyleProperties are deprecated.
 **/
void
ctk_style_properties_unset_property (GtkStyleProperties *props,
                                     const gchar        *property,
                                     GtkStateFlags       state)
{
  GtkStylePropertiesPrivate *priv;
  GtkStyleProperty *node;
  PropertyData *prop;
  guint pos;

  g_return_if_fail (CTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (property != NULL);

  node = _ctk_style_property_lookup (property);

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }
  if (_ctk_style_property_get_value_type (node) == G_TYPE_NONE)
    {
      g_warning ("Style property \"%s\" is not settable", property);
      return;
    }

  if (CTK_IS_CSS_SHORTHAND_PROPERTY (node))
    {
      GtkCssShorthandProperty *shorthand = CTK_CSS_SHORTHAND_PROPERTY (node);

      for (pos = 0; pos < _ctk_css_shorthand_property_get_n_subproperties (shorthand); pos++)
        {
          GtkCssStyleProperty *sub = _ctk_css_shorthand_property_get_subproperty (shorthand, pos);
          ctk_style_properties_unset_property (props,
                                               _ctk_style_property_get_name (CTK_STYLE_PROPERTY (sub)),
                                               state);
        }
      return;
    }

  priv = props->priv;
  prop = g_hash_table_lookup (priv->properties, node);

  if (!prop)
    return;

  if (property_data_find_position (prop, state, &pos))
    {
      ValueData *data;

      data = &g_array_index (prop->values, ValueData, pos);

      _ctk_css_value_unref (data->value);
      data->value = NULL;

      g_array_remove_index (prop->values, pos);

      _ctk_style_provider_private_changed (CTK_STYLE_PROVIDER_PRIVATE (props));
    }
}

/**
 * ctk_style_properties_clear:
 * @props: a #GtkStyleProperties
 *
 * Clears all style information from @props.
 *
 * Deprecated: 3.16: #GtkStyleProperties are deprecated.
 **/
void
ctk_style_properties_clear (GtkStyleProperties *props)
{
  GtkStylePropertiesPrivate *priv;

  g_return_if_fail (CTK_IS_STYLE_PROPERTIES (props));

  priv = props->priv;
  g_hash_table_remove_all (priv->properties);

  _ctk_style_provider_private_changed (CTK_STYLE_PROVIDER_PRIVATE (props));
}

/**
 * ctk_style_properties_merge:
 * @props: a #GtkStyleProperties
 * @props_to_merge: a second #GtkStyleProperties
 * @replace: whether to replace values or not
 *
 * Merges into @props all the style information contained
 * in @props_to_merge. If @replace is %TRUE, the values
 * will be overwritten, if it is %FALSE, the older values
 * will prevail.
 *
 * Since: 3.0
 *
 * Deprecated: 3.16: #GtkStyleProperties are deprecated.
 **/
void
ctk_style_properties_merge (GtkStyleProperties       *props,
                            const GtkStyleProperties *props_to_merge,
                            gboolean                  replace)
{
  GtkStylePropertiesPrivate *priv, *priv_to_merge;
  GHashTableIter iter;
  gpointer key, val;

  g_return_if_fail (CTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (CTK_IS_STYLE_PROPERTIES (props_to_merge));

  priv = props->priv;
  priv_to_merge = props_to_merge->priv;

  /* Merge symbolic color map */
  if (priv_to_merge->color_map)
    {
      g_hash_table_iter_init (&iter, priv_to_merge->color_map);

      while (g_hash_table_iter_next (&iter, &key, &val))
        {
          const gchar *name;
          GtkSymbolicColor *color;

          name = key;
          color = val;

          if (!replace &&
              g_hash_table_lookup (priv->color_map, name))
            continue;

          ctk_style_properties_map_color (props, name, color);
        }
    }

  /* Merge symbolic style properties */
  g_hash_table_iter_init (&iter, priv_to_merge->properties);

  while (g_hash_table_iter_next (&iter, &key, &val))
    {
      PropertyData *prop_to_merge = val;
      PropertyData *prop;
      guint i, j;

      prop = g_hash_table_lookup (priv->properties, key);

      if (!prop)
        {
          prop = property_data_new ();
          g_hash_table_insert (priv->properties, key, prop);
        }

      for (i = 0; i < prop_to_merge->values->len; i++)
        {
          ValueData *data;
          ValueData *value;

          data = &g_array_index (prop_to_merge->values, ValueData, i);

          if (replace && data->state == CTK_STATE_FLAG_NORMAL &&
              _ctk_is_css_typed_value_of_type (data->value, PANGO_TYPE_FONT_DESCRIPTION))
            {
              /* Let normal state override all states
               * previously set in the original set
               */
              property_data_remove_values (prop);
            }

          value = property_data_get_value (prop, data->state);

          if (_ctk_is_css_typed_value_of_type (data->value, PANGO_TYPE_FONT_DESCRIPTION) &&
              value->value != NULL)
            {
              PangoFontDescription *font_desc;
              PangoFontDescription *font_desc_to_merge;

              /* Handle merging of font descriptions */
              font_desc = g_value_get_boxed (_ctk_css_typed_value_get (value->value));
              font_desc_to_merge = g_value_get_boxed (_ctk_css_typed_value_get (data->value));

              pango_font_description_merge (font_desc, font_desc_to_merge, replace);
            }
          else if (_ctk_is_css_typed_value_of_type (data->value, G_TYPE_PTR_ARRAY) &&
                   value->value != NULL)
            {
              GPtrArray *array, *array_to_merge;

              /* Append the array, mainly thought
               * for the ctk-key-bindings property
               */
              array = g_value_get_boxed (_ctk_css_typed_value_get (value->value));
              array_to_merge = g_value_get_boxed (_ctk_css_typed_value_get (data->value));

              for (j = 0; j < array_to_merge->len; j++)
                g_ptr_array_add (array, g_ptr_array_index (array_to_merge, j));
            }
          else if (replace || value->value == NULL)
            {
	      _ctk_css_value_unref (value->value);
	      value->value = _ctk_css_value_ref (data->value);
            }
        }
    }

  _ctk_style_provider_private_changed (CTK_STYLE_PROVIDER_PRIVATE (props));
}

G_GNUC_END_IGNORE_DEPRECATIONS;
