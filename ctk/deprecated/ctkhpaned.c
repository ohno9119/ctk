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

#include "ctkhpaned.h"
#include "ctkorientable.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * SECTION:ctkhpaned
 * @Short_description: A container with two panes arranged horizontally
 * @Title: GtkHPaned
 *
 * The HPaned widget is a container widget with two
 * children arranged horizontally. The division between
 * the two panes is adjustable by the user by dragging
 * a handle. See #GtkPaned for details.
 *
 * GtkHPaned has been deprecated, use #GtkPaned instead.
 */


G_DEFINE_TYPE (GtkHPaned, ctk_hpaned, CTK_TYPE_PANED)

static void
ctk_hpaned_class_init (GtkHPanedClass *class)
{
}

static void
ctk_hpaned_init (GtkHPaned *hpaned)
{
  ctk_orientable_set_orientation (CTK_ORIENTABLE (hpaned),
                                  CTK_ORIENTATION_HORIZONTAL);
}

/**
 * ctk_hpaned_new:
 *
 * Create a new #GtkHPaned
 *
 * Returns: the new #GtkHPaned
 *
 * Deprecated: 3.2: Use ctk_paned_new() with %CTK_ORIENTATION_HORIZONTAL instead
 */
GtkWidget *
ctk_hpaned_new (void)
{
  return g_object_new (CTK_TYPE_HPANED, NULL);
}
