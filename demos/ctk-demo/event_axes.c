/* Touch and Drawing Tablets
 *
 * Demonstrates advanced handling of event information from exotic
 * input devices.
 *
 * On one hand, this snippet demonstrates management of drawing tablets,
 * those contain additional information for the pointer other than
 * X/Y coordinates. Tablet pads events are mapped to actions, which
 * are both defined and interpreted by the application.
 *
 * Input axes are dependent on hardware devices, on linux/unix you
 * can see the device axes through xinput list <device>. Each time
 * a different hardware device is used to move the pointer, the
 * master device will be updated to match the axes it provides,
 * these changes can be tracked through CdkDevice::changed, or
 * checking cdk_event_get_source_device().
 *
 * On the other hand, this demo handles basic multitouch events,
 * each event coming from an specific touchpoint will contain a
 * CdkEventSequence that's unique for its lifetime, so multiple
 * touchpoints can be tracked.
 */

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include "fallback-memdup.h"

typedef struct {
  CdkDevice *last_source;
  CdkDeviceTool *last_tool;
  gdouble *axes;
  CdkRGBA color;
  gdouble x;
  gdouble y;
} AxesInfo;

typedef struct {
  GHashTable *pointer_info; /* CdkDevice -> AxesInfo */
  GHashTable *touch_info; /* CdkEventSequence -> AxesInfo */
} EventData;

const gchar *colors[] = {
  "black",
  "orchid",
  "fuchsia",
  "indigo",
  "thistle",
  "sienna",
  "azure",
  "plum",
  "lime",
  "navy",
  "maroon",
  "burlywood"
};

static CtkPadActionEntry pad_actions[] = {
  { CTK_PAD_ACTION_BUTTON, 1, -1, N_("Nuclear strike"), "pad.nuke" },
  { CTK_PAD_ACTION_BUTTON, 2, -1, N_("Release siberian methane reserves"), "pad.heat" },
  { CTK_PAD_ACTION_BUTTON, 3, -1, N_("Release solar flare"), "pad.fry" },
  { CTK_PAD_ACTION_BUTTON, 4, -1, N_("De-stabilize Oort cloud"), "pad.fall" },
  { CTK_PAD_ACTION_BUTTON, 5, -1, N_("Ignite WR-104"), "pad.burst" },
  { CTK_PAD_ACTION_BUTTON, 6, -1, N_("Lart whoever asks about this button"), "pad.lart" },
  { CTK_PAD_ACTION_RING, -1, -1, N_("Earth axial tilt"), "pad.tilt" },
  { CTK_PAD_ACTION_STRIP, -1, -1, N_("Extent of weak nuclear force"), "pad.dissolve" },
};

static const gchar *pad_action_results[] = {
  "☢",
  "♨",
  "☼",
  "☄",
  "⚡",
  "💫",
  "◑",
  "⚛"
};

static guint cur_color = 0;
static guint pad_action_timeout_id = 0;

static AxesInfo *
axes_info_new (void)
{
  AxesInfo *info;

  info = g_new0 (AxesInfo, 1);
  cdk_rgba_parse (&info->color, colors[cur_color]);

  cur_color = (cur_color + 1) % G_N_ELEMENTS (colors);

  return info;
}

static EventData *
event_data_new (void)
{
  EventData *data;

  data = g_new0 (EventData, 1);
  data->pointer_info = g_hash_table_new_full (NULL, NULL, NULL,
                                              (GDestroyNotify) g_free);
  data->touch_info = g_hash_table_new_full (NULL, NULL, NULL,
                                            (GDestroyNotify) g_free);

  return data;
}

static void
event_data_free (EventData *data)
{
  g_hash_table_destroy (data->pointer_info);
  g_hash_table_destroy (data->touch_info);
  g_free (data);
}

static void
update_axes_from_event (CdkEvent  *event,
                        EventData *data)
{
  CdkDevice *device, *source_device;
  CdkEventSequence *sequence;
  CdkDeviceTool *tool;
  gdouble x, y;
  AxesInfo *info;

  device = cdk_event_get_device (event);
  source_device = cdk_event_get_source_device (event);
  sequence = cdk_event_get_event_sequence (event);
  tool = cdk_event_get_device_tool (event);

  if (event->type == CDK_TOUCH_END ||
      event->type == CDK_TOUCH_CANCEL)
    {
      g_hash_table_remove (data->touch_info, sequence);
      return;
    }
  else if (event->type == CDK_LEAVE_NOTIFY)
    {
      g_hash_table_remove (data->pointer_info, device);
      return;
    }

  if (!sequence)
    {
      info = g_hash_table_lookup (data->pointer_info, device);

      if (!info)
        {
          info = axes_info_new ();
          g_hash_table_insert (data->pointer_info, device, info);
        }
    }
  else
    {
      info = g_hash_table_lookup (data->touch_info, sequence);

      if (!info)
        {
          info = axes_info_new ();
          g_hash_table_insert (data->touch_info, sequence, info);
        }
    }

  if (info->last_source != source_device)
    info->last_source = source_device;

  if (info->last_tool != tool)
    info->last_tool = tool;

  g_clear_pointer (&info->axes, g_free);

  if (event->type == CDK_TOUCH_BEGIN ||
      event->type == CDK_TOUCH_UPDATE)
    {
      if (sequence && event->touch.emulating_pointer)
        g_hash_table_remove (data->pointer_info, device);
    }
  if (event->type == CDK_MOTION_NOTIFY)
    {
      info->axes =
      g_memdup2 (event->motion.axes,
                 sizeof (gdouble) * cdk_device_get_n_axes (source_device));
    }
  else if (event->type == CDK_BUTTON_PRESS ||
           event->type == CDK_BUTTON_RELEASE)
    {
      info->axes =
      g_memdup2 (event->button.axes,
                 sizeof (gdouble) * cdk_device_get_n_axes (source_device));
    }

  if (cdk_event_get_coords (event, &x, &y))
    {
      info->x = x;
      info->y = y;
    }
}

static gboolean
event_cb (CtkWidget *widget,
          CdkEvent  *event,
          gpointer   user_data)
{
  update_axes_from_event (event, user_data);
  ctk_widget_queue_draw (widget);
  return FALSE;
}

static void
render_arrow (cairo_t     *cr,
              gdouble      x_diff,
              gdouble      y_diff,
              const gchar *label)
{
  cairo_save (cr);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_new_path (cr);
  cairo_move_to (cr, 0, 0);
  cairo_line_to (cr, x_diff, y_diff);
  cairo_stroke (cr);

  cairo_move_to (cr, x_diff, y_diff);
  cairo_show_text (cr, label);

  cairo_restore (cr);
}

static void
draw_axes_info (cairo_t       *cr,
                AxesInfo      *info,
                CtkAllocation *allocation)
{
  gdouble pressure, tilt_x, tilt_y, distance, wheel, rotation, slider;
  CdkAxisFlags axes = cdk_device_get_axes (info->last_source);

  cairo_save (cr);

  cairo_set_line_width (cr, 1);
  cdk_cairo_set_source_rgba (cr, &info->color);

  cairo_move_to (cr, 0, info->y);
  cairo_line_to (cr, allocation->width, info->y);
  cairo_move_to (cr, info->x, 0);
  cairo_line_to (cr, info->x, allocation->height);
  cairo_stroke (cr);

  cairo_translate (cr, info->x, info->y);

  if (!info->axes)
    {
      cairo_restore (cr);
      return;
    }

  if (axes & CDK_AXIS_FLAG_PRESSURE)
    {
      cairo_pattern_t *pattern;

      cdk_device_get_axis (info->last_source, info->axes, CDK_AXIS_PRESSURE,
                           &pressure);

      pattern = cairo_pattern_create_radial (0, 0, 0, 0, 0, 100);
      cairo_pattern_add_color_stop_rgba (pattern, pressure, 1, 0, 0, pressure);
      cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 1, 0);

      cairo_set_source (cr, pattern);

      cairo_arc (cr, 0, 0, 100, 0, 2 * G_PI);
      cairo_fill (cr);

      cairo_pattern_destroy (pattern);
    }

  if (axes & CDK_AXIS_FLAG_XTILT &&
      axes & CDK_AXIS_FLAG_YTILT)
    {
      cdk_device_get_axis (info->last_source, info->axes, CDK_AXIS_XTILT,
                           &tilt_x);
      cdk_device_get_axis (info->last_source, info->axes, CDK_AXIS_YTILT,
                           &tilt_y);

      render_arrow (cr, tilt_x * 100, tilt_y * 100, "Tilt");
    }

  if (axes & CDK_AXIS_FLAG_DISTANCE)
    {
      double dashes[] = { 5.0, 5.0 };
      cairo_text_extents_t extents;

      cdk_device_get_axis (info->last_source, info->axes, CDK_AXIS_DISTANCE,
                           &distance);

      cairo_save (cr);

      cairo_move_to (cr, distance * 100, 0);

      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
      cairo_set_dash (cr, dashes, 2, 0.0);
      cairo_arc (cr, 0, 0, distance * 100, 0, 2 * G_PI);
      cairo_stroke (cr);

      cairo_move_to (cr, 0, -distance * 100);
      cairo_text_extents (cr, "Distance", &extents);
      cairo_rel_move_to (cr, -extents.width / 2, 0);
      cairo_show_text (cr, "Distance");

      cairo_move_to (cr, 0, 0);

      cairo_restore (cr);
    }

  if (axes & CDK_AXIS_FLAG_WHEEL)
    {
      cdk_device_get_axis (info->last_source, info->axes, CDK_AXIS_WHEEL,
                           &wheel);

      cairo_save (cr);
      cairo_set_line_width (cr, 10);
      cairo_set_source_rgba (cr, 0, 0, 0, 0.5);

      cairo_new_sub_path (cr);
      cairo_arc (cr, 0, 0, 100, 0, wheel * 2 * G_PI);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

  if (axes & CDK_AXIS_FLAG_ROTATION)
    {
      cdk_device_get_axis (info->last_source, info->axes, CDK_AXIS_ROTATION,
                           &rotation);
      rotation *= 2 * G_PI;

      cairo_save (cr);
      cairo_rotate (cr, - G_PI / 2);
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_width (cr, 5);

      cairo_new_sub_path (cr);
      cairo_arc (cr, 0, 0, 100, 0, rotation);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

  if (axes & CDK_AXIS_FLAG_SLIDER)
    {
      cairo_pattern_t *pattern, *mask;

      cdk_device_get_axis (info->last_source, info->axes, CDK_AXIS_SLIDER,
                           &slider);

      cairo_save (cr);

      cairo_move_to (cr, 0, -10);
      cairo_rel_line_to (cr, 0, -50);
      cairo_rel_line_to (cr, 10, 0);
      cairo_rel_line_to (cr, -5, 50);
      cairo_close_path (cr);

      cairo_clip_preserve (cr);

      pattern = cairo_pattern_create_linear (0, -10, 0, -60);
      cairo_pattern_add_color_stop_rgb (pattern, 0, 0, 1, 0);
      cairo_pattern_add_color_stop_rgb (pattern, 1, 1, 0, 0);
      cairo_set_source (cr, pattern);
      cairo_pattern_destroy (pattern);

      mask = cairo_pattern_create_linear (0, -10, 0, -60);
      cairo_pattern_add_color_stop_rgba (mask, 0, 0, 0, 0, 1);
      cairo_pattern_add_color_stop_rgba (mask, slider, 0, 0, 0, 1);
      cairo_pattern_add_color_stop_rgba (mask, slider, 0, 0, 0, 0);
      cairo_pattern_add_color_stop_rgba (mask, 1, 0, 0, 0, 0);
      cairo_mask (cr, mask);
      cairo_pattern_destroy (mask);

      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_stroke (cr);

      cairo_restore (cr);
    }

  cairo_restore (cr);
}

static const gchar *
tool_type_to_string (CdkDeviceToolType tool_type)
{
  switch (tool_type)
    {
    case CDK_DEVICE_TOOL_TYPE_PEN:
      return "Pen";
    case CDK_DEVICE_TOOL_TYPE_ERASER:
      return "Eraser";
    case CDK_DEVICE_TOOL_TYPE_BRUSH:
      return "Brush";
    case CDK_DEVICE_TOOL_TYPE_PENCIL:
      return "Pencil";
    case CDK_DEVICE_TOOL_TYPE_AIRBRUSH:
      return "Airbrush";
    case CDK_DEVICE_TOOL_TYPE_MOUSE:
      return "Mouse";
    case CDK_DEVICE_TOOL_TYPE_LENS:
      return "Lens cursor";
    case CDK_DEVICE_TOOL_TYPE_UNKNOWN:
    default:
      return "Unknown";
    }
}

static void
draw_device_info (CtkWidget        *widget,
                  cairo_t          *cr,
                  CdkEventSequence *sequence,
                  gint             *y,
                  AxesInfo         *info)
{
  PangoLayout *layout;
  GString *string;
  gint height;

  cairo_save (cr);

  string = g_string_new (NULL);
  g_string_append_printf (string, "Source: %s",
                          cdk_device_get_name (info->last_source));

  if (sequence)
    g_string_append_printf (string, "\nSequence: %d",
                            GPOINTER_TO_UINT (sequence));

  if (info->last_tool)
    {
      const gchar *tool_type;
      guint64 serial;

      tool_type = tool_type_to_string (cdk_device_tool_get_tool_type (info->last_tool));
      serial = cdk_device_tool_get_serial (info->last_tool);
      g_string_append_printf (string, "\nTool: %s", tool_type);

      if (serial != 0)
        g_string_append_printf (string, ", Serial: %lx", serial);
    }

  cairo_move_to (cr, 10, *y);
  layout = ctk_widget_create_pango_layout (widget, string->str);
  pango_cairo_show_layout (cr, layout);
  cairo_stroke (cr);

  pango_layout_get_pixel_size (layout, NULL, &height);

  cdk_cairo_set_source_rgba (cr, &info->color);
  cairo_set_line_width (cr, 10);
  cairo_move_to (cr, 0, *y);

  *y = *y + height;
  cairo_line_to (cr, 0, *y);
  cairo_stroke (cr);

  cairo_restore (cr);

  g_object_unref (layout);
  g_string_free (string, TRUE);
}

static gboolean
draw_cb (CtkWidget *widget,
         cairo_t   *cr,
         gpointer   user_data)
{
  EventData *data = user_data;
  CtkAllocation allocation;
  AxesInfo *info;
  GHashTableIter iter;
  gpointer key, value;
  gint y = 0;

  ctk_widget_get_allocation (widget, &allocation);

  /* Draw Abs info */
  g_hash_table_iter_init (&iter, data->pointer_info);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      info = value;
      draw_axes_info (cr, info, &allocation);
    }

  g_hash_table_iter_init (&iter, data->touch_info);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      info = value;
      draw_axes_info (cr, info, &allocation);
    }

  /* Draw name, color legend and misc data */
  g_hash_table_iter_init (&iter, data->pointer_info);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      info = value;
      draw_device_info (widget, cr, NULL, &y, info);
    }

  g_hash_table_iter_init (&iter, data->touch_info);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      info = value;
      draw_device_info (widget, cr, key, &y, info);
    }

  return FALSE;
}

static void
update_label_text (CtkWidget   *label,
                   const gchar *text)
{
  gchar *markup = NULL;

  if (text)
    markup = g_strdup_printf ("<span font='48.0'>%s</span>", text);
  ctk_label_set_markup (CTK_LABEL (label), markup);
  g_free (markup);
}

static gboolean
reset_label_text_timeout_cb (gpointer user_data)
{
  CtkWidget *label = user_data;

  update_label_text (label, NULL);
  pad_action_timeout_id = 0;
  return G_SOURCE_REMOVE;
}

static void
update_label_and_timeout (CtkWidget   *label,
                          const gchar *text)
{
  if (pad_action_timeout_id)
    g_source_remove (pad_action_timeout_id);

  update_label_text (label, text);
  pad_action_timeout_id = g_timeout_add (200, reset_label_text_timeout_cb, label);
}

static void
on_action_activate (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  CtkWidget *label = user_data;
  const gchar *result;

  result = g_object_get_data (G_OBJECT (action), "action-result");

  if (!parameter)
    update_label_and_timeout (label, result);
  else
    {
      gchar *str;

      str = g_strdup_printf ("%s %.2f", result, g_variant_get_double (parameter));
      update_label_and_timeout (label, str);
      g_free (str);
    }
}

static void
init_pad_controller (CtkWidget *window,
                     CtkWidget *label)
{
  CtkPadController *pad_controller;
  GSimpleActionGroup *action_group;
  GSimpleAction *action;
  gint i;

  action_group = g_simple_action_group_new ();
  pad_controller = ctk_pad_controller_new (CTK_WINDOW (window),
                                           G_ACTION_GROUP (action_group),
                                           NULL);

  for (i = 0; i < G_N_ELEMENTS (pad_actions); i++)
    {
      if (pad_actions[i].type == CTK_PAD_ACTION_BUTTON)
        {
          action = g_simple_action_new (pad_actions[i].action_name, NULL);
        }
      else
        {
          action = g_simple_action_new_stateful (pad_actions[i].action_name,
                                                 G_VARIANT_TYPE_DOUBLE, NULL);
        }

      g_signal_connect (action, "activate",
                        G_CALLBACK (on_action_activate), label);
      g_object_set_data (G_OBJECT (action), "action-result",
                         (gpointer) pad_action_results[i]);
      g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (action));
      g_object_unref (action);
    }

  ctk_pad_controller_set_action_entries (pad_controller, pad_actions,
                                         G_N_ELEMENTS (pad_actions));
  g_object_set_data_full (G_OBJECT (window), "pad-controller",
                          pad_controller, g_object_unref);

  g_object_unref (action_group);
}

CtkWidget *
do_event_axes (CtkWidget *toplevel)
{
  static CtkWidget *window = NULL;

  if (!window)
    {
      EventData *event_data;
      CtkWidget *box, *label;

      window = ctk_window_new (CTK_WINDOW_TOPLEVEL);
      ctk_window_set_title (CTK_WINDOW (window), "Event Axes");
      ctk_window_set_default_size (CTK_WINDOW (window), 400, 400);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (ctk_widget_destroyed), &window);

      box = ctk_event_box_new ();
      ctk_container_add (CTK_CONTAINER (window), box);
      ctk_widget_set_support_multidevice (box, TRUE);
      ctk_widget_add_events (box,
			     CDK_POINTER_MOTION_MASK |
			     CDK_BUTTON_PRESS_MASK |
			     CDK_BUTTON_RELEASE_MASK |
			     CDK_SMOOTH_SCROLL_MASK |
			     CDK_ENTER_NOTIFY_MASK |
			     CDK_LEAVE_NOTIFY_MASK |
			     CDK_TOUCH_MASK);

      event_data = event_data_new ();
      g_object_set_data_full (G_OBJECT (box), "ctk-demo-event-data",
                              event_data, (GDestroyNotify) event_data_free);

      g_signal_connect (box, "event",
                        G_CALLBACK (event_cb), event_data);
      g_signal_connect (box, "draw",
                        G_CALLBACK (draw_cb), event_data);

      label = ctk_label_new ("");
      ctk_label_set_use_markup (CTK_LABEL (label), TRUE);
      ctk_container_add (CTK_CONTAINER (box), label);

      init_pad_controller (window, label);
    }

  if (!ctk_widget_get_visible (window))
    ctk_widget_show_all (window);
  else
    ctk_widget_destroy (window);

  return window;
}
