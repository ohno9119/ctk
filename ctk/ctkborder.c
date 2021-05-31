/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.ctk.org/pub/ctk/. 
 */

#include "config.h"

#include "ctkborder.h"

/**
 * ctk_border_new:
 *
 * Allocates a new #GtkBorder-struct and initializes its elements to zero.
 *
 * Returns: (transfer full): a newly allocated #GtkBorder-struct.
 *  Free with ctk_border_free()
 *
 * Since: 2.14
 */
GtkBorder *
ctk_border_new (void)
{
  return g_slice_new0 (GtkBorder);
}

/**
 * ctk_border_copy:
 * @border_: a #GtkBorder-struct
 *
 * Copies a #GtkBorder-struct.
 *
 * Returns: (transfer full): a copy of @border_.
 */
GtkBorder *
ctk_border_copy (const GtkBorder *border_)
{
  g_return_val_if_fail (border_ != NULL, NULL);

  return g_slice_dup (GtkBorder, border_);
}

/**
 * ctk_border_free:
 * @border_: a #GtkBorder-struct
 *
 * Frees a #GtkBorder-struct.
 */
void
ctk_border_free (GtkBorder *border_)
{
  g_slice_free (GtkBorder, border_);
}

G_DEFINE_BOXED_TYPE (GtkBorder, ctk_border,
                     ctk_border_copy,
                     ctk_border_free)
