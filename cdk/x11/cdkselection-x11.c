/* CDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "cdkselection.h"
#include "cdkproperty.h"
#include "cdkprivate.h"
#include "cdkprivate-x11.h"
#include "cdkdisplay-x11.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

#include "fallback-memdup.h"

typedef struct _OwnerInfo OwnerInfo;

struct _OwnerInfo
{
  CdkAtom    selection;
  CdkWindow *owner;
  gulong     serial;
};

static GSList *owner_list;

/* When a window is destroyed we check if it is the owner
 * of any selections. This is somewhat inefficient, but
 * owner_list is typically short, and it is a low memory,
 * low code solution
 */
void
_cdk_x11_selection_window_destroyed (CdkWindow *window)
{
  GSList *tmp_list = owner_list;
  while (tmp_list)
    {
      OwnerInfo *info = tmp_list->data;
      tmp_list = tmp_list->next;

      if (info->owner == window)
        {
          owner_list = g_slist_remove (owner_list, info);
          g_free (info);
        }
    }
}

/* We only pass through those SelectionClear events that actually
 * reflect changes to the selection owner that we didn’t make ourself.
 */
gboolean
_cdk_x11_selection_filter_clear_event (XSelectionClearEvent *event)
{
  GSList *tmp_list = owner_list;
  CdkDisplay *display = cdk_x11_lookup_xdisplay (event->display);

  while (tmp_list)
    {
      OwnerInfo *info = tmp_list->data;

      if (cdk_window_get_display (info->owner) == display &&
          info->selection == cdk_x11_xatom_to_atom_for_display (display, event->selection))
        {
          if ((CDK_WINDOW_XID (info->owner) == event->window &&
               event->serial >= info->serial))
            {
              owner_list = g_slist_remove (owner_list, info);
              g_free (info);
              return TRUE;
            }
          else
            return FALSE;
        }
      tmp_list = tmp_list->next;
    }

  return FALSE;
}

gboolean
_cdk_x11_display_set_selection_owner (CdkDisplay *display,
                                      CdkWindow  *owner,
                                      CdkAtom     selection,
                                      guint32     time,
                                      gboolean    send_event)
{
  Display *xdisplay;
  Window xwindow;
  Atom xselection;
  GSList *tmp_list;
  OwnerInfo *info;

  if (cdk_display_is_closed (display))
    return FALSE;

  if (owner)
    {
      if (CDK_WINDOW_DESTROYED (owner) || !CDK_WINDOW_IS_X11 (owner))
        return FALSE;

      cdk_window_ensure_native (owner);
      xdisplay = CDK_WINDOW_XDISPLAY (owner);
      xwindow = CDK_WINDOW_XID (owner);
    }
  else
    {
      xdisplay = CDK_DISPLAY_XDISPLAY (display);
      xwindow = None;
    }

  xselection = cdk_x11_atom_to_xatom_for_display (display, selection);

  tmp_list = owner_list;
  while (tmp_list)
    {
      info = tmp_list->data;
      if (info->selection == selection)
        {
          owner_list = g_slist_remove (owner_list, info);
          g_free (info);
          break;
        }
      tmp_list = tmp_list->next;
    }

  if (owner)
    {
      info = g_new (OwnerInfo, 1);
      info->owner = owner;
      info->serial = NextRequest (CDK_WINDOW_XDISPLAY (owner));
      info->selection = selection;

      owner_list = g_slist_prepend (owner_list, info);
    }

  XSetSelectionOwner (xdisplay, xselection, xwindow, time);

  return (XGetSelectionOwner (xdisplay, xselection) == xwindow);
}

CdkWindow *
_cdk_x11_display_get_selection_owner (CdkDisplay *display,
                                      CdkAtom     selection)
{
  Window xwindow;

  if (cdk_display_is_closed (display))
    return NULL;

  xwindow = XGetSelectionOwner (CDK_DISPLAY_XDISPLAY (display),
                                cdk_x11_atom_to_xatom_for_display (display,
                                                                   selection));
  if (xwindow == None)
    return NULL;

  return cdk_x11_window_lookup_for_display (display, xwindow);
}

void
_cdk_x11_display_convert_selection (CdkDisplay *display,
                                    CdkWindow  *requestor,
                                    CdkAtom     selection,
                                    CdkAtom     target,
                                    guint32     time)
{
  g_return_if_fail (selection != CDK_NONE);

  if (CDK_WINDOW_DESTROYED (requestor) || !CDK_WINDOW_IS_X11 (requestor))
    return;

  cdk_window_ensure_native (requestor);

  XConvertSelection (CDK_WINDOW_XDISPLAY (requestor),
                     cdk_x11_atom_to_xatom_for_display (display, selection),
                     cdk_x11_atom_to_xatom_for_display (display, target),
                     cdk_x11_get_xatom_by_name_for_display (display, "CDK_SELECTION"),
                     CDK_WINDOW_XID (requestor), time);
}

gint
_cdk_x11_display_get_selection_property (CdkDisplay  *display,
                                         CdkWindow   *requestor,
                                         guchar     **data,
                                         CdkAtom     *ret_type,
                                         gint        *ret_format)
{
  gulong nitems;
  gulong nbytes;
  gulong length = 0;
  Atom prop_type;
  gint prop_format;
  guchar *t = NULL;

  if (CDK_WINDOW_DESTROYED (requestor) || !CDK_WINDOW_IS_X11 (requestor))
    goto err;

  t = NULL;

  /* We can't delete the selection here, because it might be the INCR
     protocol, in which case the client has to make sure they'll be
     notified of PropertyChange events _before_ the property is deleted.
     Otherwise there's no guarantee we'll win the race ... */
  if (XGetWindowProperty (CDK_WINDOW_XDISPLAY (requestor),
                          CDK_WINDOW_XID (requestor),
                          cdk_x11_get_xatom_by_name_for_display (display, "CDK_SELECTION"),
                          0, 0x1FFFFFFF /* MAXINT32 / 4 */, False,
                          AnyPropertyType, &prop_type, &prop_format,
                          &nitems, &nbytes, &t) != Success)
    goto err;

  if (prop_type != None)
    {
      if (ret_type)
        *ret_type = cdk_x11_xatom_to_atom_for_display (display, prop_type);
      if (ret_format)
        *ret_format = prop_format;

      if (prop_type == XA_ATOM ||
          prop_type == cdk_x11_get_xatom_by_name_for_display (display, "ATOM_PAIR"))
        {
          Atom* atoms = (Atom*) t;
          CdkAtom* atoms_dest;
          gint num_atom, i;

          if (prop_format != 32)
            goto err;

          num_atom = nitems;
          length = sizeof (CdkAtom) * num_atom + 1;

          if (data)
            {
              *data = g_malloc (length);
              (*data)[length - 1] = '\0';
              atoms_dest = (CdkAtom *)(*data);

              for (i=0; i < num_atom; i++)
                atoms_dest[i] = cdk_x11_xatom_to_atom_for_display (display, atoms[i]);
            }
        }
      else
        {
          switch (prop_format)
            {
            case 8:
              length = nitems;
              break;
            case 16:
              length = sizeof(short) * nitems;
              break;
            case 32:
              length = sizeof(long) * nitems;
              break;
            default:
              g_assert_not_reached ();
              break;
            }

          /* Add on an extra byte to handle null termination.  X guarantees
             that t will be 1 longer than nitems and null terminated */
          length += 1;

          if (data)
            *data = g_memdup2 (t, length);
        }

      if (t)
        XFree (t);

      return length - 1;
    }

 err:
  if (ret_type)
    *ret_type = CDK_NONE;
  if (ret_format)
    *ret_format = 0;
  if (data)
    *data = NULL;

  return 0;
}

void
_cdk_x11_display_send_selection_notify (CdkDisplay       *display,
                                        CdkWindow        *requestor,
                                        CdkAtom          selection,
                                        CdkAtom          target,
                                        CdkAtom          property,
                                        guint32          time)
{
  XSelectionEvent xevent;

  xevent.type = SelectionNotify;
  xevent.serial = 0;
  xevent.send_event = True;
  xevent.requestor = CDK_WINDOW_XID (requestor);
  xevent.selection = cdk_x11_atom_to_xatom_for_display (display, selection);
  xevent.target = cdk_x11_atom_to_xatom_for_display (display, target);
  if (property == CDK_NONE)
    xevent.property = None;
  else
    xevent.property = cdk_x11_atom_to_xatom_for_display (display, property);
  xevent.time = time;

  _cdk_x11_display_send_xevent (display, xevent.requestor, False, NoEventMask, (XEvent*) & xevent);
}

/**
 * cdk_x11_display_text_property_to_text_list:
 * @display: (type CdkX11Display): The #CdkDisplay where the encoding is defined
 * @encoding: an atom representing the encoding. The most
 *    common values for this are STRING, or COMPOUND_TEXT.
 *    This is value used as the type for the property
 * @format: the format of the property
 * @text: The text data
 * @length: The number of items to transform
 * @list: location to store an  array of strings in
 *    the encoding of the current locale. This array should be
 *    freed using cdk_free_text_list().
 *
 * Convert a text string from the encoding as it is stored
 * in a property into an array of strings in the encoding of
 * the current locale. (The elements of the array represent the
 * nul-separated elements of the original text string.)
 *
 * Returns: the number of strings stored in list, or 0,
 *     if the conversion failed
 *
 * Since: 2.24
 */
gint
cdk_x11_display_text_property_to_text_list (CdkDisplay   *display,
                                            CdkAtom       encoding,
                                            gint          format,
                                            const guchar *text,
                                            gint          length,
                                            gchar      ***list)
{
  XTextProperty property;
  gint count = 0;
  gint res;
  gchar **local_list;
  g_return_val_if_fail (CDK_IS_DISPLAY (display), 0);

  if (list)
    *list = NULL;

  if (cdk_display_is_closed (display))
    return 0;

  property.value = (guchar *)text;
  property.encoding = cdk_x11_atom_to_xatom_for_display (display, encoding);
  property.format = format;
  property.nitems = length;
  res = XmbTextPropertyToTextList (CDK_DISPLAY_XDISPLAY (display), &property,
                                   &local_list, &count);
  if (res == XNoMemory || res == XLocaleNotSupported || res == XConverterNotFound)
    return 0;
  else
    {
      if (list)
        *list = local_list;
      else
        XFreeStringList (local_list);

      return count;
    }
}

/**
 * cdk_x11_free_text_list:
 * @list: the value stored in the @list parameter by
 *   a call to cdk_x11_display_text_property_to_text_list().
 *
 * Frees the array of strings created by
 * cdk_x11_display_text_property_to_text_list().
 *
 * Since: 2.24
 */
void
cdk_x11_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

  XFreeStringList (list);
}

static gint
make_list (const gchar  *text,
           gint          length,
           gboolean      latin1,
           gchar      ***list)
{
  GSList *strings = NULL;
  gint n_strings = 0;
  gint i;
  const gchar *p = text;
  const gchar *q;
  GSList *tmp_list;
  GError *error = NULL;

  while (p < text + length)
    {
      gchar *str;

      q = p;
      while (*q && q < text + length)
        q++;

      if (latin1)
        {
          str = g_convert (p, q - p,
                           "UTF-8", "ISO-8859-1",
                           NULL, NULL, &error);

          if (!str)
            {
              g_warning ("Error converting selection from STRING: %s",
                         error->message);
              g_error_free (error);
            }
        }
      else
        {
          str = g_strndup (p, q - p);
          if (!g_utf8_validate (str, -1, NULL))
            {
              g_warning ("Error converting selection from UTF8_STRING");
              g_free (str);
              str = NULL;
            }
        }

      if (str)
        {
          strings = g_slist_prepend (strings, str);
          n_strings++;
        }

      p = q + 1;
    }

  if (list)
    {
      *list = g_new (gchar *, n_strings + 1);
      (*list)[n_strings] = NULL;
    }

  i = n_strings;
  tmp_list = strings;
  while (tmp_list)
    {
      if (list)
        (*list)[--i] = tmp_list->data;
      else
        g_free (tmp_list->data);

      tmp_list = tmp_list->next;
    }

  g_slist_free (strings);

  return n_strings;
}

gint
_cdk_x11_display_text_property_to_utf8_list (CdkDisplay    *display,
                                             CdkAtom        encoding,
                                             gint           format,
                                             const guchar  *text,
                                             gint           length,
                                             gchar       ***list)
{
  if (encoding == CDK_TARGET_STRING)
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == cdk_atom_intern_static_string ("UTF8_STRING"))
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      gchar **local_list;
      gint local_count;
      gint i;
      const gchar *charset = NULL;
      gboolean need_conversion = !g_get_charset (&charset);
      gint count = 0;
      GError *error = NULL;

      /* Probably COMPOUND text, we fall back to Xlib routines
       */
      local_count = cdk_x11_display_text_property_to_text_list (display,
                                                                encoding,
                                                                format,
                                                                text,
                                                                length,
                                                                &local_list);
      if (list)
        *list = g_new (gchar *, local_count + 1);

      for (i=0; i<local_count; i++)
        {
          /* list contains stuff in our default encoding
           */
          if (need_conversion)
            {
              gchar *utf = g_convert (local_list[i], -1,
                                      "UTF-8", charset,
                                      NULL, NULL, &error);
              if (utf)
                {
                  if (list)
                    (*list)[count++] = utf;
                  else
                    g_free (utf);
                }
              else
                {
                  g_warning ("Error converting to UTF-8 from '%s': %s",
                             charset, error->message);
                  g_error_free (error);
                  error = NULL;
                }
            }
          else
            {
              if (list)
                {
                  if (g_utf8_validate (local_list[i], -1, NULL))
                    (*list)[count++] = g_strdup (local_list[i]);
                  else
                    g_warning ("Error converting selection");
                }
            }
        }

      if (local_count)
        cdk_x11_free_text_list (local_list);

      if (list)
        (*list)[count] = NULL;

      return count;
    }
}

/**
 * cdk_x11_display_string_to_compound_text:
 * @display: (type CdkX11Display): the #CdkDisplay where the encoding is defined
 * @str: a nul-terminated string
 * @encoding: (out) (transfer none): location to store the encoding atom
 *     (to be used as the type for the property)
 * @format: (out): location to store the format of the property
 * @ctext: (out) (array length=length): location to store newly
 *     allocated data for the property
 * @length: the length of @ctext, in bytes
 *
 * Convert a string from the encoding of the current
 * locale into a form suitable for storing in a window property.
 *
 * Returns: 0 upon success, non-zero upon failure
 *
 * Since: 2.24
 */
gint
cdk_x11_display_string_to_compound_text (CdkDisplay  *display,
                                         const gchar *str,
                                         CdkAtom     *encoding,
                                         gint        *format,
                                         guchar     **ctext,
                                         gint        *length)
{
  gint res;
  XTextProperty property;

  g_return_val_if_fail (CDK_IS_DISPLAY (display), 0);

  if (cdk_display_is_closed (display))
    res = XLocaleNotSupported;
  else
    res = XmbTextListToTextProperty (CDK_DISPLAY_XDISPLAY (display),
                                     (char **)&str, 1, XCompoundTextStyle,
                                     &property);
  if (res != Success)
    {
      property.encoding = None;
      property.format = None;
      property.value = NULL;
      property.nitems = 0;
    }

  if (encoding)
    *encoding = cdk_x11_xatom_to_atom_for_display (display, property.encoding);
  if (format)
    *format = property.format;
  if (ctext)
    *ctext = property.value;
  if (length)
    *length = property.nitems;

  return res;
}

/* The specifications for COMPOUND_TEXT and STRING specify that C0 and
 * C1 are not allowed except for \n and \t, however the X conversions
 * routines for COMPOUND_TEXT only enforce this in one direction,
 * causing cut-and-paste of \r and \r\n separated text to fail.
 * This routine strips out all non-allowed C0 and C1 characters
 * from the input string and also canonicalizes \r, and \r\n to \n
 */
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
_cdk_x11_display_utf8_to_string_target (CdkDisplay  *display,
                                        const gchar *str)
{
  return sanitize_utf8 (str, TRUE);
}

/**
 * cdk_x11_display_utf8_to_compound_text:
 * @display: (type CdkX11Display): a #CdkDisplay
 * @str: a UTF-8 string
 * @encoding: (out): location to store resulting encoding
 * @format: (out): location to store format of the result
 * @ctext: (out) (array length=length): location to store the data of the result
 * @length: location to store the length of the data
 *     stored in @ctext
 *
 * Converts from UTF-8 to compound text.
 *
 * Returns: %TRUE if the conversion succeeded,
 *     otherwise %FALSE
 *
 * Since: 2.24
 */
gboolean
cdk_x11_display_utf8_to_compound_text (CdkDisplay  *display,
                                       const gchar *str,
                                       CdkAtom     *encoding,
                                       gint        *format,
                                       guchar     **ctext,
                                       gint        *length)
{
  gboolean need_conversion;
  const gchar *charset;
  gchar *locale_str, *tmp_str;
  GError *error = NULL;
  gboolean result;

  g_return_val_if_fail (str != NULL, FALSE);
  g_return_val_if_fail (CDK_IS_DISPLAY (display), FALSE);

  need_conversion = !g_get_charset (&charset);

  tmp_str = sanitize_utf8 (str, FALSE);

  if (need_conversion)
    {
      locale_str = g_convert (tmp_str, -1,
                              charset, "UTF-8",
                              NULL, NULL, &error);
      g_free (tmp_str);

      if (!locale_str)
        {
          if (!g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE))
            {
              g_warning ("Error converting from UTF-8 to '%s': %s",
                         charset, error->message);
            }
          g_error_free (error);

          if (encoding)
            *encoding = None;
          if (format)
            *format = None;
          if (ctext)
            *ctext = NULL;
          if (length)
            *length = 0;

          return FALSE;
        }
    }
  else
    locale_str = tmp_str;

  result = cdk_x11_display_string_to_compound_text (display, locale_str,
                                                    encoding, format,
                                                    ctext, length);
  result = (result == Success? TRUE : FALSE);

  g_free (locale_str);

  return result;
}

/**
 * cdk_x11_free_compound_text:
 * @ctext: The pointer stored in @ctext from a call to
 *   cdk_x11_display_string_to_compound_text().
 *
 * Frees the data returned from cdk_x11_display_string_to_compound_text().
 *
 * Since: 2.24
 */
void
cdk_x11_free_compound_text (guchar *ctext)
{
  if (ctext)
    XFree (ctext);
}
