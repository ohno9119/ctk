/* CDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"

#include "cdkx11devicemanager-core.h"
#include "cdkdevicemanagerprivate-core.h"
#include "cdkx11device-core.h"

#include "cdkdeviceprivate.h"
#include "cdkseatdefaultprivate.h"
#include "cdkdisplayprivate.h"
#include "cdkeventtranslator.h"
#include "cdkprivate-x11.h"
#include "cdkkeysyms.h"

#include "fallback-memdup.h"


#define HAS_FOCUS(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

static void    cdk_x11_device_manager_core_finalize    (GObject *object);
static void    cdk_x11_device_manager_core_constructed (GObject *object);

static GList * cdk_x11_device_manager_core_list_devices (CdkDeviceManager *device_manager,
                                                         CdkDeviceType     type);
static CdkDevice * cdk_x11_device_manager_core_get_client_pointer (CdkDeviceManager *device_manager);

static void     cdk_x11_device_manager_event_translator_init (CdkEventTranslatorIface *iface);

static gboolean cdk_x11_device_manager_core_translate_event  (CdkEventTranslator *translator,
                                                              CdkDisplay         *display,
                                                              CdkEvent           *event,
                                                              XEvent             *xevent);


G_DEFINE_TYPE_WITH_CODE (CdkX11DeviceManagerCore, cdk_x11_device_manager_core, CDK_TYPE_DEVICE_MANAGER,
                         G_IMPLEMENT_INTERFACE (CDK_TYPE_EVENT_TRANSLATOR,
                                                cdk_x11_device_manager_event_translator_init))

static void
cdk_x11_device_manager_core_class_init (CdkX11DeviceManagerCoreClass *klass)
{
  CdkDeviceManagerClass *device_manager_class = CDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cdk_x11_device_manager_core_finalize;
  object_class->constructed = cdk_x11_device_manager_core_constructed;
  device_manager_class->list_devices = cdk_x11_device_manager_core_list_devices;
  device_manager_class->get_client_pointer = cdk_x11_device_manager_core_get_client_pointer;
}

static void
cdk_x11_device_manager_event_translator_init (CdkEventTranslatorIface *iface)
{
  iface->translate_event = cdk_x11_device_manager_core_translate_event;
}

static CdkDevice *
create_core_pointer (CdkDeviceManager *device_manager,
                     CdkDisplay       *display)
{
  return g_object_new (CDK_TYPE_X11_DEVICE_CORE,
                       "name", "Core Pointer",
                       "type", CDK_DEVICE_TYPE_MASTER,
                       "input-source", CDK_SOURCE_MOUSE,
                       "input-mode", CDK_MODE_SCREEN,
                       "has-cursor", TRUE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static CdkDevice *
create_core_keyboard (CdkDeviceManager *device_manager,
                      CdkDisplay       *display)
{
  return g_object_new (CDK_TYPE_X11_DEVICE_CORE,
                       "name", "Core Keyboard",
                       "type", CDK_DEVICE_TYPE_MASTER,
                       "input-source", CDK_SOURCE_KEYBOARD,
                       "input-mode", CDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static void
cdk_x11_device_manager_core_init (CdkX11DeviceManagerCore *device_manager)
{
}

static void
cdk_x11_device_manager_core_finalize (GObject *object)
{
  CdkX11DeviceManagerCore *device_manager_core;

  device_manager_core = CDK_X11_DEVICE_MANAGER_CORE (object);

  g_object_unref (device_manager_core->core_pointer);
  g_object_unref (device_manager_core->core_keyboard);

  G_OBJECT_CLASS (cdk_x11_device_manager_core_parent_class)->finalize (object);
}

static void
cdk_x11_device_manager_core_constructed (GObject *object)
{
  CdkX11DeviceManagerCore *device_manager;
  CdkDisplay *display;

  device_manager = CDK_X11_DEVICE_MANAGER_CORE (object);
  display = cdk_device_manager_get_display (CDK_DEVICE_MANAGER (object));
  device_manager->core_pointer = create_core_pointer (CDK_DEVICE_MANAGER (device_manager), display);
  device_manager->core_keyboard = create_core_keyboard (CDK_DEVICE_MANAGER (device_manager), display);

  _cdk_device_set_associated_device (device_manager->core_pointer, device_manager->core_keyboard);
  _cdk_device_set_associated_device (device_manager->core_keyboard, device_manager->core_pointer);

  /* We expect subclasses to handle their own seats */
  if (G_OBJECT_TYPE (object) == CDK_TYPE_X11_DEVICE_MANAGER_CORE)
    {
      CdkSeat *seat;

      seat = cdk_seat_default_new_for_master_pair (device_manager->core_pointer,
                                                   device_manager->core_keyboard);

      cdk_display_add_seat (display, seat);
      g_object_unref (seat);
  }
}

static void
translate_key_event (CdkDisplay              *display,
                     CdkX11DeviceManagerCore *device_manager,
                     CdkEvent                *event,
                     XEvent                  *xevent)
{
  CdkKeymap *keymap = cdk_keymap_get_for_display (display);
  CdkModifierType consumed, state;

  event->key.type = xevent->xany.type == KeyPress ? CDK_KEY_PRESS : CDK_KEY_RELEASE;
  event->key.time = xevent->xkey.time;
  cdk_event_set_device (event, device_manager->core_keyboard);

  event->key.state = (CdkModifierType) xevent->xkey.state;
  event->key.group = cdk_x11_keymap_get_group_for_state (keymap, xevent->xkey.state);
  event->key.hardware_keycode = xevent->xkey.keycode;
  cdk_event_set_scancode (event, xevent->xkey.keycode);

  event->key.keyval = CDK_KEY_VoidSymbol;

  cdk_keymap_translate_keyboard_state (keymap,
                                       event->key.hardware_keycode,
                                       event->key.state,
                                       event->key.group,
                                       &event->key.keyval,
                                       NULL, NULL, &consumed);

  state = event->key.state & ~consumed;
  _cdk_x11_keymap_add_virt_mods (keymap, &state);
  event->key.state |= state;

  event->key.is_modifier = cdk_x11_keymap_key_is_modifier (keymap, event->key.hardware_keycode);

  _cdk_x11_event_translate_keyboard_string (&event->key);

#ifdef G_ENABLE_DEBUG
  if (CDK_DEBUG_CHECK (EVENTS))
    {
      g_message ("%s:\t\twindow: %ld     key: %12s  %d",
                 event->type == CDK_KEY_PRESS ? "key press  " : "key release",
                 xevent->xkey.window,
                 event->key.keyval ? cdk_keyval_name (event->key.keyval) : "(none)",
                 event->key.keyval);

      if (event->key.length > 0)
        g_message ("\t\tlength: %4d string: \"%s\"",
                   event->key.length, event->key.string);
    }
#endif /* G_ENABLE_DEBUG */
  return;
}

#ifdef G_ENABLE_DEBUG
static const char notify_modes[][19] = {
  "NotifyNormal",
  "NotifyGrab",
  "NotifyUngrab",
  "NotifyWhileGrabbed"
};

static const char notify_details[][23] = {
  "NotifyAncestor",
  "NotifyVirtual",
  "NotifyInferior",
  "NotifyNonlinear",
  "NotifyNonlinearVirtual",
  "NotifyPointer",
  "NotifyPointerRoot",
  "NotifyDetailNone"
};
#endif

static void
set_user_time (CdkWindow *window,
               CdkEvent  *event)
{
  g_return_if_fail (event != NULL);

  window = cdk_window_get_toplevel (event->any.window);
  g_return_if_fail (CDK_IS_WINDOW (window));

  /* If an event doesn't have a valid timestamp, we shouldn't use it
   * to update the latest user interaction time.
   */
  if (cdk_event_get_time (event) != CDK_CURRENT_TIME)
    cdk_x11_window_set_user_time (cdk_window_get_toplevel (window),
                                  cdk_event_get_time (event));
}

static gboolean
set_screen_from_root (CdkDisplay *display,
                      CdkEvent   *event,
                      Window      xrootwin)
{
  CdkScreen *screen;

  screen = _cdk_x11_display_screen_for_xrootwin (display, xrootwin);

  if (screen)
    {
      cdk_event_set_screen (event, screen);

      return TRUE;
    }

  return FALSE;
}

static CdkCrossingMode
translate_crossing_mode (int mode)
{
  switch (mode)
    {
    case NotifyNormal:
      return CDK_CROSSING_NORMAL;
    case NotifyGrab:
      return CDK_CROSSING_GRAB;
    case NotifyUngrab:
      return CDK_CROSSING_UNGRAB;
    default:
      g_assert_not_reached ();
    }
}

static CdkNotifyType
translate_notify_type (int detail)
{
  switch (detail)
    {
    case NotifyInferior:
      return CDK_NOTIFY_INFERIOR;
    case NotifyAncestor:
      return CDK_NOTIFY_ANCESTOR;
    case NotifyVirtual:
      return CDK_NOTIFY_VIRTUAL;
    case NotifyNonlinear:
      return CDK_NOTIFY_NONLINEAR;
    case NotifyNonlinearVirtual:
      return CDK_NOTIFY_NONLINEAR_VIRTUAL;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
is_parent_of (CdkWindow *parent,
              CdkWindow *child)
{
  CdkWindow *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
        return TRUE;

      w = cdk_window_get_parent (w);
    }

  return FALSE;
}

static CdkWindow *
get_event_window (CdkEventTranslator *translator,
                  XEvent             *xevent)
{
  CdkDeviceManager *device_manager;
  CdkDisplay *display;
  CdkWindow *window;

  device_manager = CDK_DEVICE_MANAGER (translator);
  display = cdk_device_manager_get_display (device_manager);
  window = cdk_x11_window_lookup_for_display (display, xevent->xany.window);

  /* Apply keyboard grabs to non-native windows */
  if (xevent->type == KeyPress || xevent->type == KeyRelease)
    {
      CdkDeviceGrabInfo *info;
      gulong serial;

      serial = _cdk_display_get_next_serial (display);
      info = _cdk_display_has_device_grab (display,
                                           CDK_X11_DEVICE_MANAGER_CORE (device_manager)->core_keyboard,
                                           serial);
      if (info &&
          (!is_parent_of (info->window, window) ||
           !info->owner_events))
        {
          /* Report key event against grab window */
          window = info->window;
        }
    }

  return window;
}

static gboolean
cdk_x11_device_manager_core_translate_event (CdkEventTranslator *translator,
                                             CdkDisplay         *display,
                                             CdkEvent           *event,
                                             XEvent             *xevent)
{
  CdkWindowImplX11 *impl;
  CdkX11DeviceManagerCore *device_manager;
  CdkWindow *window;
  gboolean return_val;
  int scale;
  CdkX11Display *display_x11 = CDK_X11_DISPLAY (display);

  device_manager = CDK_X11_DEVICE_MANAGER_CORE (translator);

  window = get_event_window (translator, xevent);

  scale = 1;
  if (window)
    {
      if (CDK_WINDOW_DESTROYED (window) || !CDK_IS_WINDOW (window))
        return FALSE;

      g_object_ref (window);
      impl = CDK_WINDOW_IMPL_X11 (window->impl);
      scale = impl->window_scale;
    }

  event->any.window = window;
  event->any.send_event = xevent->xany.send_event ? TRUE : FALSE;

  if (window && CDK_WINDOW_DESTROYED (window))
    {
      if (xevent->type != DestroyNotify)
        {
          return_val = FALSE;
          goto done;
        }
    }

  if (window &&
      (xevent->type == MotionNotify ||
       xevent->type == ButtonRelease))
    {
      if (_cdk_x11_moveresize_handle_event (xevent))
        {
          return_val = FALSE;
          goto done;
        }
    }

  /* We do a "manual" conversion of the XEvent to a
   *  CdkEvent. The structures are mostly the same so
   *  the conversion is fairly straightforward. We also
   *  optionally print debugging info regarding events
   *  received.
   */

  return_val = TRUE;

  switch (xevent->type)
    {
    case KeyPress:
      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }
      translate_key_event (display, device_manager, event, xevent);
      set_user_time (window, event);
      break;

    case KeyRelease:
      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      /* Emulate detectable auto-repeat by checking to see
       * if the next event is a key press with the same
       * keycode and timestamp, and if so, ignoring the event.
       */

      if (!display_x11->have_xkb_autorepeat && XPending (xevent->xkey.display))
        {
          XEvent next_event;

          XPeekEvent (xevent->xkey.display, &next_event);

          if (next_event.type == KeyPress &&
              next_event.xkey.keycode == xevent->xkey.keycode &&
              next_event.xkey.time == xevent->xkey.time)
            {
              return_val = FALSE;
              break;
            }
        }

      translate_key_event (display, device_manager, event, xevent);
      break;

    case ButtonPress:
      CDK_NOTE (EVENTS,
                g_message ("button press:\t\twindow: %ld  x,y: %d %d  button: %d",
                           xevent->xbutton.window,
                           xevent->xbutton.x, xevent->xbutton.y,
                           xevent->xbutton.button));

      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      /* If we get a ButtonPress event where the button is 4 or 5,
         it's a Scroll event */
      switch (xevent->xbutton.button)
        {
        case 4: /* up */
        case 5: /* down */
        case 6: /* left */
        case 7: /* right */
          event->scroll.type = CDK_SCROLL;

          if (xevent->xbutton.button == 4)
            event->scroll.direction = CDK_SCROLL_UP;
          else if (xevent->xbutton.button == 5)
            event->scroll.direction = CDK_SCROLL_DOWN;
          else if (xevent->xbutton.button == 6)
            event->scroll.direction = CDK_SCROLL_LEFT;
          else
            event->scroll.direction = CDK_SCROLL_RIGHT;

          event->scroll.window = window;
          event->scroll.time = xevent->xbutton.time;
          event->scroll.x = (gdouble) xevent->xbutton.x / scale;
          event->scroll.y = (gdouble) xevent->xbutton.y / scale;
          event->scroll.x_root = (gdouble) xevent->xbutton.x_root / scale;
          event->scroll.y_root = (gdouble) xevent->xbutton.y_root / scale;
          event->scroll.state = (CdkModifierType) xevent->xbutton.state;
          event->scroll.device = device_manager->core_pointer;

          event->scroll.delta_x = 0;
          event->scroll.delta_y = 0;

          if (!set_screen_from_root (display, event, xevent->xbutton.root))
            {
              return_val = FALSE;
              break;
            }

          break;

        default:
          event->button.type = CDK_BUTTON_PRESS;
          event->button.window = window;
          event->button.time = xevent->xbutton.time;
          event->button.x = (gdouble) xevent->xbutton.x / scale;
          event->button.y = (gdouble) xevent->xbutton.y / scale;
          event->button.x_root = (gdouble) xevent->xbutton.x_root / scale;
          event->button.y_root = (gdouble) xevent->xbutton.y_root / scale;
          event->button.axes = NULL;
          event->button.state = (CdkModifierType) xevent->xbutton.state;
          event->button.button = xevent->xbutton.button;
          event->button.device = device_manager->core_pointer;

          if (!set_screen_from_root (display, event, xevent->xbutton.root))
            return_val = FALSE;

          break;
        }

      set_user_time (window, event);

      break;

    case ButtonRelease:
      CDK_NOTE (EVENTS,
                g_message ("button release:\twindow: %ld  x,y: %d %d  button: %d",
                           xevent->xbutton.window,
                           xevent->xbutton.x, xevent->xbutton.y,
                           xevent->xbutton.button));

      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      /* We treat button presses as scroll wheel events, so ignore the release */
      if (xevent->xbutton.button == 4 || xevent->xbutton.button == 5 ||
          xevent->xbutton.button == 6 || xevent->xbutton.button == 7)
        {
          return_val = FALSE;
          break;
        }

      event->button.type = CDK_BUTTON_RELEASE;
      event->button.window = window;
      event->button.time = xevent->xbutton.time;
      event->button.x = (gdouble) xevent->xbutton.x / scale;
      event->button.y = (gdouble) xevent->xbutton.y / scale;
      event->button.x_root = (gdouble) xevent->xbutton.x_root / scale;
      event->button.y_root = (gdouble) xevent->xbutton.y_root / scale;
      event->button.axes = NULL;
      event->button.state = (CdkModifierType) xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      event->button.device = device_manager->core_pointer;

      if (!set_screen_from_root (display, event, xevent->xbutton.root))
        return_val = FALSE;

      break;

    case MotionNotify:
      CDK_NOTE (EVENTS,
                g_message ("motion notify:\t\twindow: %ld  x,y: %d %d  hint: %s",
                           xevent->xmotion.window,
                           xevent->xmotion.x, xevent->xmotion.y,
                           (xevent->xmotion.is_hint) ? "true" : "false"));

      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      event->motion.type = CDK_MOTION_NOTIFY;
      event->motion.window = window;
      event->motion.time = xevent->xmotion.time;
      event->motion.x = (gdouble) xevent->xmotion.x / scale;
      event->motion.y = (gdouble) xevent->xmotion.y / scale;
      event->motion.x_root = (gdouble) xevent->xmotion.x_root / scale;
      event->motion.y_root = (gdouble) xevent->xmotion.y_root / scale;
      event->motion.axes = NULL;
      event->motion.state = (CdkModifierType) xevent->xmotion.state;
      event->motion.is_hint = xevent->xmotion.is_hint;
      event->motion.device = device_manager->core_pointer;

      if (!set_screen_from_root (display, event, xevent->xbutton.root))
        {
          return_val = FALSE;
          break;
        }

      break;

    case EnterNotify:
      CDK_NOTE (EVENTS,
                g_message ("enter notify:\t\twindow: %ld  detail: %d subwin: %ld",
                           xevent->xcrossing.window,
                           xevent->xcrossing.detail,
                           xevent->xcrossing.subwindow));

      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      if (!set_screen_from_root (display, event, xevent->xbutton.root))
        {
          return_val = FALSE;
          break;
        }

      event->crossing.type = CDK_ENTER_NOTIFY;
      event->crossing.window = window;
      cdk_event_set_device (event, device_manager->core_pointer);

      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding CdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
        event->crossing.subwindow = cdk_x11_window_lookup_for_display (display, xevent->xcrossing.subwindow);
      else
        event->crossing.subwindow = NULL;

      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = (gdouble) xevent->xcrossing.x / scale;
      event->crossing.y = (gdouble) xevent->xcrossing.y / scale;
      event->crossing.x_root = (gdouble) xevent->xcrossing.x_root / scale;
      event->crossing.y_root = (gdouble) xevent->xcrossing.y_root / scale;

      event->crossing.mode = translate_crossing_mode (xevent->xcrossing.mode);
      event->crossing.detail = translate_notify_type (xevent->xcrossing.detail);

      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;

      break;

    case LeaveNotify:
      CDK_NOTE (EVENTS,
                g_message ("leave notify:\t\twindow: %ld  detail: %d subwin: %ld",
                           xevent->xcrossing.window,
                           xevent->xcrossing.detail, xevent->xcrossing.subwindow));

      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      if (!set_screen_from_root (display, event, xevent->xbutton.root))
        {
          return_val = FALSE;
          break;
        }

      event->crossing.type = CDK_LEAVE_NOTIFY;
      event->crossing.window = window;
      cdk_event_set_device (event, device_manager->core_pointer);

      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding CdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
        event->crossing.subwindow = cdk_x11_window_lookup_for_display (display, xevent->xcrossing.subwindow);
      else
        event->crossing.subwindow = NULL;

      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = (gdouble) xevent->xcrossing.x / scale;
      event->crossing.y = (gdouble) xevent->xcrossing.y / scale;
      event->crossing.x_root = (gdouble) xevent->xcrossing.x_root / scale;
      event->crossing.y_root = (gdouble) xevent->xcrossing.y_root / scale;

      event->crossing.mode = translate_crossing_mode (xevent->xcrossing.mode);
      event->crossing.detail = translate_notify_type (xevent->xcrossing.detail);

      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;

      break;

    case FocusIn:
    case FocusOut:
      if (window)
        _cdk_device_manager_core_handle_focus (window,
                                               xevent->xfocus.window,
                                               device_manager->core_keyboard,
                                               NULL,
                                               xevent->type == FocusIn,
                                               xevent->xfocus.detail,
                                               xevent->xfocus.mode);
      return_val = FALSE;
      break;
                                              
    default:
        return_val = FALSE;
    }

 done:
  if (return_val)
    {
      if (event->any.window)
        g_object_ref (event->any.window);

      if (((event->any.type == CDK_ENTER_NOTIFY) ||
           (event->any.type == CDK_LEAVE_NOTIFY)) &&
          (event->crossing.subwindow != NULL))
        g_object_ref (event->crossing.subwindow);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.window = NULL;
      event->any.type = CDK_NOTHING;
    }

  if (window)
    g_object_unref (window);

  return return_val;
}

static GList *
cdk_x11_device_manager_core_list_devices (CdkDeviceManager *device_manager,
                                          CdkDeviceType     type)
{
  GList *devices = NULL;

  if (type == CDK_DEVICE_TYPE_MASTER)
    {
      CdkX11DeviceManagerCore *device_manager_core;

      device_manager_core = (CdkX11DeviceManagerCore *) device_manager;
      devices = g_list_prepend (devices, device_manager_core->core_keyboard);
      devices = g_list_prepend (devices, device_manager_core->core_pointer);
    }

  return devices;
}

static CdkDevice *
cdk_x11_device_manager_core_get_client_pointer (CdkDeviceManager *device_manager)
{
  CdkX11DeviceManagerCore *device_manager_core;

  device_manager_core = (CdkX11DeviceManagerCore *) device_manager;
  return device_manager_core->core_pointer;
}

void
_cdk_x11_event_translate_keyboard_string (CdkEventKey *event)
{
  gunichar c = 0;
  gchar buf[7];

  /* Fill in event->string crudely, since various programs
   * depend on it.
   */
  event->string = NULL;

  if (event->keyval != CDK_KEY_VoidSymbol)
    c = cdk_keyval_to_unicode (event->keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;

      /* Apply the control key - Taken from Xlib
       */
      if (event->state & CDK_CONTROL_MASK)
        {
          if ((c >= '@' && c < '\177') || c == ' ') c &= 0x1F;
          else if (c == '2')
            {
              event->string = g_memdup2 ("\0\0", 2);
              event->length = 1;
              buf[0] = '\0';
              return;
            }
          else if (c >= '3' && c <= '7') c -= ('3' - '\033');
          else if (c == '8') c = '\177';
          else if (c == '/') c = '_' & 0x1F;
        }

      len = g_unichar_to_utf8 (c, buf);
      buf[len] = '\0';

      event->string = g_locale_from_utf8 (buf, len,
                                          NULL, &bytes_written,
                                          NULL);
      if (event->string)
        event->length = bytes_written;
    }
  else if (event->keyval == CDK_KEY_Escape)
    {
      event->length = 1;
      event->string = g_strdup ("\033");
    }
  else if (event->keyval == CDK_KEY_Return ||
          event->keyval == CDK_KEY_KP_Enter)
    {
      event->length = 1;
      event->string = g_strdup ("\r");
    }

  if (!event->string)
    {
      event->length = 0;
      event->string = g_strdup ("");
    }
}

/* We only care about focus events that indicate that _this_
 * window (not a ancestor or child) got or lost the focus
 */
void
_cdk_device_manager_core_handle_focus (CdkWindow *window,
                                       Window     original,
                                       CdkDevice *device,
                                       CdkDevice *source_device,
                                       gboolean   focus_in,
                                       int        detail,
                                       int        mode)
{
  CdkToplevelX11 *toplevel;
  CdkX11Screen *x11_screen;
  gboolean had_focus;

  g_return_if_fail (CDK_IS_WINDOW (window));
  g_return_if_fail (CDK_IS_DEVICE (device));
  g_return_if_fail (source_device == NULL || CDK_IS_DEVICE (source_device));

  CDK_NOTE (EVENTS,
            g_message ("focus out:\t\twindow: %ld, detail: %s, mode: %s",
                       CDK_WINDOW_XID (window),
                       notify_details[detail],
                       notify_modes[mode]));

  toplevel = _cdk_x11_window_get_toplevel (window);

  if (!toplevel)
    return;

  if (toplevel->focus_window == original)
    return;

  had_focus = HAS_FOCUS (toplevel);
  x11_screen = CDK_X11_SCREEN (cdk_window_get_screen (window));

  switch (detail)
    {
    case NotifyAncestor:
    case NotifyVirtual:
      /* When the focus moves from an ancestor of the window to
       * the window or a descendent of the window, *and* the
       * pointer is inside the window, then we were previously
       * receiving keystroke events in the has_pointer_focus
       * case and are now receiving them in the
       * has_focus_window case.
       */
      if (toplevel->has_pointer &&
          !x11_screen->wmspec_check_window &&
          mode != NotifyGrab &&
#ifdef XINPUT_2
	  mode != XINotifyPassiveGrab &&
	  mode != XINotifyPassiveUngrab &&
#endif /* XINPUT_2 */
          mode != NotifyUngrab)
        toplevel->has_pointer_focus = (focus_in) ? FALSE : TRUE;

      /* fall through */
    case NotifyNonlinear:
    case NotifyNonlinearVirtual:
      if (mode != NotifyGrab &&
#ifdef XINPUT_2
	  mode != XINotifyPassiveGrab &&
	  mode != XINotifyPassiveUngrab &&
#endif /* XINPUT_2 */
          mode != NotifyUngrab)
        toplevel->has_focus_window = (focus_in) ? TRUE : FALSE;
      /* We pretend that the focus moves to the grab
       * window, so we pay attention to NotifyGrab
       * NotifyUngrab, and ignore NotifyWhileGrabbed
       */
      if (mode != NotifyWhileGrabbed)
        toplevel->has_focus = (focus_in) ? TRUE : FALSE;
      break;
    case NotifyPointer:
      /* The X server sends NotifyPointer/NotifyGrab,
       * but the pointer focus is ignored while a
       * grab is in effect
       */
      if (!x11_screen->wmspec_check_window &&
          mode != NotifyGrab &&
#ifdef XINPUT_2
	  mode != XINotifyPassiveGrab &&
	  mode != XINotifyPassiveUngrab &&
#endif /* XINPUT_2 */
          mode != NotifyUngrab)
        toplevel->has_pointer_focus = (focus_in) ? TRUE : FALSE;
      break;
    case NotifyInferior:
    case NotifyPointerRoot:
    case NotifyDetailNone:
    default:
      break;
    }

  if (HAS_FOCUS (toplevel) != had_focus)
    {
      CdkEvent *event;

      event = cdk_event_new (CDK_FOCUS_CHANGE);
      event->focus_change.window = g_object_ref (window);
      event->focus_change.send_event = FALSE;
      event->focus_change.in = focus_in;
      cdk_event_set_device (event, device);
      if (source_device)
        cdk_event_set_source_device (event, source_device);

      cdk_event_put (event);
      cdk_event_free (event);
    }
}

