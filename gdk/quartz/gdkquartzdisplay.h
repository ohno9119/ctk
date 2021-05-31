/* gdkquartzdisplay.h
 *
 * Copyright (C) 2005-2007  Imendio AB
 * Copyright (C) 2010 Kristian Rietveld  <kris@ctk.org>
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

#ifndef __GDK_QUARTZ_DISPLAY_H__
#define __GDK_QUARTZ_DISPLAY_H__

#if !defined(__GDKQUARTZ_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkquartz.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GDK_TYPE_QUARTZ_DISPLAY              (gdk_quartz_display_get_type ())
#define GDK_QUARTZ_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_QUARTZ_DISPLAY, GdkQuartzDisplay))
#define GDK_QUARTZ_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_QUARTZ_DISPLAY, GdkQuartzDisplayClass))
#define GDK_IS_QUARTZ_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_QUARTZ_DISPLAY))
#define GDK_IS_QUARTZ_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_QUARTZ_DISPLAY))
#define GDK_QUARTZ_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_QUARTZ_DISPLAY, GdkQuartzDisplayClass))

#ifdef GDK_COMPILATION
typedef struct _GdkQuartzDisplay GdkQuartzDisplay;
#else
typedef GdkDisplay GdkQuartzDisplay;
#endif
typedef struct _GdkQuartzDisplayClass GdkQuartzDisplayClass;


GDK_AVAILABLE_IN_ALL
GType gdk_quartz_display_get_type (void);

G_END_DECLS

#endif /* __GDK_QUARTZ_DISPLAY_H__ */
