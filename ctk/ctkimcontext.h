/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat Software
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

#ifndef __CTK_IM_CONTEXT_H__
#define __CTK_IM_CONTEXT_H__


#if !defined (__CTK_H_INSIDE__) && !defined (CTK_COMPILATION)
#error "Only <ctk/ctk.h> can be included directly."
#endif

#include <gdk/gdk.h>


G_BEGIN_DECLS

#define CTK_TYPE_IM_CONTEXT              (ctk_im_context_get_type ())
#define CTK_IM_CONTEXT(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CTK_TYPE_IM_CONTEXT, GtkIMContext))
#define CTK_IM_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CTK_TYPE_IM_CONTEXT, GtkIMContextClass))
#define CTK_IS_IM_CONTEXT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CTK_TYPE_IM_CONTEXT))
#define CTK_IS_IM_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CTK_TYPE_IM_CONTEXT))
#define CTK_IM_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CTK_TYPE_IM_CONTEXT, GtkIMContextClass))


typedef struct _GtkIMContext       GtkIMContext;
typedef struct _GtkIMContextClass  GtkIMContextClass;

struct _GtkIMContext
{
  GObject parent_instance;
};

struct _GtkIMContextClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  /* Signals */
  void     (*preedit_start)        (GtkIMContext *context);
  void     (*preedit_end)          (GtkIMContext *context);
  void     (*preedit_changed)      (GtkIMContext *context);
  void     (*commit)               (GtkIMContext *context, const gchar *str);
  gboolean (*retrieve_surrounding) (GtkIMContext *context);
  gboolean (*delete_surrounding)   (GtkIMContext *context,
				    gint          offset,
				    gint          n_chars);

  /* Virtual functions */
  void     (*set_client_window)   (GtkIMContext   *context,
				   GdkWindow      *window);
  void     (*get_preedit_string)  (GtkIMContext   *context,
				   gchar         **str,
				   PangoAttrList **attrs,
				   gint           *cursor_pos);
  gboolean (*filter_keypress)     (GtkIMContext   *context,
			           GdkEventKey    *event);
  void     (*focus_in)            (GtkIMContext   *context);
  void     (*focus_out)           (GtkIMContext   *context);
  void     (*reset)               (GtkIMContext   *context);
  void     (*set_cursor_location) (GtkIMContext   *context,
				   GdkRectangle   *area);
  void     (*set_use_preedit)     (GtkIMContext   *context,
				   gboolean        use_preedit);
  void     (*set_surrounding)     (GtkIMContext   *context,
				   const gchar    *text,
				   gint            len,
				   gint            cursor_index);
  gboolean (*get_surrounding)     (GtkIMContext   *context,
				   gchar         **text,
				   gint           *cursor_index);
  /*< private >*/
  /* Padding for future expansion */
  void (*_ctk_reserved1) (void);
  void (*_ctk_reserved2) (void);
  void (*_ctk_reserved3) (void);
  void (*_ctk_reserved4) (void);
  void (*_ctk_reserved5) (void);
  void (*_ctk_reserved6) (void);
};

GDK_AVAILABLE_IN_ALL
GType    ctk_im_context_get_type            (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
void     ctk_im_context_set_client_window   (GtkIMContext       *context,
					     GdkWindow          *window);
GDK_AVAILABLE_IN_ALL
void     ctk_im_context_get_preedit_string  (GtkIMContext       *context,
					     gchar             **str,
					     PangoAttrList     **attrs,
					     gint               *cursor_pos);
GDK_AVAILABLE_IN_ALL
gboolean ctk_im_context_filter_keypress     (GtkIMContext       *context,
					     GdkEventKey        *event);
GDK_AVAILABLE_IN_ALL
void     ctk_im_context_focus_in            (GtkIMContext       *context);
GDK_AVAILABLE_IN_ALL
void     ctk_im_context_focus_out           (GtkIMContext       *context);
GDK_AVAILABLE_IN_ALL
void     ctk_im_context_reset               (GtkIMContext       *context);
GDK_AVAILABLE_IN_ALL
void     ctk_im_context_set_cursor_location (GtkIMContext       *context,
					     const GdkRectangle *area);
GDK_AVAILABLE_IN_ALL
void     ctk_im_context_set_use_preedit     (GtkIMContext       *context,
					     gboolean            use_preedit);
GDK_AVAILABLE_IN_ALL
void     ctk_im_context_set_surrounding     (GtkIMContext       *context,
					     const gchar        *text,
					     gint                len,
					     gint                cursor_index);
GDK_AVAILABLE_IN_ALL
gboolean ctk_im_context_get_surrounding     (GtkIMContext       *context,
					     gchar             **text,
					     gint               *cursor_index);
GDK_AVAILABLE_IN_ALL
gboolean ctk_im_context_delete_surrounding  (GtkIMContext       *context,
					     gint                offset,
					     gint                n_chars);

G_END_DECLS

#endif /* __CTK_IM_CONTEXT_H__ */
