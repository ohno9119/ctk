/* CTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
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

#include "ctkcssshadowsvalueprivate.h"

#include <math.h>

#include "ctkcairoblurprivate.h"
#include "ctkcssshadowvalueprivate.h"

#include <string.h>


struct _CtkCssValue {
  CTK_CSS_VALUE_BASE
  guint         len;
  CtkCssValue  *values[1];
};

static CtkCssValue *    ctk_css_shadows_value_new       (CtkCssValue **values,
                                                         guint         len);

static void
ctk_css_value_shadows_free (CtkCssValue *value)
{
  guint i;

  for (i = 0; i < value->len; i++)
    {
      _ctk_css_value_unref (value->values[i]);
    }

  g_slice_free1 (sizeof (CtkCssValue) + sizeof (CtkCssValue *) * (value->len - 1), value);
}

static CtkCssValue *
ctk_css_value_shadows_compute (CtkCssValue             *value,
                               guint                    property_id,
                               CtkStyleProviderPrivate *provider,
                               CtkCssStyle             *style,
                               CtkCssStyle             *parent_style)
{
  CtkCssValue *result, *tmp;
  guint i, j;

  if (value->len == 0)
    return _ctk_css_value_ref (value);

  result = NULL;
  for (i = 0; i < value->len; i++)
    {
      tmp = _ctk_css_value_compute (value->values[i], property_id, provider, style, parent_style);

      if (result)
        {
          result->values[i] = tmp;
        }
      else if (tmp != value->values[i])
        {
          result = ctk_css_shadows_value_new (value->values, value->len);
          for (j = 0; j < i; j++)
            {
              _ctk_css_value_ref (result->values[j]);
            }
          result->values[i] = tmp;
        }
      else
        {
          _ctk_css_value_unref (tmp);
        }
    }

  if (result != NULL)
    return result;
  else
    return _ctk_css_value_ref (value);
}

static gboolean
ctk_css_value_shadows_equal (const CtkCssValue *value1,
                             const CtkCssValue *value2)
{
  guint i;

  /* XXX: Should we fill up here? */
  if (value1->len != value2->len)
    return FALSE;

  for (i = 0; i < value1->len; i++)
    {
      if (!_ctk_css_value_equal (value1->values[i],
                                 value2->values[i]))
        return FALSE;
    }

  return TRUE;
}

static CtkCssValue *
ctk_css_value_shadows_transition (CtkCssValue *start,
                                  CtkCssValue *end,
                                  guint        property_id,
                                  double       progress)
{
  guint i, len;
  CtkCssValue **values;

  /* catches the important case of 2 none values */
  if (start == end)
    return _ctk_css_value_ref (start);

  if (start->len > end->len)
    len = start->len;
  else
    len = end->len;

  values = g_newa (CtkCssValue *, len);

  for (i = 0; i < MIN (start->len, end->len); i++)
    {
      values[i] = _ctk_css_value_transition (start->values[i], end->values[i], property_id, progress);
      if (values[i] == NULL)
        {
          while (i--)
            _ctk_css_value_unref (values[i]);
          return NULL;
        }
    }
  if (start->len > end->len)
    {
      for (; i < len; i++)
        {
          CtkCssValue *fill = _ctk_css_shadow_value_new_for_transition (start->values[i]);
          values[i] = _ctk_css_value_transition (start->values[i], fill, property_id, progress);
          _ctk_css_value_unref (fill);

          if (values[i] == NULL)
            {
              while (i--)
                _ctk_css_value_unref (values[i]);
              return NULL;
            }
        }
    }
  else
    {
      for (; i < len; i++)
        {
          CtkCssValue *fill = _ctk_css_shadow_value_new_for_transition (end->values[i]);
          values[i] = _ctk_css_value_transition (fill, end->values[i], property_id, progress);
          _ctk_css_value_unref (fill);

          if (values[i] == NULL)
            {
              while (i--)
                _ctk_css_value_unref (values[i]);
              return NULL;
            }
        }
    }

  return ctk_css_shadows_value_new (values, len);
}

static void
ctk_css_value_shadows_print (const CtkCssValue *value,
                             GString           *string)
{
  guint i;

  if (value->len == 0)
    {
      g_string_append (string, "none");
      return;
    }

  for (i = 0; i < value->len; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      _ctk_css_value_print (value->values[i], string);
    }
}

static const CtkCssValueClass CTK_CSS_VALUE_SHADOWS = {
  ctk_css_value_shadows_free,
  ctk_css_value_shadows_compute,
  ctk_css_value_shadows_equal,
  ctk_css_value_shadows_transition,
  ctk_css_value_shadows_print
};

static CtkCssValue none_singleton = { &CTK_CSS_VALUE_SHADOWS, 1, 0, { NULL } };

CtkCssValue *
_ctk_css_shadows_value_new_none (void)
{
  return _ctk_css_value_ref (&none_singleton);
}

static CtkCssValue *
ctk_css_shadows_value_new (CtkCssValue **values,
                           guint         len)
{
  CtkCssValue *result;
           
  g_return_val_if_fail (values != NULL, NULL);
  g_return_val_if_fail (len > 0, NULL);
         
  result = _ctk_css_value_alloc (&CTK_CSS_VALUE_SHADOWS, sizeof (CtkCssValue) + sizeof (CtkCssValue *) * (len - 1));
  result->len = len;
  memcpy (&result->values[0], values, sizeof (CtkCssValue *) * len);
            
  return result;
}

CtkCssValue *
_ctk_css_shadows_value_parse (CtkCssParser *parser,
                              gboolean      box_shadow_mode)
{
  CtkCssValue *result;
  GPtrArray *values;

  if (_ctk_css_parser_try (parser, "none", TRUE))
    return _ctk_css_shadows_value_new_none ();

  values = g_ptr_array_new ();

  do {
    CtkCssValue *value;

    value = _ctk_css_shadow_value_parse (parser, box_shadow_mode);

    if (value == NULL)
      {
        g_ptr_array_set_free_func (values, (GDestroyNotify) _ctk_css_value_unref);
        g_ptr_array_free (values, TRUE);
        return NULL;
      }

    g_ptr_array_add (values, value);
  } while (_ctk_css_parser_try (parser, ",", TRUE));

  result = ctk_css_shadows_value_new ((CtkCssValue **) values->pdata, values->len);
  g_ptr_array_free (values, TRUE);
  return result;
}

gboolean
_ctk_css_shadows_value_is_none (const CtkCssValue *shadows)
{
  g_return_val_if_fail (shadows->class == &CTK_CSS_VALUE_SHADOWS, TRUE);

  return shadows->len == 0;
}

void
_ctk_css_shadows_value_paint_layout (const CtkCssValue *shadows,
                                     cairo_t           *cr,
                                     PangoLayout       *layout)
{
  guint i;

  g_return_if_fail (shadows->class == &CTK_CSS_VALUE_SHADOWS);

  for (i = 0; i < shadows->len; i++)
    {
      _ctk_css_shadow_value_paint_layout (shadows->values[i], cr, layout);
    }
}

void
_ctk_css_shadows_value_paint_icon (const CtkCssValue *shadows,
                                   cairo_t           *cr)
{
  guint i;

  g_return_if_fail (shadows->class == &CTK_CSS_VALUE_SHADOWS);

  for (i = 0; i < shadows->len; i++)
    {
      _ctk_css_shadow_value_paint_icon (shadows->values[i], cr);
    }
}

void
_ctk_css_shadows_value_paint_box (const CtkCssValue   *shadows,
                                  cairo_t             *cr,
                                  const CtkRoundedBox *padding_box,
                                  gboolean             inset)
{
  guint i;

  g_return_if_fail (shadows->class == &CTK_CSS_VALUE_SHADOWS);

  for (i = 0; i < shadows->len; i++)
    {
      if (inset == _ctk_css_shadow_value_get_inset (shadows->values[i]))
        _ctk_css_shadow_value_paint_box (shadows->values[i], cr, padding_box);
    }
}

void
_ctk_css_shadows_value_get_extents (const CtkCssValue *shadows,
                                    CtkBorder         *border)
{
  guint i;
  CtkBorder b = { 0 };
  gdouble hoffset, voffset, spread, radius, clip_radius;

  g_return_if_fail (shadows->class == &CTK_CSS_VALUE_SHADOWS);

  *border = b;

  for (i = 0; i < shadows->len; i++)
    {
      const CtkCssValue *shadow;

      shadow = shadows->values[i];

      if (_ctk_css_shadow_value_get_inset (shadow))
        continue;

      _ctk_css_shadow_value_get_geometry (shadow,
                                          &hoffset, &voffset,
                                          &radius, &spread);
      clip_radius = _ctk_cairo_blur_compute_pixels (radius);

      b.top = MAX (0, ceil (clip_radius + spread - voffset));
      b.right = MAX (0, ceil (clip_radius + spread + hoffset));
      b.bottom = MAX (0, ceil (clip_radius + spread + voffset));
      b.left = MAX (0, ceil (clip_radius + spread - hoffset));

      border->top = MAX (border->top, b.top);
      border->right = MAX (border->right, b.right);
      border->bottom = MAX (border->bottom, b.bottom);
      border->left = MAX (border->left, b.left);
    }
}
