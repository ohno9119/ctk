/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * Modified by the CTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the CTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * CTK+ at ftp://ftp.ctk.org/pub/ctk/. 
 */

#include "config.h"

#include "cdkdndprivate.h"

#include "cdkinternals.h"
#include "cdkproperty.h"
#include "cdkprivate-broadway.h"
#include "cdkinternals.h"
#include "cdkscreen-broadway.h"
#include "cdkdisplay-broadway.h"

#include <string.h>

#define GDK_TYPE_BROADWAY_DRAG_CONTEXT              (cdk_broadway_drag_context_get_type ())
#define GDK_BROADWAY_DRAG_CONTEXT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_BROADWAY_DRAG_CONTEXT, GdkBroadwayDragContext))
#define GDK_BROADWAY_DRAG_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_BROADWAY_DRAG_CONTEXT, GdkBroadwayDragContextClass))
#define GDK_IS_BROADWAY_DRAG_CONTEXT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_BROADWAY_DRAG_CONTEXT))
#define GDK_IS_BROADWAY_DRAG_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_BROADWAY_DRAG_CONTEXT))
#define GDK_BROADWAY_DRAG_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_BROADWAY_DRAG_CONTEXT, GdkBroadwayDragContextClass))

#ifdef GDK_COMPILATION
typedef struct _GdkBroadwayDragContext GdkBroadwayDragContext;
#else
typedef GdkDragContext GdkBroadwayDragContext;
#endif
typedef struct _GdkBroadwayDragContextClass GdkBroadwayDragContextClass;

GType     cdk_broadway_drag_context_get_type (void);

struct _GdkBroadwayDragContext {
  GdkDragContext context;
};

struct _GdkBroadwayDragContextClass
{
  GdkDragContextClass parent_class;
};

static void cdk_broadway_drag_context_finalize (GObject *object);

static GList *contexts;

G_DEFINE_TYPE (GdkBroadwayDragContext, cdk_broadway_drag_context, GDK_TYPE_DRAG_CONTEXT)

static void
cdk_broadway_drag_context_init (GdkBroadwayDragContext *dragcontext)
{
  contexts = g_list_prepend (contexts, dragcontext);
}

static void
cdk_broadway_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);

  contexts = g_list_remove (contexts, context);

  G_OBJECT_CLASS (cdk_broadway_drag_context_parent_class)->finalize (object);
}

/* Drag Contexts */

GdkDragContext *
_cdk_broadway_window_drag_begin (GdkWindow *window,
				 GdkDevice *device,
				 GList     *targets,
                                 gint       x_root,
                                 gint       y_root)
{
  GdkDragContext *new_context;

  g_return_val_if_fail (window != NULL, NULL);
  g_return_val_if_fail (GDK_WINDOW_IS_BROADWAY (window), NULL);

  new_context = g_object_new (GDK_TYPE_BROADWAY_DRAG_CONTEXT,
			      NULL);
  new_context->display = cdk_window_get_display (window);

  return new_context;
}

GdkDragProtocol
_cdk_broadway_window_get_drag_protocol (GdkWindow *window,
					GdkWindow **target)
{
  return GDK_DRAG_PROTO_NONE;
}

static GdkWindow *
cdk_broadway_drag_context_find_window (GdkDragContext  *context,
				       GdkWindow       *drag_window,
				       GdkScreen       *screen,
				       gint             x_root,
				       gint             y_root,
				       GdkDragProtocol *protocol)
{
  g_return_val_if_fail (context != NULL, NULL);
  return NULL;
}

static gboolean
cdk_broadway_drag_context_drag_motion (GdkDragContext *context,
				       GdkWindow      *dest_window,
				       GdkDragProtocol protocol,
				       gint            x_root,
				       gint            y_root,
				       GdkDragAction   suggested_action,
				       GdkDragAction   possible_actions,
				       guint32         time)
{
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (dest_window == NULL || GDK_WINDOW_IS_BROADWAY (dest_window), FALSE);

  return FALSE;
}

static void
cdk_broadway_drag_context_drag_drop (GdkDragContext *context,
				     guint32         time)
{
  g_return_if_fail (context != NULL);
}

static void
cdk_broadway_drag_context_drag_abort (GdkDragContext *context,
				      guint32         time)
{
  g_return_if_fail (context != NULL);
}

/* Destination side */

static void
cdk_broadway_drag_context_drag_status (GdkDragContext   *context,
				       GdkDragAction     action,
				       guint32           time)
{
  g_return_if_fail (context != NULL);
}

static void
cdk_broadway_drag_context_drop_reply (GdkDragContext   *context,
				      gboolean          ok,
				      guint32           time)
{
  g_return_if_fail (context != NULL);
}

static void
cdk_broadway_drag_context_drop_finish (GdkDragContext   *context,
				       gboolean          success,
				       guint32           time)
{
  g_return_if_fail (context != NULL);
}

void
_cdk_broadway_window_register_dnd (GdkWindow      *window)
{
}

static GdkAtom
cdk_broadway_drag_context_get_selection (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, GDK_NONE);

  return GDK_NONE;
}

static gboolean
cdk_broadway_drag_context_drop_status (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  return FALSE;
}

void
_cdk_broadway_display_init_dnd (GdkDisplay *display)
{
}

static void
cdk_broadway_drag_context_class_init (GdkBroadwayDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = cdk_broadway_drag_context_finalize;

  context_class->find_window = cdk_broadway_drag_context_find_window;
  context_class->drag_status = cdk_broadway_drag_context_drag_status;
  context_class->drag_motion = cdk_broadway_drag_context_drag_motion;
  context_class->drag_abort = cdk_broadway_drag_context_drag_abort;
  context_class->drag_drop = cdk_broadway_drag_context_drag_drop;
  context_class->drop_reply = cdk_broadway_drag_context_drop_reply;
  context_class->drop_finish = cdk_broadway_drag_context_drop_finish;
  context_class->drop_status = cdk_broadway_drag_context_drop_status;
  context_class->get_selection = cdk_broadway_drag_context_get_selection;
}
