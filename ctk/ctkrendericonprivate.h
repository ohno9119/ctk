/* CTK - The GIMP Toolkit
 * Copyright (C) 2014,2015 Benjamin Otte
 * 
 * Authors: Benjamin Otte <otte@gnome.org>
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

#ifndef __CTK_RENDER_ICON_PRIVATE_H__
#define __CTK_RENDER_ICON_PRIVATE_H__

#include <glib-object.h>
#include <cairo.h>

#include "ctkcsstypesprivate.h"
#include "ctktypes.h"

G_BEGIN_DECLS

void    ctk_css_style_render_icon               (CtkCssStyle            *style,
                                                 cairo_t                *cr,
                                                 double                  x,
                                                 double                  y,
                                                 double                  width,
                                                 double                  height,
                                                 CtkCssImageBuiltinType  builtin_type);

void    ctk_css_style_render_icon_surface       (CtkCssStyle            *style,
                                                 cairo_t                *cr,
                                                 cairo_surface_t        *surface,
                                                 double                  x,
                                                 double                  y);

void    ctk_css_style_render_icon_get_extents   (CtkCssStyle            *style,
                                                 GdkRectangle           *extents,
                                                 gint                    x,
                                                 gint                    y,
                                                 gint                    width,
                                                 gint                    height);

G_END_DECLS

#endif /* __CTK_RENDER_ICON_PRIVATE_H__ */
