/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010 Red Hat, Inc.
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

#ifndef __GDK_WIN32_KEYS_H__
#define __GDK_WIN32_KEYS_H__

#if !defined (__GDKWIN32_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <cdk/cdkwin32.h> can be included directly."
#endif

#include <cdk/cdk.h>

G_BEGIN_DECLS

/**
 * CdkWin32KeymapMatch:
 * @GDK_WIN32_KEYMAP_MATCH_NONE: no matches found. Output is not valid.
 * @GDK_WIN32_KEYMAP_MATCH_INCOMPLETE: the sequence matches so far, but is incomplete. Output is not valid.
 * @GDK_WIN32_KEYMAP_MATCH_PARTIAL: the sequence matches up to the last key,
 *     which does not match. Output is valid.
 * @GDK_WIN32_KEYMAP_MATCH_EXACT: the sequence matches exactly. Output is valid.
 *
 * An enumeration describing the result of a deadkey combination matching.
 */
typedef enum
{
  GDK_WIN32_KEYMAP_MATCH_NONE,
  GDK_WIN32_KEYMAP_MATCH_INCOMPLETE,
  GDK_WIN32_KEYMAP_MATCH_PARTIAL,
  GDK_WIN32_KEYMAP_MATCH_EXACT
} CdkWin32KeymapMatch;

#ifdef GDK_COMPILATION
typedef struct _CdkWin32Keymap CdkWin32Keymap;
#else
typedef CdkKeymap CdkWin32Keymap;
#endif
typedef struct _CdkWin32KeymapClass CdkWin32KeymapClass;

#define GDK_TYPE_WIN32_KEYMAP              (cdk_win32_keymap_get_type())
#define GDK_WIN32_KEYMAP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WIN32_KEYMAP, CdkWin32Keymap))
#define GDK_WIN32_KEYMAP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_KEYMAP, CdkWin32KeymapClass))
#define GDK_IS_WIN32_KEYMAP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WIN32_KEYMAP))
#define GDK_IS_WIN32_KEYMAP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_KEYMAP))
#define GDK_WIN32_KEYMAP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_KEYMAP, CdkWin32KeymapClass))

GDK_AVAILABLE_IN_ALL
GType cdk_win32_keymap_get_type (void);

GDK_AVAILABLE_IN_3_20
CdkWin32KeymapMatch cdk_win32_keymap_check_compose (CdkWin32Keymap *keymap,
                                                    guint16        *compose_buffer,
                                                    gsize           compose_buffer_len,
                                                    guint16        *output,
                                                    gsize          *output_len);

G_END_DECLS

#endif /* __GDK_WIN32_KEYMAP_H__ */
