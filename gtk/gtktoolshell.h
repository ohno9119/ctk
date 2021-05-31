/* GTK - The GIMP Toolkit
 * Copyright (C) 2007  Openismus GmbH
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
 *
 * Author:
 *   Mathias Hasselmann
 */

#ifndef __CTK_TOOL_SHELL_H__
#define __CTK_TOOL_SHELL_H__


#if !defined (__CTK_H_INSIDE__) && !defined (CTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkenums.h>
#include <pango/pango.h>
#include <gtk/gtksizegroup.h>


G_BEGIN_DECLS

#define CTK_TYPE_TOOL_SHELL            (ctk_tool_shell_get_type ())
#define CTK_TOOL_SHELL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CTK_TYPE_TOOL_SHELL, GtkToolShell))
#define CTK_IS_TOOL_SHELL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CTK_TYPE_TOOL_SHELL))
#define CTK_TOOL_SHELL_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CTK_TYPE_TOOL_SHELL, GtkToolShellIface))

typedef struct _GtkToolShell           GtkToolShell; /* dummy typedef */
typedef struct _GtkToolShellIface      GtkToolShellIface;

/**
 * GtkToolShellIface:
 * @get_icon_size:        mandatory implementation of ctk_tool_shell_get_icon_size().
 * @get_orientation:      mandatory implementation of ctk_tool_shell_get_orientation().
 * @get_style:            mandatory implementation of ctk_tool_shell_get_style().
 * @get_relief_style:     optional implementation of ctk_tool_shell_get_relief_style().
 * @rebuild_menu:         optional implementation of ctk_tool_shell_rebuild_menu().
 * @get_text_orientation: optional implementation of ctk_tool_shell_get_text_orientation().
 * @get_text_alignment:   optional implementation of ctk_tool_shell_get_text_alignment().
 * @get_ellipsize_mode:   optional implementation of ctk_tool_shell_get_ellipsize_mode().
 * @get_text_size_group:  optional implementation of ctk_tool_shell_get_text_size_group().
 *
 * Virtual function table for the #GtkToolShell interface.
 */
struct _GtkToolShellIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  GtkIconSize        (*get_icon_size)        (GtkToolShell *shell);
  GtkOrientation     (*get_orientation)      (GtkToolShell *shell);
  GtkToolbarStyle    (*get_style)            (GtkToolShell *shell);
  GtkReliefStyle     (*get_relief_style)     (GtkToolShell *shell);
  void               (*rebuild_menu)         (GtkToolShell *shell);
  GtkOrientation     (*get_text_orientation) (GtkToolShell *shell);
  gfloat             (*get_text_alignment)   (GtkToolShell *shell);
  PangoEllipsizeMode (*get_ellipsize_mode)   (GtkToolShell *shell);
  GtkSizeGroup *     (*get_text_size_group)  (GtkToolShell *shell);
};

GDK_AVAILABLE_IN_ALL
GType              ctk_tool_shell_get_type             (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkIconSize        ctk_tool_shell_get_icon_size        (GtkToolShell *shell);
GDK_AVAILABLE_IN_ALL
GtkOrientation     ctk_tool_shell_get_orientation      (GtkToolShell *shell);
GDK_AVAILABLE_IN_ALL
GtkToolbarStyle    ctk_tool_shell_get_style            (GtkToolShell *shell);
GDK_AVAILABLE_IN_ALL
GtkReliefStyle     ctk_tool_shell_get_relief_style     (GtkToolShell *shell);
GDK_AVAILABLE_IN_ALL
void               ctk_tool_shell_rebuild_menu         (GtkToolShell *shell);
GDK_AVAILABLE_IN_ALL
GtkOrientation     ctk_tool_shell_get_text_orientation (GtkToolShell *shell);
GDK_AVAILABLE_IN_ALL
gfloat             ctk_tool_shell_get_text_alignment   (GtkToolShell *shell);
GDK_AVAILABLE_IN_ALL
PangoEllipsizeMode ctk_tool_shell_get_ellipsize_mode   (GtkToolShell *shell);
GDK_AVAILABLE_IN_ALL
GtkSizeGroup *     ctk_tool_shell_get_text_size_group  (GtkToolShell *shell);

G_END_DECLS

#endif /* __CTK_TOOL_SHELL_H__ */
