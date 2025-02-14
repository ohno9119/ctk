/*
 * Copyright © 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <fcntl.h>
#include <unistd.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>

#include "cdkwayland.h"
#include "cdkprivate-wayland.h"
#include "cdkdisplay-wayland.h"
#include "cdkdndprivate.h"
#include "cdkselection.h"
#include "cdkproperty.h"
#include "cdkprivate.h"

#include <string.h>

#include "fallback-memdup.h"

typedef struct _SelectionBuffer SelectionBuffer;
typedef struct _SelectionData SelectionData;
typedef struct _StoredSelection StoredSelection;
typedef struct _AsyncWriteData AsyncWriteData;
typedef struct _DataOfferData DataOfferData;

struct _SelectionBuffer
{
  GInputStream *stream;
  GCancellable *cancellable;
  GByteArray *data;
  GList *requestors;
  CdkAtom selection;
  CdkAtom target;
  gint ref_count;
};

struct _StoredSelection
{
  CdkWaylandSelection *selection;
  CdkWindow *source;
  GCancellable *cancellable;
  guchar *data;
  gsize data_len;
  CdkAtom type;
  CdkAtom selection_atom;
  GPtrArray *pending_writes; /* Array of AsyncWriteData */
};

struct _DataOfferData
{
  GDestroyNotify destroy_notify;
  gpointer offer_data;
  GList *targets; /* List of CdkAtom */
};

struct _AsyncWriteData
{
  GOutputStream *stream;
  StoredSelection *stored_selection;
  gsize index;
};

struct _SelectionData
{
  DataOfferData *offer;
  GHashTable *buffers; /* Hashtable of target_atom->SelectionBuffer */
};

enum {
  ATOM_PRIMARY,
  ATOM_CLIPBOARD,
  ATOM_DND,
  N_ATOMS
};

static CdkAtom atoms[N_ATOMS] = { 0 };

struct _CdkWaylandSelection
{
  /* Destination-side data */
  SelectionData selections[N_ATOMS];
  GHashTable *offers; /* Currently alive offers, Hashtable of wl_data_offer->DataOfferData */

  /* Source-side data */
  GPtrArray *stored_selections; /* Array of StoredSelection */
  StoredSelection *current_request_selection;
  GArray *source_targets;
  CdkAtom requested_target;

  gpointer primary_source;
  CdkWindow *primary_owner;

  struct wl_data_source *clipboard_source;
  CdkWindow *clipboard_owner;

  struct wl_data_source *dnd_source; /* Owned by the CdkDragContext */
  CdkWindow *dnd_owner;
};

static void selection_buffer_read (SelectionBuffer *buffer);
static void async_write_data_write (AsyncWriteData *write_data);
static void async_write_data_free (AsyncWriteData *write_data);
static void emit_selection_clear (CdkDisplay *display, CdkAtom selection);
static void emit_empty_selection_notify (CdkWindow *requestor,
                                         CdkAtom    selection,
                                         CdkAtom    target);
static void cdk_wayland_selection_handle_next_request (CdkWaylandSelection *wayland_selection);

static void
selection_buffer_notify (SelectionBuffer *buffer)
{
  GList *l;

  for (l = buffer->requestors; l; l = l->next)
    {
      CdkEvent *event;

      event = cdk_event_new (CDK_SELECTION_NOTIFY);
      event->selection.window = g_object_ref (l->data);
      event->selection.send_event = FALSE;
      event->selection.selection = buffer->selection;
      event->selection.target = buffer->target;
      event->selection.property = cdk_atom_intern_static_string ("CDK_SELECTION");
      event->selection.time = CDK_CURRENT_TIME;
      event->selection.requestor = g_object_ref (l->data);

      cdk_event_put (event);
      cdk_event_free (event);
    }
}

static SelectionBuffer *
selection_buffer_new (GInputStream *stream,
                      CdkAtom       selection,
                      CdkAtom       target)
{
  SelectionBuffer *buffer;

  buffer = g_new0 (SelectionBuffer, 1);
  buffer->stream = (stream) ? g_object_ref (stream) : NULL;
  buffer->cancellable = g_cancellable_new ();
  buffer->data = g_byte_array_new ();
  buffer->selection = selection;
  buffer->target = target;
  buffer->ref_count = 1;

  if (stream)
    selection_buffer_read (buffer);

  return buffer;
}

static SelectionBuffer *
selection_buffer_ref (SelectionBuffer *buffer)
{
  buffer->ref_count++;
  return buffer;
}

static void
selection_buffer_unref (SelectionBuffer *buffer_data)
{
  GList *l;

  buffer_data->ref_count--;

  if (buffer_data->ref_count != 0)
    return;

  for (l = buffer_data->requestors; l; l = l->next)
    {
      emit_empty_selection_notify (l->data, buffer_data->selection,
                                   buffer_data->target);
    }

  g_list_free (buffer_data->requestors);
  buffer_data->requestors = NULL;

  if (buffer_data->cancellable)
    g_object_unref (buffer_data->cancellable);

  if (buffer_data->stream)
    g_object_unref (buffer_data->stream);

  if (buffer_data->data)
    g_byte_array_unref (buffer_data->data);

  g_free (buffer_data);
}

static void
selection_buffer_append_data (SelectionBuffer *buffer,
                              gconstpointer    data,
                              gsize            len)
{
  g_byte_array_append (buffer->data, data, len);
}

static void
selection_buffer_cancel_and_unref (SelectionBuffer *buffer_data)
{
  if (buffer_data->cancellable)
    g_cancellable_cancel (buffer_data->cancellable);

  selection_buffer_unref (buffer_data);
}

static void
selection_buffer_add_requestor (SelectionBuffer *buffer,
                                CdkWindow       *requestor)
{
  if (!g_list_find (buffer->requestors, requestor))
    buffer->requestors = g_list_prepend (buffer->requestors,
                                         g_object_ref (requestor));
}

static gboolean
selection_buffer_remove_requestor (SelectionBuffer *buffer,
                                   CdkWindow       *requestor)
{
  GList *link = g_list_find (buffer->requestors, requestor);

  if (!link)
    return FALSE;

  g_object_unref (link->data);
  buffer->requestors = g_list_delete_link (buffer->requestors, link);
  return TRUE;
}

static inline glong
get_buffer_size (void)
{
  return sysconf (_SC_PAGESIZE);
}

static void
selection_buffer_read_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  SelectionBuffer *buffer = user_data;
  gboolean finished = TRUE;
  GError *error = NULL;
  GBytes *bytes;

  bytes = g_input_stream_read_bytes_finish (buffer->stream, result, &error);

  if (bytes)
    {
      finished = g_bytes_get_size (bytes) == 0;
      selection_buffer_append_data (buffer,
                                    g_bytes_get_data (bytes, NULL),
                                    g_bytes_get_size (bytes));
      g_bytes_unref (bytes);
    }

  if (!finished)
    selection_buffer_read (buffer);
  else
    {
      if (error)
        {
          g_warning (G_STRLOC ": error reading selection buffer: %s", error->message);
          g_error_free (error);
        }
      else
        selection_buffer_notify (buffer);

      g_input_stream_close (buffer->stream, NULL, NULL);
      g_clear_object (&buffer->stream);
      g_clear_object (&buffer->cancellable);
    }

  selection_buffer_unref (buffer);
}

static void
selection_buffer_read (SelectionBuffer *buffer)
{
  selection_buffer_ref (buffer);
  g_input_stream_read_bytes_async (buffer->stream, get_buffer_size(),
                                   G_PRIORITY_DEFAULT,
                                   buffer->cancellable, selection_buffer_read_cb,
                                   buffer);
}

static DataOfferData *
data_offer_data_new (gpointer       offer,
                     GDestroyNotify destroy_notify)
{
  DataOfferData *info;

  info = g_slice_new0 (DataOfferData);
  info->offer_data = offer;
  info->destroy_notify = destroy_notify;

  return info;
}

static void
data_offer_data_free (DataOfferData *info)
{
  info->destroy_notify (info->offer_data);
  g_list_free (info->targets);
  g_slice_free (DataOfferData, info);
}

static StoredSelection *
stored_selection_new (CdkWaylandSelection *wayland_selection,
                      CdkWindow           *source,
                      CdkAtom              selection,
                      CdkAtom              type)
{
  StoredSelection *stored_selection;

  stored_selection = g_new0 (StoredSelection, 1);
  stored_selection->source = source;
  stored_selection->type = type;
  stored_selection->selection_atom = selection;
  stored_selection->selection = wayland_selection;
  stored_selection->cancellable = g_cancellable_new ();
  stored_selection->pending_writes =
    g_ptr_array_new_with_free_func ((GDestroyNotify) async_write_data_free);

  return stored_selection;
}

static void
stored_selection_add_data (StoredSelection *stored_selection,
                           CdkPropMode      mode,
                           guchar          *data,
                           gsize            data_len)
{
  if (mode == CDK_PROP_MODE_REPLACE)
    {
      g_free (stored_selection->data);
      stored_selection->data = g_memdup2 (data, data_len);
      stored_selection->data_len = data_len;
    }
  else
    {
      GArray *array;

      array = g_array_new (TRUE, TRUE, sizeof (guchar));
      g_array_append_vals (array, stored_selection->data, stored_selection->data_len);

      if (mode == CDK_PROP_MODE_APPEND)
        g_array_append_vals (array, data, data_len);
      else if (mode == CDK_PROP_MODE_PREPEND)
        g_array_prepend_vals (array, data, data_len);

      g_free (stored_selection->data);
      stored_selection->data_len = array->len;
      stored_selection->data = (guchar *) g_array_free (array, FALSE);
    }
}

static void
stored_selection_free (StoredSelection *stored_selection)
{
  g_cancellable_cancel (stored_selection->cancellable);
  g_object_unref (stored_selection->cancellable);
  g_ptr_array_unref (stored_selection->pending_writes);
  g_free (stored_selection->data);
  g_free (stored_selection);
}

static void
stored_selection_notify_write (StoredSelection *stored_selection)
{
  gint i;

  for (i = 0; i < stored_selection->pending_writes->len; i++)
    {
      AsyncWriteData *write_data;

      write_data = g_ptr_array_index (stored_selection->pending_writes, i);
      async_write_data_write (write_data);
    }
}

static void
stored_selection_cancel_write (StoredSelection *stored_selection)
{
  g_cancellable_cancel (stored_selection->cancellable);
  g_object_unref (stored_selection->cancellable);
  stored_selection->cancellable = g_cancellable_new ();
  g_ptr_array_set_size (stored_selection->pending_writes, 0);
}

CdkWaylandSelection *
cdk_wayland_selection_new (void)
{
  CdkWaylandSelection *selection;
  gint i;

  /* init atoms */
  atoms[ATOM_PRIMARY] = cdk_atom_intern_static_string ("PRIMARY");
  atoms[ATOM_CLIPBOARD] = cdk_atom_intern_static_string ("CLIPBOARD");
  atoms[ATOM_DND] = cdk_atom_intern_static_string ("CdkWaylandSelection");

  selection = g_new0 (CdkWaylandSelection, 1);
  for (i = 0; i < G_N_ELEMENTS (selection->selections); i++)
    {
      selection->selections[i].buffers =
        g_hash_table_new_full (NULL, NULL, NULL,
                               (GDestroyNotify) selection_buffer_cancel_and_unref);
    }

  selection->offers =
    g_hash_table_new_full (NULL, NULL, NULL,
                           (GDestroyNotify) data_offer_data_free);
  selection->stored_selections =
    g_ptr_array_new_with_free_func ((GDestroyNotify) stored_selection_free);

  selection->source_targets = g_array_new (FALSE, FALSE, sizeof (CdkAtom));
  return selection;
}

static void
primary_selection_source_destroy (gpointer primary_source)
{
  CdkDisplay *display = cdk_display_get_default ();
  CdkWaylandDisplay *display_wayland = CDK_WAYLAND_DISPLAY (display);

  if (display_wayland->zwp_primary_selection_manager_v1)
    zwp_primary_selection_source_v1_destroy (primary_source);
  else if (display_wayland->ctk_primary_selection_manager)
    ctk_primary_selection_source_destroy (primary_source);
}

void
cdk_wayland_selection_free (CdkWaylandSelection *selection)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (selection->selections); i++)
    g_hash_table_destroy (selection->selections[i].buffers);

  g_array_unref (selection->source_targets);

  g_hash_table_destroy (selection->offers);
  g_ptr_array_unref (selection->stored_selections);

  if (selection->primary_source)
    primary_selection_source_destroy (selection->primary_source);
  if (selection->clipboard_source)
    wl_data_source_destroy (selection->clipboard_source);
  if (selection->dnd_source)
    wl_data_source_destroy (selection->dnd_source);

  g_free (selection);
}

static void
data_offer_offer (void                 *data,
                  struct wl_data_offer *wl_data_offer,
                  const char           *type)
{
  CdkWaylandSelection *selection = data;
  DataOfferData *info;
  CdkAtom atom = cdk_atom_intern (type, FALSE);

  info = g_hash_table_lookup (selection->offers, wl_data_offer);

  if (!info || g_list_find (info->targets, atom))
    return;

  CDK_NOTE (EVENTS,
            g_message ("data offer offer, offer %p, type = %s", wl_data_offer, type));

  info->targets = g_list_prepend (info->targets, atom);
}

static inline CdkDragAction
_wl_to_cdk_actions (uint32_t dnd_actions)
{
  CdkDragAction actions = 0;

  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    actions |= CDK_ACTION_COPY;
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    actions |= CDK_ACTION_MOVE;
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    actions |= CDK_ACTION_ASK;

  return actions;
}

static void
data_offer_source_actions (void                 *data,
                           struct wl_data_offer *wl_data_offer,
                           uint32_t              source_actions)
{
  CdkDragContext *drop_context;
  CdkDisplay *display;
  CdkDevice *device;
  CdkSeat *seat;

  display = cdk_display_get_default ();
  seat = cdk_display_get_default_seat (display);
  device = cdk_seat_get_pointer (seat);
  drop_context = cdk_wayland_device_get_drop_context (device);

  drop_context->actions = _wl_to_cdk_actions (source_actions);

  CDK_NOTE (EVENTS,
            g_message ("data offer source actions, offer %p, actions %d", wl_data_offer, source_actions));

  if (cdk_drag_context_get_dest_window (drop_context))
    _cdk_wayland_drag_context_emit_event (drop_context, CDK_DRAG_MOTION,
                                          CDK_CURRENT_TIME);
}

static void
data_offer_action (void                 *data,
                   struct wl_data_offer *wl_data_offer,
                   uint32_t              action)
{
  CdkDragContext *drop_context;
  CdkDisplay *display;
  CdkDevice *device;
  CdkSeat *seat;

  display = cdk_display_get_default ();
  seat = cdk_display_get_default_seat (display);
  device = cdk_seat_get_pointer (seat);
  drop_context = cdk_wayland_device_get_drop_context (device);

  drop_context->action = _wl_to_cdk_actions (action);

  if (cdk_drag_context_get_dest_window (drop_context))
    _cdk_wayland_drag_context_emit_event (drop_context, CDK_DRAG_MOTION,
                                          CDK_CURRENT_TIME);
}

static const struct wl_data_offer_listener data_offer_listener = {
  data_offer_offer,
  data_offer_source_actions,
  data_offer_action
};

static void
primary_offer_offer (void       *data,
                     gpointer    offer,
                     const char *type)
{
  CdkWaylandSelection *selection = data;
  DataOfferData *info;
  CdkAtom atom = cdk_atom_intern (type, FALSE);

  info = g_hash_table_lookup (selection->offers, offer);

  if (!info || g_list_find (info->targets, atom))
    return;

  CDK_NOTE (EVENTS,
            g_message ("primary offer offer, offer %p, type = %s", offer, type));

  info->targets = g_list_prepend (info->targets, atom);
}

static void
ctk_primary_offer_offer (void                               *data,
                         struct ctk_primary_selection_offer *offer,
                         const char                         *type)
{
  primary_offer_offer (data, (gpointer) offer, type);
}

static void
zwp_primary_offer_v1_offer (void                                  *data,
                            struct zwp_primary_selection_offer_v1 *offer,
                            const char                            *type)
{
  primary_offer_offer (data, (gpointer) offer, type);
}

static const struct ctk_primary_selection_offer_listener ctk_primary_offer_listener = {
  ctk_primary_offer_offer,
};

static const struct zwp_primary_selection_offer_v1_listener zwp_primary_offer_listener_v1 = {
  zwp_primary_offer_v1_offer,
};

SelectionData *
selection_lookup_offer_by_atom (CdkWaylandSelection *selection,
                                CdkAtom              selection_atom)
{
  if (selection_atom == atoms[ATOM_PRIMARY])
    return &selection->selections[ATOM_PRIMARY];
  else if (selection_atom == atoms[ATOM_CLIPBOARD])
    return &selection->selections[ATOM_CLIPBOARD];
  else if (selection_atom == atoms[ATOM_DND])
    return &selection->selections[ATOM_DND];
  else
    return NULL;
}

void
cdk_wayland_selection_ensure_offer (CdkDisplay           *display,
                                    struct wl_data_offer *wl_offer)
{
  CdkWaylandSelection *selection = cdk_wayland_display_get_selection (display);
  DataOfferData *info;

  info = g_hash_table_lookup (selection->offers, wl_offer);

  if (!info)
    {
      info = data_offer_data_new (wl_offer,
                                  (GDestroyNotify) wl_data_offer_destroy);
      g_hash_table_insert (selection->offers, wl_offer, info);
      wl_data_offer_add_listener (wl_offer,
                                  &data_offer_listener,
                                  selection);
    }
}

void
cdk_wayland_selection_ensure_primary_offer (CdkDisplay *display,
                                            gpointer    ctk_offer)
{
  CdkWaylandDisplay *display_wayland = CDK_WAYLAND_DISPLAY (display);
  CdkWaylandSelection *selection = cdk_wayland_display_get_selection (display);
  DataOfferData *info;

  info = g_hash_table_lookup (selection->offers, ctk_offer);

  if (!info)
    {
      if (display_wayland->zwp_primary_selection_manager_v1)
        {
          info = data_offer_data_new (ctk_offer,
                                      (GDestroyNotify) zwp_primary_selection_offer_v1_destroy);
          g_hash_table_insert (selection->offers, ctk_offer, info);
          zwp_primary_selection_offer_v1_add_listener (ctk_offer,
                                                       &zwp_primary_offer_listener_v1,
                                                       selection);
        }
      else if (display_wayland->ctk_primary_selection_manager)
        {
          info = data_offer_data_new (ctk_offer,
                                      (GDestroyNotify) ctk_primary_selection_offer_destroy);
          g_hash_table_insert (selection->offers, ctk_offer, info);
          ctk_primary_selection_offer_add_listener (ctk_offer,
                                                    &ctk_primary_offer_listener,
                                                    selection);
        }
    }
}

void
cdk_wayland_selection_set_offer (CdkDisplay *display,
                                 CdkAtom     selection_atom,
                                 gpointer    wl_offer)
{
  CdkWaylandSelection *selection = cdk_wayland_display_get_selection (display);
  struct wl_data_offer *prev_offer;
  SelectionData *selection_data;
  DataOfferData *info;

  info = g_hash_table_lookup (selection->offers, wl_offer);

  prev_offer = cdk_wayland_selection_get_offer (display, selection_atom);

  if (prev_offer)
    g_hash_table_remove (selection->offers, prev_offer);

  selection_data = selection_lookup_offer_by_atom (selection, selection_atom);

  if (selection_data)
    {
      selection_data->offer = info;
      /* Clear all buffers */
      g_hash_table_remove_all (selection_data->buffers);
    }
}

gpointer
cdk_wayland_selection_get_offer (CdkDisplay *display,
                                 CdkAtom     selection_atom)
{
  CdkWaylandSelection *selection = cdk_wayland_display_get_selection (display);
  const SelectionData *data;

  data = selection_lookup_offer_by_atom (selection, selection_atom);

  if (data && data->offer)
    return data->offer->offer_data;

  return NULL;
}

GList *
cdk_wayland_selection_get_targets (CdkDisplay *display,
                                   CdkAtom     selection_atom)
{
  CdkWaylandSelection *selection = cdk_wayland_display_get_selection (display);
  const SelectionData *data;

  data = selection_lookup_offer_by_atom (selection, selection_atom);

  if (data && data->offer)
    return data->offer->targets;

  return NULL;
}

static void
cdk_wayland_selection_emit_request (CdkWindow *window,
                                    CdkAtom    selection,
                                    CdkAtom    target)
{
  CdkEvent *event;

  event = cdk_event_new (CDK_SELECTION_REQUEST);
  event->selection.window = g_object_ref (window);
  event->selection.send_event = FALSE;
  event->selection.selection = selection;
  event->selection.target = target;
  event->selection.property = cdk_atom_intern_static_string ("CDK_SELECTION");
  event->selection.time = CDK_CURRENT_TIME;
  event->selection.requestor = g_object_ref (window);

  cdk_event_put (event);
  cdk_event_free (event);
}

static AsyncWriteData *
async_write_data_new (StoredSelection *stored_selection,
                      gint             fd)
{
  AsyncWriteData *write_data;

  write_data = g_slice_new0 (AsyncWriteData);
  write_data->stored_selection = stored_selection;
  write_data->stream = g_unix_output_stream_new (fd, TRUE);
  g_ptr_array_add (stored_selection->pending_writes, write_data);

  return write_data;
}

static void
async_write_data_free (AsyncWriteData *write_data)
{
  g_object_unref (write_data->stream);
  g_slice_free (AsyncWriteData, write_data);
}

static void
async_write_data_cb (GObject      *object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  AsyncWriteData *write_data = user_data;
  GError *error = NULL;
  gsize bytes_written;

  bytes_written = g_output_stream_write_finish (G_OUTPUT_STREAM (object),
                                                res, &error);
  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Error writing selection data: %s", error->message);
          g_ptr_array_remove_fast (write_data->stored_selection->pending_writes,
                                   write_data);
        }

      g_error_free (error);
      return;
    }

  write_data->index += bytes_written;

  if (write_data->index < write_data->stored_selection->data_len)
    {
      /* Write the next chunk */
      async_write_data_write (write_data);
    }
  else
    {
      g_ptr_array_remove_fast (write_data->stored_selection->pending_writes,
                               write_data);
    }
}

static void
async_write_data_write (AsyncWriteData *write_data)
{
  gsize buf_len;
  guchar *buf;

  buf = write_data->stored_selection->data;
  buf_len = write_data->stored_selection->data_len;

  g_output_stream_write_async (write_data->stream,
                               &buf[write_data->index],
                               buf_len - write_data->index,
                               G_PRIORITY_DEFAULT,
                               write_data->stored_selection->cancellable,
                               async_write_data_cb,
                               write_data);
}

static StoredSelection *
cdk_wayland_selection_find_stored_selection (CdkWaylandSelection *wayland_selection,
                                             CdkWindow           *window,
                                             CdkAtom              selection,
                                             CdkAtom              type)
{
  gint i;

  for (i = 0; i < wayland_selection->stored_selections->len; i++)
    {
      StoredSelection *stored_selection;

      stored_selection = g_ptr_array_index (wayland_selection->stored_selections, i);

      if (stored_selection->source == window &&
          stored_selection->selection_atom == selection &&
          stored_selection->type == type)
        return stored_selection;
    }

  return NULL;
}

static void
cdk_wayland_selection_reset_selection (CdkWaylandSelection *wayland_selection,
                                       CdkAtom              selection)
{
  gint i = 0;

  while (i < wayland_selection->stored_selections->len)
    {
      StoredSelection *stored_selection;

      stored_selection = g_ptr_array_index (wayland_selection->stored_selections, i);

      if (stored_selection->selection_atom == selection)
        {
          if (wayland_selection->current_request_selection == stored_selection)
            wayland_selection->current_request_selection = NULL;

          g_ptr_array_remove_index_fast (wayland_selection->stored_selections, i);
        }
      else
        i++;
    }
}

void
cdk_wayland_selection_store (CdkWindow    *window,
                             CdkAtom       type,
                             CdkPropMode   mode,
                             const guchar *data,
                             gint          len)
{
  CdkDisplay *display = cdk_window_get_display (window);
  CdkWaylandSelection *selection = cdk_wayland_display_get_selection (display);
  StoredSelection *stored_selection;

  if (type == cdk_atom_intern_static_string ("NULL"))
    return;
  if (!selection->current_request_selection)
    return;

  stored_selection = selection->current_request_selection;

  if ((mode == CDK_PROP_MODE_PREPEND ||
       mode == CDK_PROP_MODE_REPLACE) &&
      stored_selection->data &&
      stored_selection->pending_writes->len > 0)
    {
      /* If a prepend/replace action happens, all current readers are
       * pretty much stale.
       */
      stored_selection_cancel_write (stored_selection);
    }

  stored_selection_add_data (stored_selection, mode, data, len);

  if (stored_selection->data)
    stored_selection_notify_write (stored_selection);
  else
    {
      g_ptr_array_remove_fast (selection->stored_selections,
                               stored_selection);
    }

  /* Handle the next CDK_SELECTION_REQUEST / store, if any */
  selection->current_request_selection = NULL;
  cdk_wayland_selection_handle_next_request (selection);
}

static SelectionBuffer *
cdk_wayland_selection_lookup_requestor_buffer (CdkWindow *requestor)
{
  CdkDisplay *display = cdk_window_get_display (requestor);
  CdkWaylandSelection *selection = cdk_wayland_display_get_selection (display);
  SelectionBuffer *buffer_data;
  GHashTableIter iter;
  gint i;

  for (i = 0; i < G_N_ELEMENTS (selection->selections); i++)
    {
      g_hash_table_iter_init (&iter, selection->selections[i].buffers);

      while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &buffer_data))
        {
          if (g_list_find (buffer_data->requestors, requestor))
            return buffer_data;
        }
    }

  return NULL;
}

static gboolean
cdk_wayland_selection_source_handles_target (CdkWaylandSelection *wayland_selection,
                                             CdkAtom              target)
{
  CdkAtom atom;
  guint i;

  if (target == CDK_NONE)
    return FALSE;

  for (i = 0; i < wayland_selection->source_targets->len; i++)
    {
      atom = g_array_index (wayland_selection->source_targets, CdkAtom, i);

      if (atom == target)
        return TRUE;
    }

  return FALSE;
}

static void
cdk_wayland_selection_handle_next_request (CdkWaylandSelection *wayland_selection)
{
  gint i;

  for (i = 0; i < wayland_selection->stored_selections->len; i++)
    {
      StoredSelection *stored_selection;

      stored_selection = g_ptr_array_index (wayland_selection->stored_selections, i);

      if (!stored_selection->data)
        {
          cdk_wayland_selection_emit_request (stored_selection->source,
                                              stored_selection->selection_atom,
                                              stored_selection->type);
          wayland_selection->current_request_selection = stored_selection;
          break;
        }
    }
}

static gboolean
cdk_wayland_selection_request_target (CdkWaylandSelection *wayland_selection,
                                      CdkWindow           *window,
                                      CdkAtom              selection,
                                      CdkAtom              target,
                                      gint                 fd)
{
  StoredSelection *stored_selection;
  AsyncWriteData *write_data;

  if (!window ||
      !cdk_wayland_selection_source_handles_target (wayland_selection, target))
    {
      close (fd);
      return FALSE;
    }

  stored_selection =
    cdk_wayland_selection_find_stored_selection (wayland_selection, window,
                                                 selection, target);

  if (stored_selection && stored_selection->data)
    {
      /* Fast path, we already have the type cached */
      write_data = async_write_data_new (stored_selection, fd);
      async_write_data_write (write_data);
      return TRUE;
    }

  if (!stored_selection)
    {
      stored_selection = stored_selection_new (wayland_selection, window,
                                               selection, target);
      g_ptr_array_add (wayland_selection->stored_selections, stored_selection);
    }

  write_data = async_write_data_new (stored_selection, fd);

  if (!wayland_selection->current_request_selection)
    cdk_wayland_selection_handle_next_request (wayland_selection);

  return TRUE;
}

static void
data_source_target (void                  *data,
                    struct wl_data_source *source,
                    const char            *mime_type)
{
  CDK_NOTE (EVENTS,
            g_message ("data source target, source = %p, mime_type = %s",
                       source, mime_type));
}

static void
data_source_send (void                  *data,
                  struct wl_data_source *source,
                  const char            *mime_type,
                  int32_t                fd)
{
  CdkWaylandSelection *wayland_selection = data;
  CdkWindow *window;
  CdkAtom selection;

  CDK_NOTE (EVENTS,
            g_message ("data source send, source = %p, mime_type = %s, fd = %d",
                       source, mime_type, fd));

  if (!mime_type)
    {
      close (fd);
      return;
    }

  if (source == wayland_selection->dnd_source)
    {
      window = wayland_selection->dnd_owner;
      selection = atoms[ATOM_DND];
    }
  else if (source == wayland_selection->clipboard_source)
    {
      window = wayland_selection->clipboard_owner;
      selection = atoms[ATOM_CLIPBOARD];
    }
  else
    {
      close (fd);
      return;
    }

  if (!window)
    return;

  cdk_wayland_selection_request_target (wayland_selection, window,
                                        selection,
                                        cdk_atom_intern (mime_type, FALSE),
                                        fd);
}

static void
data_source_cancelled (void                  *data,
                       struct wl_data_source *source)
{
  CdkWaylandSelection *wayland_selection = data;
  CdkDragContext *context;
  CdkDisplay *display;
  CdkAtom atom;

  CDK_NOTE (EVENTS,
            g_message ("data source cancelled, source = %p", source));

  display = cdk_display_get_default ();

  if (source == wayland_selection->dnd_source)
    atom = atoms[ATOM_DND];
  else if (source == wayland_selection->clipboard_source)
    atom = atoms[ATOM_CLIPBOARD];
  else
    return;

  context = cdk_wayland_drag_context_lookup_by_data_source (source);

  if (context)
    cdk_drag_context_cancel (context, CDK_DRAG_CANCEL_ERROR);

  emit_selection_clear (display, atom);
  cdk_selection_owner_set (NULL, atom, CDK_CURRENT_TIME, FALSE);
  cdk_wayland_selection_unset_data_source (display, atom);
}

static void
data_source_dnd_drop_performed (void                  *data,
                                struct wl_data_source *source)
{
  CdkDragContext *context;

  context = cdk_wayland_drag_context_lookup_by_data_source (source);

  if (!context)
    return;

  g_signal_emit_by_name (context, "drop-performed", CDK_CURRENT_TIME);
}

static void
data_source_dnd_finished (void                  *data,
                          struct wl_data_source *source)
{
  CdkDisplay *display = cdk_display_get_default ();
  CdkDragContext *context;

  context = cdk_wayland_drag_context_lookup_by_data_source (source);

  if (!context)
    return;

  if (context->action == CDK_ACTION_MOVE)
    {
      cdk_wayland_selection_emit_request (context->source_window,
                                          atoms[ATOM_DND],
                                          cdk_atom_intern_static_string ("DELETE"));
    }

  g_signal_emit_by_name (context, "dnd-finished");
  cdk_selection_owner_set (NULL, atoms[ATOM_DND], CDK_CURRENT_TIME, TRUE);
  cdk_wayland_selection_clear_targets (display, atoms[ATOM_DND]);
}

static void
data_source_action (void                  *data,
                    struct wl_data_source *source,
                    uint32_t               action)
{
  CdkDragContext *context;

  CDK_NOTE (EVENTS,
            g_message ("data source action, source = %p action=%x",
                       source, action));

  context = cdk_wayland_drag_context_lookup_by_data_source (source);

  if (!context)
    return;

  context->action = _wl_to_cdk_actions (action);
  g_signal_emit_by_name (context, "action-changed", context->action);
}

static const struct wl_data_source_listener data_source_listener = {
  data_source_target,
  data_source_send,
  data_source_cancelled,
  data_source_dnd_drop_performed,
  data_source_dnd_finished,
  data_source_action,
};

static void
primary_source_send (void       *data,
                     gpointer    source,
                     const char *mime_type,
                     int32_t     fd)
{
  CdkWaylandSelection *wayland_selection = data;

  CDK_NOTE (EVENTS,
            g_message ("primary source send, source = %p, mime_type = %s, fd = %d",
                       source, mime_type, fd));

  if (!mime_type || !wayland_selection->primary_owner)
    {
      close (fd);
      return;
    }

  cdk_wayland_selection_request_target (wayland_selection,
                                        wayland_selection->primary_owner,
                                        atoms[ATOM_PRIMARY],
                                        cdk_atom_intern (mime_type, FALSE),
                                        fd);
}

static void
ctk_primary_source_send (void                                *data,
                         struct ctk_primary_selection_source *source,
                         const char                          *mime_type,
                         int32_t                              fd)
{
  primary_source_send (data, (gpointer) source, mime_type, fd);
}

static void
zwp_primary_source_v1_send (void                                   *data,
                            struct zwp_primary_selection_source_v1 *source,
                            const char                             *mime_type,
                            int32_t                                 fd)
{
  primary_source_send (data, (gpointer) source, mime_type, fd);
}

static void
primary_source_cancelled (void     *data,
                          gpointer  source)
{
  CdkDisplay *display;
  CdkAtom atom;

  CDK_NOTE (EVENTS,
            g_message ("primary source cancelled, source = %p", source));

  display = cdk_display_get_default ();

  atom = atoms[ATOM_PRIMARY];
  emit_selection_clear (display, atom);
  cdk_selection_owner_set (NULL, atom, CDK_CURRENT_TIME, FALSE);
  cdk_wayland_selection_unset_data_source (display, atom);
}

static void
ctk_primary_source_cancelled (void                                *data,
                              struct ctk_primary_selection_source *source)
{
  primary_source_cancelled (data, source);
}

static void
zwp_primary_source_v1_cancelled (void                                   *data,
                                 struct zwp_primary_selection_source_v1 *source)
{
  primary_source_cancelled (data, source);
}

static const struct ctk_primary_selection_source_listener ctk_primary_source_listener = {
  ctk_primary_source_send,
  ctk_primary_source_cancelled,
};

static const struct zwp_primary_selection_source_v1_listener zwp_primary_source_v1_listener = {
  zwp_primary_source_v1_send,
  zwp_primary_source_v1_cancelled,
};

struct wl_data_source *
cdk_wayland_selection_get_data_source (CdkWindow *owner,
                                       CdkAtom    selection)
{
  CdkDisplay *display = cdk_window_get_display (owner);
  CdkWaylandSelection *wayland_selection = cdk_wayland_display_get_selection (display);
  gpointer source = NULL;
  CdkWaylandDisplay *display_wayland;

  if (selection == atoms[ATOM_DND])
    {
      if (wayland_selection->dnd_source &&
          (!owner || owner == wayland_selection->dnd_owner))
        return wayland_selection->dnd_source;
    }
  else if (selection == atoms[ATOM_PRIMARY])
    {
      if (wayland_selection->primary_source &&
          (!owner || owner == wayland_selection->primary_owner))
        return wayland_selection->primary_source;

      if (wayland_selection->primary_source)
        {
          primary_selection_source_destroy (wayland_selection->primary_source);
          wayland_selection->primary_source = NULL;
        }
    }
  else if (selection == atoms[ATOM_CLIPBOARD])
    {
      if (wayland_selection->clipboard_source &&
          (!owner || owner == wayland_selection->clipboard_owner))
        return wayland_selection->clipboard_source;

      if (wayland_selection->clipboard_source)
        {
          wl_data_source_destroy (wayland_selection->clipboard_source);
          wayland_selection->clipboard_source = NULL;
        }
    }
  else
    return NULL;

  if (!owner)
    return NULL;

  display_wayland = CDK_WAYLAND_DISPLAY (cdk_window_get_display (owner));

  if (selection == atoms[ATOM_PRIMARY])
    {
      if (display_wayland->zwp_primary_selection_manager_v1)
        {
          source = zwp_primary_selection_device_manager_v1_create_source (display_wayland->zwp_primary_selection_manager_v1);
          zwp_primary_selection_source_v1_add_listener (source,
                                                        &zwp_primary_source_v1_listener,
                                                        wayland_selection);
        }
      else if (display_wayland->ctk_primary_selection_manager)
        {
          source = ctk_primary_selection_device_manager_create_source (display_wayland->ctk_primary_selection_manager);
          ctk_primary_selection_source_add_listener (source,
                                                     &ctk_primary_source_listener,
                                                     wayland_selection);
        }
    }
  else
    {
      source = wl_data_device_manager_create_data_source (display_wayland->data_device_manager);
      wl_data_source_add_listener (source,
                                   &data_source_listener,
                                   wayland_selection);
    }

  if (selection == atoms[ATOM_DND])
    wayland_selection->dnd_source = source;
  else if (selection == atoms[ATOM_PRIMARY])
    wayland_selection->primary_source = source;
  else if (selection == atoms[ATOM_CLIPBOARD])
    wayland_selection->clipboard_source = source;

  return source;
}

void
cdk_wayland_selection_unset_data_source (CdkDisplay *display,
                                         CdkAtom     selection)
{
  CdkWaylandSelection *wayland_selection = cdk_wayland_display_get_selection (display);

  if (selection == atoms[ATOM_CLIPBOARD])
    {
      if (wayland_selection->clipboard_source)
        {
          wl_data_source_destroy (wayland_selection->clipboard_source);
          wayland_selection->clipboard_source = NULL;
        }
    }
  else if (selection == atoms[ATOM_PRIMARY])
    {
      if (wayland_selection->primary_source)
        {
          primary_selection_source_destroy (wayland_selection->primary_source);
          wayland_selection->primary_source = NULL;
        }
    }
  else if (selection == atoms[ATOM_DND])
    {
      wayland_selection->dnd_source = NULL;
    }
}

CdkWindow *
_cdk_wayland_display_get_selection_owner (CdkDisplay *display,
                                          CdkAtom     selection)
{
  CdkWaylandSelection *wayland_selection = cdk_wayland_display_get_selection (display);

  if (selection == atoms[ATOM_CLIPBOARD])
    return wayland_selection->clipboard_owner;
  else if (selection == atoms[ATOM_PRIMARY])
    return wayland_selection->primary_owner;
  else if (selection == atoms[ATOM_DND])
    return wayland_selection->dnd_owner;

  return NULL;
}

gboolean
_cdk_wayland_display_set_selection_owner (CdkDisplay *display,
                                          CdkWindow  *owner,
                                          CdkAtom     selection,
                                          guint32     time,
                                          gboolean    send_event)
{
  CdkWaylandSelection *wayland_selection = cdk_wayland_display_get_selection (display);
  CdkSeat *seat = cdk_display_get_default_seat (display);

  cdk_wayland_selection_reset_selection (wayland_selection, selection);

  if (selection == atoms[ATOM_CLIPBOARD])
    {
      wayland_selection->clipboard_owner = owner;
      if (send_event && !owner)
        {
          cdk_wayland_seat_set_selection (seat, NULL);
          cdk_wayland_selection_unset_data_source (display, selection);
        }
      return TRUE;
    }
  else if (selection == atoms[ATOM_PRIMARY])
    {
      wayland_selection->primary_owner = owner;
      if (send_event && !owner)
        {
          cdk_wayland_seat_set_primary (seat, NULL);
          cdk_wayland_selection_unset_data_source (display, selection);
        }
      return TRUE;
    }
  else if (selection == atoms[ATOM_DND])
    {
      wayland_selection->dnd_owner = owner;
      return TRUE;
    }

  return FALSE;
}

void
_cdk_wayland_display_send_selection_notify (CdkDisplay *display,
                                            CdkWindow  *requestor,
                                            CdkAtom     selection,
                                            CdkAtom     target,
                                            CdkAtom     property,
                                            guint32     time)
{
  CdkWaylandSelection *wayland_selection;

  if (property != CDK_NONE)
    return;

  wayland_selection = cdk_wayland_display_get_selection (display);

  if (!wayland_selection->current_request_selection)
    return;

  g_ptr_array_remove_fast (wayland_selection->stored_selections,
                           wayland_selection->current_request_selection);

  /* Handle the next CDK_SELECTION_REQUEST / store, if any */
  wayland_selection->current_request_selection = NULL;
  cdk_wayland_selection_handle_next_request (wayland_selection);
}

gint
_cdk_wayland_display_get_selection_property (CdkDisplay  *display,
                                             CdkWindow   *requestor,
                                             guchar     **data,
                                             CdkAtom     *ret_type,
                                             gint        *ret_format)
{
  SelectionBuffer *buffer_data;
  gsize len;

  buffer_data = cdk_wayland_selection_lookup_requestor_buffer (requestor);

  if (!buffer_data)
    return 0;

  selection_buffer_remove_requestor (buffer_data, requestor);
  len = buffer_data->data->len;

  if (data)
    {
      guchar *buffer;

      buffer = g_new0 (guchar, len + 1);
      memcpy (buffer, buffer_data->data->data, len);
      *data = buffer;
    }

  if (buffer_data->target == cdk_atom_intern_static_string ("TARGETS"))
    {
      if (ret_type)
        *ret_type = CDK_SELECTION_TYPE_ATOM;
      if (ret_format)
        *ret_format = 32;
    }
  else
    {
      if (ret_type)
        *ret_type = buffer_data->target;
      if (ret_format)
        *ret_format = 8;
    }

  return len;
}

static void
emit_empty_selection_notify (CdkWindow *requestor,
                             CdkAtom    selection,
                             CdkAtom    target)
{
  CdkEvent *event;

  event = cdk_event_new (CDK_SELECTION_NOTIFY);
  event->selection.window = g_object_ref (requestor);
  event->selection.send_event = FALSE;
  event->selection.selection = selection;
  event->selection.target = target;
  event->selection.property = CDK_NONE;
  event->selection.time = CDK_CURRENT_TIME;
  event->selection.requestor = g_object_ref (requestor);

  cdk_event_put (event);
  cdk_event_free (event);
}

static void
emit_selection_clear (CdkDisplay *display,
                      CdkAtom     selection)
{
  CdkEvent *event;
  CdkWindow *window;

  event = cdk_event_new (CDK_SELECTION_CLEAR);
  event->selection.selection = selection;
  event->selection.time = CDK_CURRENT_TIME;

  window = _cdk_wayland_display_get_selection_owner (display, selection);
  if (window != NULL)
    {
      event->selection.window = g_object_ref (window);
      event->selection.requestor = g_object_ref (window);
    }

  cdk_event_put (event);
  cdk_event_free (event);
}

void
_cdk_wayland_display_convert_selection (CdkDisplay *display,
                                        CdkWindow  *requestor,
                                        CdkAtom     selection,
                                        CdkAtom     target,
                                        guint32     time)
{
  CdkWaylandDisplay *display_wayland = CDK_WAYLAND_DISPLAY (display);
  CdkWaylandSelection *wayland_selection = cdk_wayland_display_get_selection (display);
  const SelectionData *selection_data;
  SelectionBuffer *buffer_data;
  gpointer offer;
  gchar *mimetype;
  GList *target_list;

  selection_data = selection_lookup_offer_by_atom (wayland_selection, selection);
  if (!selection_data)
    {
      emit_empty_selection_notify (requestor, selection, target);
      return;
    }

  offer = cdk_wayland_selection_get_offer (display, selection);
  target_list = cdk_wayland_selection_get_targets (display, selection);

  if (!offer || target == cdk_atom_intern_static_string ("DELETE"))
    {
      emit_empty_selection_notify (requestor, selection, target);
      return;
    }

  mimetype = cdk_atom_name (target);

  if (target != cdk_atom_intern_static_string ("TARGETS"))
    {
      if (!g_list_find (target_list, CDK_ATOM_TO_POINTER (target)))
        {
          emit_empty_selection_notify (requestor, selection, target);
          return;
        }

      if (selection != atoms[ATOM_PRIMARY])
        wl_data_offer_accept (offer,
                              _cdk_wayland_display_get_serial (CDK_WAYLAND_DISPLAY (display)),
                              mimetype);
    }

  buffer_data = g_hash_table_lookup (selection_data->buffers, target);

  if (buffer_data)
    selection_buffer_add_requestor (buffer_data, requestor);
  else
    {
      GInputStream *stream = NULL;
      int natoms = 0;
      CdkAtom *targets = NULL;

      if (target == cdk_atom_intern_static_string ("TARGETS"))
        {
          gint i = 0;
          GList *l;

          natoms = g_list_length (target_list);
          targets = g_new0 (CdkAtom, natoms);

          for (l = target_list; l; l = l->next)
            targets[i++] = l->data;
        }
      else
        {
          int pipe_fd[2];

          g_unix_open_pipe (pipe_fd, FD_CLOEXEC, NULL);

          if (selection == atoms[ATOM_PRIMARY])
            {
              if (display_wayland->zwp_primary_selection_manager_v1)
                zwp_primary_selection_offer_v1_receive (offer, mimetype, pipe_fd[1]);
              else if (display_wayland->ctk_primary_selection_manager)
                ctk_primary_selection_offer_receive (offer, mimetype, pipe_fd[1]);
            }
          else
            {
              wl_data_offer_receive (offer, mimetype, pipe_fd[1]);
            }

          stream = g_unix_input_stream_new (pipe_fd[0], TRUE);
          close (pipe_fd[1]);
        }

      buffer_data = selection_buffer_new (stream, selection, target);
      selection_buffer_add_requestor (buffer_data, requestor);

      if (stream)
        g_object_unref (stream);

      if (targets)
        {
          /* Store directly the local atoms */
          selection_buffer_append_data (buffer_data, targets, natoms * sizeof (CdkAtom));
          g_free (targets);
        }

      g_hash_table_insert (selection_data->buffers,
                           CDK_ATOM_TO_POINTER (target),
                           buffer_data);
    }

  if (!buffer_data->stream)
    selection_buffer_notify (buffer_data);

  g_free (mimetype);
}

gint
_cdk_wayland_display_text_property_to_utf8_list (CdkDisplay    *display,
                                                 CdkAtom        encoding,
                                                 gint           format,
                                                 const guchar  *text,
                                                 gint           length,
                                                 gchar       ***list)
{
  GPtrArray *array;
  const gchar *ptr;
  gsize chunk_len;
  gchar *copy;
  guint nitems;

  ptr = (const gchar *) text;
  array = g_ptr_array_new ();

  while (ptr < (const gchar *) &text[length])
    {
      chunk_len = strlen (ptr);

      if (g_utf8_validate (ptr, chunk_len, NULL))
        {
          copy = g_strndup (ptr, chunk_len);
          g_ptr_array_add (array, copy);
        }

      ptr = &ptr[chunk_len + 1];
    }

  nitems = array->len;
  g_ptr_array_add (array, NULL);

  if (list)
    *list = (gchar **) g_ptr_array_free (array, FALSE);
  else
    g_ptr_array_free (array, TRUE);

  return nitems;
}

/* This function has been copied straight from the x11 backend */
static gchar *
sanitize_utf8 (const gchar *src,
               gboolean return_latin1)
{
  gint len = strlen (src);
  GString *result = g_string_sized_new (len);
  const gchar *p = src;

  while (*p)
    {
      if (*p == '\r')
        {
          p++;
          if (*p == '\n')
            p++;

          g_string_append_c (result, '\n');
        }
      else
        {
          gunichar ch = g_utf8_get_char (p);

          if (!((ch < 0x20 && ch != '\t' && ch != '\n') || (ch >= 0x7f && ch < 0xa0)))
            {
              if (return_latin1)
                {
                  if (ch <= 0xff)
                    g_string_append_c (result, ch);
                  else
                    g_string_append_printf (result,
                                            ch < 0x10000 ? "\\u%04x" : "\\U%08x",
                                            ch);
                }
              else
                {
                  char buf[7];
                  gint buflen;

                  buflen = g_unichar_to_utf8 (ch, buf);
                  g_string_append_len (result, buf, buflen);
                }
            }

          p = g_utf8_next_char (p);
        }
    }

  return g_string_free (result, FALSE);
}

gchar *
_cdk_wayland_display_utf8_to_string_target (CdkDisplay  *display,
                                            const gchar *str)
{
  /* This is mainly needed when interfacing with old clients through
   * Xwayland, the STRING target could be used, and passed as-is
   * by the compositor.
   *
   * There's already some handling of this atom (aka "mimetype" in
   * this backend) in common code, so we end up in this vfunc.
   */
  return sanitize_utf8 (str, TRUE);
}

void
cdk_wayland_selection_add_targets (CdkWindow *window,
                                   CdkAtom    selection,
                                   guint      ntargets,
                                   CdkAtom   *targets)
{
  CdkDisplay *display = cdk_window_get_display (window);
  CdkWaylandSelection *wayland_selection = cdk_wayland_display_get_selection (display);
  gpointer data_source;
  guint i;

  g_return_if_fail (CDK_IS_WINDOW (window));

  data_source = cdk_wayland_selection_get_data_source (window, selection);

  if (!data_source)
    return;

  g_array_append_vals (wayland_selection->source_targets, targets, ntargets);

  for (i = 0; i < ntargets; i++)
    {
      gchar *mimetype = cdk_atom_name (targets[i]);

      wl_data_source_offer (data_source, mimetype);
      g_free (mimetype);
    }

  if (selection == atoms[ATOM_CLIPBOARD])
    {
      CdkSeat *seat;

      seat = cdk_display_get_default_seat (display);
      cdk_wayland_seat_set_selection (seat, data_source);
    }
  else if (selection == atoms[ATOM_PRIMARY])
    {
      CdkSeat *seat;

      seat = cdk_display_get_default_seat (display);
      cdk_wayland_seat_set_primary (seat, data_source);
    }
}

void
cdk_wayland_selection_clear_targets (CdkDisplay *display,
                                     CdkAtom     selection)
{
  CdkWaylandSelection *wayland_selection = cdk_wayland_display_get_selection (display);

  wayland_selection->requested_target = CDK_NONE;
  g_array_set_size (wayland_selection->source_targets, 0);
  cdk_wayland_selection_unset_data_source (display, selection);
}

gboolean
cdk_wayland_selection_set_current_offer_actions (CdkDisplay *display,
                                                 uint32_t    action)
{
  CdkWaylandDisplay *display_wayland = CDK_WAYLAND_DISPLAY (display);
  struct wl_data_offer *offer;
  uint32_t all_actions = 0;

  offer = cdk_wayland_selection_get_offer (display, atoms[ATOM_DND]);

  if (!offer)
    return FALSE;

  if (action != 0)
    all_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
      WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE |
      WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;

  if (display_wayland->data_device_manager_version >=
      WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION)
    wl_data_offer_set_actions (offer, all_actions, action);
  return TRUE;
}
