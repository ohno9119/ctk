/* HSV color selector for GTK+
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Simon Budig <Simon.Budig@unix-ag.org> (original code)
 *          Federico Mena-Quintero <federico@gimp.org> (cleanup for GTK+)
 *          Jonathan Blandford <jrb@redhat.com> (cleanup for GTK+)
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

#ifndef __CTK_HSV_H__
#define __CTK_HSV_H__

#if !defined (__CTK_H_INSIDE__) && !defined (CTK_COMPILATION)
#error "Only <ctk/ctk.h> can be included directly."
#endif

#include <ctk/ctkwidget.h>

G_BEGIN_DECLS

#define CTK_TYPE_HSV            (ctk_hsv_get_type ())
#define CTK_HSV(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CTK_TYPE_HSV, GtkHSV))
#define CTK_HSV_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CTK_TYPE_HSV, GtkHSVClass))
#define CTK_IS_HSV(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CTK_TYPE_HSV))
#define CTK_IS_HSV_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CTK_TYPE_HSV))
#define CTK_HSV_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CTK_TYPE_HSV, GtkHSVClass))


typedef struct _GtkHSV              GtkHSV;
typedef struct _GtkHSVPrivate       GtkHSVPrivate;
typedef struct _GtkHSVClass         GtkHSVClass;

struct _GtkHSV
{
  GtkWidget parent_instance;

  /*< private >*/
  GtkHSVPrivate *priv;
};

struct _GtkHSVClass
{
  GtkWidgetClass parent_class;

  /* Notification signals */
  void (* changed) (GtkHSV          *hsv);

  /* Keybindings */
  void (* move)    (GtkHSV          *hsv,
                    GtkDirectionType type);

  /* Padding for future expansion */
  void (*_ctk_reserved1) (void);
  void (*_ctk_reserved2) (void);
  void (*_ctk_reserved3) (void);
  void (*_ctk_reserved4) (void);
};


GDK_DEPRECATED_IN_3_4
GType      ctk_hsv_get_type     (void) G_GNUC_CONST;
GDK_DEPRECATED_IN_3_4
GtkWidget* ctk_hsv_new          (void);
GDK_DEPRECATED_IN_3_4
void       ctk_hsv_set_color    (GtkHSV    *hsv,
				 double     h,
				 double     s,
				 double     v);
GDK_DEPRECATED_IN_3_4
void       ctk_hsv_get_color    (GtkHSV    *hsv,
				 gdouble   *h,
				 gdouble   *s,
				 gdouble   *v);
GDK_DEPRECATED_IN_3_4
void       ctk_hsv_set_metrics  (GtkHSV    *hsv,
				 gint       size,
				 gint       ring_width);
GDK_DEPRECATED_IN_3_4
void       ctk_hsv_get_metrics  (GtkHSV    *hsv,
				 gint      *size,
				 gint      *ring_width);
GDK_DEPRECATED_IN_3_4
gboolean   ctk_hsv_is_adjusting (GtkHSV    *hsv);

G_END_DECLS

#endif /* __CTK_HSV_H__ */
