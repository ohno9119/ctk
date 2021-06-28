/*
 * Copyright © 2011 William Hua, Ryan Lortie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: William Hua <william@attente.ca>
 *         Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "ctkapplicationprivate.h"
#include "ctkmenutracker.h"
#include "ctkicontheme.h"
#include "ctktoolbarprivate.h"
#include "ctkquartz.h"

#include <cdk/quartz/cdkquartz.h>

#import <Cocoa/Cocoa.h>

#define ICON_SIZE 16

#define BLACK               "#000000"
#define TANGO_CHAMELEON_3   "#4e9a06"
#define TANGO_ORANGE_2      "#f57900"
#define TANGO_SCARLET_RED_2 "#cc0000"

@interface GNSMenu : NSMenu
{
  CtkMenuTracker *tracker;
}

- (id)initWithTitle:(NSString *)title model:(GMenuModel *)model observable:(CtkActionObservable *)observable;

- (id)initWithTitle:(NSString *)title trackerItem:(CtkMenuTrackerItem *)trackerItem;

@end

@interface NSMenuItem (CtkMenuTrackerItem)

+ (id)menuItemForTrackerItem:(CtkMenuTrackerItem *)trackerItem;

@end

@interface GNSMenuItem : NSMenuItem
{
  CtkMenuTrackerItem *trackerItem;
  gulong trackerItemChangedHandler;
  GCancellable *cancellable;
  BOOL isSpecial;
}

- (id)initWithTrackerItem:(CtkMenuTrackerItem *)aTrackerItem;

- (void)didChangeLabel;
- (void)didChangeIcon;
- (void)didChangeVisible;
- (void)didChangeToggled;
- (void)didChangeAccel;

- (void)didSelectItem:(id)sender;
- (BOOL)validateMenuItem:(NSMenuItem *)menuItem;

@end

static void
tracker_item_changed (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  GNSMenuItem *item = user_data;
  const gchar *name = g_param_spec_get_name (pspec);

  if (name != NULL)
    {
      if (g_str_equal (name, "label"))
        [item didChangeLabel];
      else if (g_str_equal (name, "icon"))
        [item didChangeIcon];
      else if (g_str_equal (name, "is-visible"))
        [item didChangeVisible];
      else if (g_str_equal (name, "toggled"))
        [item didChangeToggled];
      else if (g_str_equal (name, "accel"))
        [item didChangeAccel];
    }
}

static void
icon_loaded (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  CtkIconInfo *info = CTK_ICON_INFO (object);
  GNSMenuItem *item = user_data;
  GError *error = NULL;
  GdkPixbuf *pixbuf;
  gint scale = 1;

#ifdef AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER
       /* we need a run-time check for the backingScaleFactor selector because we
        * may be compiling on a 10.7 framework, but targeting a 10.6 one
        */
      if ([[NSScreen mainScreen] respondsToSelector:@selector(backingScaleFactor)])
        scale = roundf ([[NSScreen mainScreen] backingScaleFactor]);
#endif

  pixbuf = ctk_icon_info_load_symbolic_finish (info, result, NULL, &error);

  if (pixbuf != NULL)
    {
      cairo_t *cr;
      cairo_surface_t *surface;
      NSImage *image;

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            gdk_pixbuf_get_width (pixbuf),
                                            gdk_pixbuf_get_height (pixbuf));

      cr = cairo_create (surface);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
      g_object_unref (pixbuf);

      cairo_surface_set_device_scale (surface, scale, scale);
      image = _ctk_quartz_create_image_from_surface (surface);
      cairo_surface_destroy (surface);

      if (image != NULL)
        [item setImage:image];
      else
        [item setImage:nil];
    }
  else
    {
      /* on failure to load, clear the old icon */
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        [item setImage:nil];

      g_error_free (error);
    }
}

@implementation GNSMenuItem

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
  return ctk_menu_tracker_item_get_sensitive (trackerItem) ? YES : NO;
}

- (id)initWithTrackerItem:(CtkMenuTrackerItem *)aTrackerItem
{
  self = [super initWithTitle:@""
                       action:@selector(didSelectItem:)
                keyEquivalent:@""];

  if (self != nil)
    {
      const gchar *special = ctk_menu_tracker_item_get_special (aTrackerItem);

      if (special && g_str_equal (special, "hide-this"))
        {
          [self setAction:@selector(hide:)];
          [self setTarget:NSApp];
        }
      else if (special && g_str_equal (special, "hide-others"))
        {
          [self setAction:@selector(hideOtherApplications:)];
          [self setTarget:NSApp];
        }
      else if (special && g_str_equal (special, "show-all"))
        {
          [self setAction:@selector(unhideAllApplications:)];
          [self setTarget:NSApp];
        }
      else if (special && g_str_equal (special, "services-submenu"))
        {
          [self setSubmenu:[[[NSMenu alloc] init] autorelease]];
          [NSApp setServicesMenu:[self submenu]];
          [self setTarget:self];
        }
      else
        [self setTarget:self];

      trackerItem = g_object_ref (aTrackerItem);
      trackerItemChangedHandler = g_signal_connect (trackerItem, "notify", G_CALLBACK (tracker_item_changed), self);
      isSpecial = (special != NULL);

      [self didChangeLabel];
      [self didChangeIcon];
      [self didChangeVisible];
      [self didChangeToggled];
      [self didChangeAccel];

      if (ctk_menu_tracker_item_get_has_link (trackerItem, G_MENU_LINK_SUBMENU))
        [self setSubmenu:[[[GNSMenu alloc] initWithTitle:[self title] trackerItem:trackerItem] autorelease]];
    }

  return self;
}

- (void)dealloc
{
  if (cancellable != NULL)
    {
      g_cancellable_cancel (cancellable);
      g_clear_object (&cancellable);
    }

  g_signal_handler_disconnect (trackerItem, trackerItemChangedHandler);
  g_object_unref (trackerItem);

  [super dealloc];
}

- (void)didChangeLabel
{
  gchar *label = _ctk_toolbar_elide_underscores (ctk_menu_tracker_item_get_label (trackerItem));

  NSString *title = [NSString stringWithUTF8String:label ? : ""];

  if (isSpecial)
    {
      NSRange range = [title rangeOfString:@"%s"];

      if (range.location != NSNotFound)
        {
          NSBundle *bundle = [NSBundle mainBundle];
          NSString *name = [[bundle localizedInfoDictionary] objectForKey:@"CFBundleName"];

          if (name == nil)
            name = [[bundle infoDictionary] objectForKey:@"CFBundleName"];

          if (name == nil)
            name = [[NSProcessInfo processInfo] processName];

          if (name != nil)
            title = [title stringByReplacingCharactersInRange:range withString:name];
        }
    }

  [self setTitle:title];

  g_free (label);
}

- (void)didChangeIcon
{
  GIcon *icon = ctk_menu_tracker_item_get_icon (trackerItem);

  if (cancellable != NULL)
    {
      g_cancellable_cancel (cancellable);
      g_clear_object (&cancellable);
    }

  if (icon != NULL)
    {
      static gboolean parsed;

      static CdkRGBA foreground;
      static CdkRGBA success;
      static CdkRGBA warning;
      static CdkRGBA error;

      CtkIconTheme *theme;
      CtkIconInfo *info;
      gint scale = 1;

      if (!parsed)
        {
          cdk_rgba_parse (&foreground, BLACK);
          cdk_rgba_parse (&success, TANGO_CHAMELEON_3);
          cdk_rgba_parse (&warning, TANGO_ORANGE_2);
          cdk_rgba_parse (&error, TANGO_SCARLET_RED_2);

          parsed = TRUE;
        }

      theme = ctk_icon_theme_get_default ();

#ifdef AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER
       /* we need a run-time check for the backingScaleFactor selector because we
        * may be compiling on a 10.7 framework, but targeting a 10.6 one
        */
      if ([[NSScreen mainScreen] respondsToSelector:@selector(backingScaleFactor)])
        scale = roundf ([[NSScreen mainScreen] backingScaleFactor]);
#endif
      info = ctk_icon_theme_lookup_by_gicon_for_scale (theme, icon, ICON_SIZE, scale, CTK_ICON_LOOKUP_USE_BUILTIN);

      if (info != NULL)
        {
          cancellable = g_cancellable_new ();
          ctk_icon_info_load_symbolic_async (info, &foreground, &success, &warning, &error,
                                             cancellable, icon_loaded, self);
          g_object_unref (info);
          return;
        }
    }

  [self setImage:nil];
}

- (void)didChangeVisible
{
  [self setHidden:ctk_menu_tracker_item_get_is_visible (trackerItem) ? NO : YES];
}

- (void)didChangeToggled
{
  [self setState:ctk_menu_tracker_item_get_toggled (trackerItem) ? NSOnState : NSOffState];
}

- (void)didChangeAccel
{
  const gchar *accel = ctk_menu_tracker_item_get_accel (trackerItem);

  if (accel != NULL)
    {
      guint key;
      CdkModifierType mask;
      unichar character;
      NSUInteger modifiers;

      ctk_accelerator_parse (accel, &key, &mask);

      character = cdk_quartz_get_key_equivalent (key);
      [self setKeyEquivalent:[NSString stringWithCharacters:&character length:1]];

      modifiers = 0;
      if (mask & CDK_SHIFT_MASK)
        modifiers |= NSShiftKeyMask;
      if (mask & CDK_CONTROL_MASK)
        modifiers |= NSControlKeyMask;
      if (mask & CDK_MOD1_MASK)
        modifiers |= NSAlternateKeyMask;
      if (mask & CDK_META_MASK)
        modifiers |= NSCommandKeyMask;
      [self setKeyEquivalentModifierMask:modifiers];
    }
  else
    {
      [self setKeyEquivalent:@""];
      [self setKeyEquivalentModifierMask:0];
    }
}

- (void)didSelectItem:(id)sender
{
  ctk_menu_tracker_item_activated (trackerItem);
}

@end

@implementation NSMenuItem (CtkMenuTrackerItem)

+ (id)menuItemForTrackerItem:(CtkMenuTrackerItem *)trackerItem
{
  if (ctk_menu_tracker_item_get_is_separator (trackerItem))
    return [NSMenuItem separatorItem];

  return [[[GNSMenuItem alloc] initWithTrackerItem:trackerItem] autorelease];
}

@end

static void
menu_item_inserted (CtkMenuTrackerItem *item,
                    gint                position,
                    gpointer            user_data)
{
  GNSMenu *menu = user_data;

  [menu insertItem:[NSMenuItem menuItemForTrackerItem:item] atIndex:position];
}

static void
menu_item_removed (gint     position,
                   gpointer user_data)
{
  GNSMenu *menu = user_data;
  @try
  {
    [menu removeItemAtIndex:position];
  }
  @catch(NSException *err)
    {
      g_critical("GNSMenu removeItemAtIndex: %d raised exception %s", position,
                 [[err reason] UTF8String]);
    }
}

@implementation GNSMenu

- (id)initWithTitle:(NSString *)title model:(GMenuModel *)model observable:(CtkActionObservable *)observable
{
  self = [super initWithTitle:title];

  if (self != nil)
    {
      tracker = ctk_menu_tracker_new (observable,
                                      model,
                                      NO,
                                      YES,
                                      YES,
                                      NULL,
                                      menu_item_inserted,
                                      menu_item_removed,
                                      self);
    }

  return self;
}

- (id)initWithTitle:(NSString *)title trackerItem:(CtkMenuTrackerItem *)trackerItem
{
  self = [super initWithTitle:title];

  if (self != nil)
    {
      tracker = ctk_menu_tracker_new_for_item_link (trackerItem,
                                                       G_MENU_LINK_SUBMENU,
                                                       YES,
                                                       YES,
                                                       menu_item_inserted,
                                                       menu_item_removed,
                                                       self);
    }

  return self;
}

- (void)dealloc
{
  ctk_menu_tracker_free (tracker);

  [super dealloc];
}

@end

void
ctk_application_impl_quartz_setup_menu (GMenuModel     *model,
                                        CtkActionMuxer *muxer)
{
  NSMenu *menu;

  if (model != NULL)
    menu = [[GNSMenu alloc] initWithTitle:@"Main Menu" model:model observable:CTK_ACTION_OBSERVABLE (muxer)];
  else
    menu = [[NSMenu alloc] init];

  [NSApp setMainMenu:menu];
  [menu release];
}
