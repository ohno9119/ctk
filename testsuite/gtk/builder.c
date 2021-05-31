/* buildertest.c
 * Copyright (C) 2006-2007 Async Open Source
 * Authors: Johan Dahlin
 *          Henrique Romano
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

#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

/* exported for GtkBuilder */
G_MODULE_EXPORT void signal_normal (GtkWindow *window, GParamSpec *spec);
G_MODULE_EXPORT void signal_after (GtkWindow *window, GParamSpec *spec);
G_MODULE_EXPORT void signal_object (GtkButton *button, GParamSpec *spec);
G_MODULE_EXPORT void signal_object_after (GtkButton *button, GParamSpec *spec);
G_MODULE_EXPORT void signal_first (GtkButton *button, GParamSpec *spec);
G_MODULE_EXPORT void signal_second (GtkButton *button, GParamSpec *spec);
G_MODULE_EXPORT void signal_extra (GtkButton *button, GParamSpec *spec);
G_MODULE_EXPORT void signal_extra2 (GtkButton *button, GParamSpec *spec);

/* Copied from gtkiconfactory.c; keep in sync! */
struct _GtkIconSet
{
  guint ref_count;
  GSList *sources;
  GSList *cache;
  guint cache_size;
  guint cache_serial;
};


static GtkBuilder *
builder_new_from_string (const gchar *buffer,
                         gsize length,
                         const gchar *domain)
{
  GtkBuilder *builder;
  GError *error = NULL;

  builder = ctk_builder_new ();
  if (domain)
    ctk_builder_set_translation_domain (builder, domain);
  ctk_builder_add_from_string (builder, buffer, length, &error);
  if (error)
    {
      g_print ("ERROR: %s", error->message);
      g_error_free (error);
    }

  return builder;
}

static void
test_parser (void)
{
  GtkBuilder *builder;
  GError *error;
  
  builder = ctk_builder_new ();

  error = NULL;
  ctk_builder_add_from_string (builder, "<xxx/>", -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_UNHANDLED_TAG);
  g_error_free (error);
  
  error = NULL;
  ctk_builder_add_from_string (builder, "<interface invalid=\"X\"/>", -1, &error);
  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE);
  g_error_free (error);

  error = NULL;
  ctk_builder_add_from_string (builder, "<interface><child/></interface>", -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_TAG);
  g_error_free (error);

  error = NULL;
  ctk_builder_add_from_string (builder, "<interface><object class=\"GtkVBox\" id=\"a\"><object class=\"GtkHBox\" id=\"b\"/></object></interface>", -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_TAG);
  g_error_free (error);

  error = NULL;
  ctk_builder_add_from_string (builder, "<interface><object class=\"Unknown\" id=\"a\"></object></interface>", -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);

  error = NULL;
  ctk_builder_add_from_string (builder, "<interface><object class=\"GtkWidget\" id=\"a\" constructor=\"none\"></object></interface>", -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);

  error = NULL;
  ctk_builder_add_from_string (builder, "<interface><object class=\"GtkButton\" id=\"a\"><child internal-child=\"foobar\"><object class=\"GtkButton\" id=\"int\"/></child></object></interface>", -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);

  error = NULL;
  ctk_builder_add_from_string (builder, "<interface><object class=\"GtkButton\" id=\"a\"></object><object class=\"GtkButton\" id=\"a\"/></object></interface>", -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_DUPLICATE_ID);
  g_error_free (error);

  error = NULL;
  ctk_builder_add_from_string (builder, "<interface><object class=\"GtkButton\" id=\"a\"><property name=\"deafbeef\"></property></object></interface>", -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_PROPERTY);
  g_error_free (error);
  
  error = NULL;
  ctk_builder_add_from_string (builder, "<interface><object class=\"GtkButton\" id=\"a\"><signal name=\"deafbeef\" handler=\"ctk_true\"/></object></interface>", -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_SIGNAL);
  g_error_free (error);

  g_object_unref (builder);
}

static int normal = 0;
static int after = 0;
static int object = 0;
static int object_after = 0;

G_MODULE_EXPORT void /* exported for GtkBuilder */
signal_normal (GtkWindow *window, GParamSpec *spec)
{
  g_assert (CTK_IS_WINDOW (window));
  g_assert (normal == 0);
  g_assert (after == 0);

  normal++;
}

G_MODULE_EXPORT void /* exported for GtkBuilder */
signal_after (GtkWindow *window, GParamSpec *spec)
{
  g_assert (CTK_IS_WINDOW (window));
  g_assert (normal == 1);
  g_assert (after == 0);
  
  after++;
}

G_MODULE_EXPORT void /* exported for GtkBuilder */
signal_object (GtkButton *button, GParamSpec *spec)
{
  g_assert (CTK_IS_BUTTON (button));
  g_assert (object == 0);
  g_assert (object_after == 0);

  object++;
}

G_MODULE_EXPORT void /* exported for GtkBuilder */
signal_object_after (GtkButton *button, GParamSpec *spec)
{
  g_assert (CTK_IS_BUTTON (button));
  g_assert (object == 1);
  g_assert (object_after == 0);

  object_after++;
}

G_MODULE_EXPORT void /* exported for GtkBuilder */
signal_first (GtkButton *button, GParamSpec *spec)
{
  g_assert (normal == 0);
  normal = 10;
}

G_MODULE_EXPORT void /* exported for GtkBuilder */
signal_second (GtkButton *button, GParamSpec *spec)
{
  g_assert (normal == 10);
  normal = 20;
}

G_MODULE_EXPORT void /* exported for GtkBuilder */
signal_extra (GtkButton *button, GParamSpec *spec)
{
  g_assert (normal == 20);
  normal = 30;
}

G_MODULE_EXPORT void /* exported for GtkBuilder */
signal_extra2 (GtkButton *button, GParamSpec *spec)
{
  g_assert (normal == 30);
  normal = 40;
}

static void
test_connect_signals (void)
{
  GtkBuilder *builder;
  GObject *window;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkButton\" id=\"button\"/>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <signal name=\"notify::title\" handler=\"signal_normal\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_after\" after=\"yes\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_object\""
    "            object=\"button\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_object_after\""
    "            object=\"button\" after=\"yes\"/>"
    "  </object>"
    "</interface>";
  const gchar buffer_order[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <signal name=\"notify::title\" handler=\"signal_first\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_second\"/>"
    "  </object>"
    "</interface>";
  const gchar buffer_extra[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window2\">"
    "    <signal name=\"notify::title\" handler=\"signal_extra\"/>"
    "  </object>"
    "</interface>";
  const gchar buffer_extra2[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window3\">"
    "    <signal name=\"notify::title\" handler=\"signal_extra2\"/>"
    "  </object>"
    "</interface>";
  const gchar buffer_after_child[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\"/>"
    "    </child>"
    "    <signal name=\"notify::title\" handler=\"signal_normal\"/>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);
  ctk_builder_connect_signals (builder, NULL);

  window = ctk_builder_get_object (builder, "window1");
  ctk_window_set_title (CTK_WINDOW (window), "test");

  g_assert_cmpint (normal, ==, 1);
  g_assert_cmpint (after, ==, 1);
  g_assert_cmpint (object, ==, 1);
  g_assert_cmpint (object_after, ==, 1);

  ctk_widget_destroy (CTK_WIDGET (window));
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer_order, -1, NULL);
  ctk_builder_connect_signals (builder, NULL);
  window = ctk_builder_get_object (builder, "window1");
  normal = 0;
  ctk_window_set_title (CTK_WINDOW (window), "test");
  g_assert (normal == 20);

  ctk_widget_destroy (CTK_WIDGET (window));

  ctk_builder_add_from_string (builder, buffer_extra,
			       strlen (buffer_extra), NULL);
  ctk_builder_add_from_string (builder, buffer_extra2,
			       strlen (buffer_extra2), NULL);
  ctk_builder_connect_signals (builder, NULL);
  window = ctk_builder_get_object (builder, "window2");
  ctk_window_set_title (CTK_WINDOW (window), "test");
  g_assert (normal == 30);

  ctk_widget_destroy (CTK_WIDGET (window));
  window = ctk_builder_get_object (builder, "window3");
  ctk_window_set_title (CTK_WINDOW (window), "test");
  g_assert (normal == 40);
  ctk_widget_destroy (CTK_WIDGET (window));
  
  g_object_unref (builder);

  /* new test, reset globals */
  after = 0;
  normal = 0;
  
  builder = builder_new_from_string (buffer_after_child, -1, NULL);
  window = ctk_builder_get_object (builder, "window1");
  ctk_builder_connect_signals (builder, NULL);
  ctk_window_set_title (CTK_WINDOW (window), "test");

  g_assert (normal == 1);
  ctk_widget_destroy (CTK_WIDGET (window));
  g_object_unref (builder);
}

static void
test_uimanager_simple (void)
{
  GtkBuilder *builder;
  GObject *window, *uimgr, *menubar;
  GObject *menu, *label;
  GList *children;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkUIManager\" id=\"uimgr1\"/>"
    "</interface>";
    
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkUIManager\" id=\"uimgr1\">"
    "    <child>"
    "      <object class=\"GtkActionGroup\" id=\"ag1\">"
    "        <child>"
    "          <object class=\"GtkAction\" id=\"file\">"
    "            <property name=\"label\">_File</property>"
    "          </object>"
    "          <accelerator key=\"n\" modifiers=\"GDK_CONTROL_MASK\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "    <ui>"
    "      <menubar name=\"menubar1\">"
    "        <menu action=\"file\">"
    "        </menu>"
    "      </menubar>"
    "    </ui>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkMenuBar\" id=\"menubar1\" constructor=\"uimgr1\"/>"
    "    </child>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);

  uimgr = ctk_builder_get_object (builder, "uimgr1");
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  g_assert (CTK_IS_UI_MANAGER (uimgr));
  G_GNUC_END_IGNORE_DEPRECATIONS;
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer2, -1, NULL);

  menubar = ctk_builder_get_object (builder, "menubar1");
  g_assert (CTK_IS_MENU_BAR (menubar));

  children = ctk_container_get_children (CTK_CONTAINER (menubar));
  menu = children->data;
  g_assert (CTK_IS_MENU_ITEM (menu));
  g_assert (strcmp (ctk_widget_get_name (CTK_WIDGET (menu)), "file") == 0);
  g_list_free (children);
  
  label = G_OBJECT (ctk_bin_get_child (CTK_BIN (menu)));
  g_assert (CTK_IS_LABEL (label));
  g_assert (strcmp (ctk_label_get_text (CTK_LABEL (label)), "File") == 0);

  window = ctk_builder_get_object (builder, "window1");
  ctk_widget_destroy (CTK_WIDGET (window));
  g_object_unref (builder);
}

static void
test_domain (void)
{
  GtkBuilder *builder;
  const gchar buffer1[] = "<interface/>";
  const gchar buffer2[] = "<interface domain=\"domain\"/>";
  const gchar *domain;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  domain = ctk_builder_get_translation_domain (builder);
  g_assert (domain == NULL);
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer1, -1, "domain-1");
  domain = ctk_builder_get_translation_domain (builder);
  g_assert (domain);
  g_assert (strcmp (domain, "domain-1") == 0);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  domain = ctk_builder_get_translation_domain (builder);
  g_assert (domain == NULL);
  g_object_unref (builder);
}

#if 0
static void
test_translation (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label\">"
    "        <property name=\"label\" translatable=\"yes\">File</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GtkLabel *window, *label;

  setlocale (LC_ALL, "sv_SE");
  textdomain ("builder");
  bindtextdomain ("builder", "tests");

  builder = builder_new_from_string (buffer, -1, NULL);
  label = CTK_LABEL (ctk_builder_get_object (builder, "label"));
  g_assert (strcmp (ctk_label_get_text (label), "Arkiv") == 0);

  window = ctk_builder_get_object (builder, "window1");
  ctk_widget_destroy (CTK_WIDGET (window));
  g_object_unref (builder);
}
#endif

static void
test_sizegroup (void)
{
  GtkBuilder * builder;
  const gchar buffer1[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup1\">"
    "    <property name=\"mode\">CTK_SIZE_GROUP_HORIZONTAL</property>"
    "    <widgets>"
    "      <widget name=\"radio1\"/>"
    "      <widget name=\"radio2\"/>"
    "    </widgets>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"vbox1\">"
    "        <child>"
    "          <object class=\"GtkRadioButton\" id=\"radio1\"/>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkRadioButton\" id=\"radio2\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup1\">"
    "    <property name=\"mode\">CTK_SIZE_GROUP_HORIZONTAL</property>"
    "    <widgets>"
    "    </widgets>"
    "   </object>"
    "</interface>";
  const gchar buffer3[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup1\">"
    "    <property name=\"mode\">CTK_SIZE_GROUP_HORIZONTAL</property>"
    "    <widgets>"
    "      <widget name=\"radio1\"/>"
    "      <widget name=\"radio2\"/>"
    "    </widgets>"
    "  </object>"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup2\">"
    "    <property name=\"mode\">CTK_SIZE_GROUP_HORIZONTAL</property>"
    "    <widgets>"
    "      <widget name=\"radio1\"/>"
    "      <widget name=\"radio2\"/>"
    "    </widgets>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"vbox1\">"
    "        <child>"
    "          <object class=\"GtkRadioButton\" id=\"radio1\"/>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkRadioButton\" id=\"radio2\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *sizegroup;
  GSList *widgets;

  builder = builder_new_from_string (buffer1, -1, NULL);
  sizegroup = ctk_builder_get_object (builder, "sizegroup1");
  widgets = ctk_size_group_get_widgets (CTK_SIZE_GROUP (sizegroup));
  g_assert (g_slist_length (widgets) == 2);
  g_slist_free (widgets);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  sizegroup = ctk_builder_get_object (builder, "sizegroup1");
  widgets = ctk_size_group_get_widgets (CTK_SIZE_GROUP (sizegroup));
  g_assert (g_slist_length (widgets) == 0);
  g_slist_free (widgets);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer3, -1, NULL);
  sizegroup = ctk_builder_get_object (builder, "sizegroup1");
  widgets = ctk_size_group_get_widgets (CTK_SIZE_GROUP (sizegroup));
  g_assert (g_slist_length (widgets) == 2);
  g_slist_free (widgets);
  sizegroup = ctk_builder_get_object (builder, "sizegroup2");
  widgets = ctk_size_group_get_widgets (CTK_SIZE_GROUP (sizegroup));
  g_assert (g_slist_length (widgets) == 2);
  g_slist_free (widgets);

#if 0
  {
    GObject *window;
    window = ctk_builder_get_object (builder, "window1");
    ctk_widget_destroy (CTK_WIDGET (window));
  }
#endif  
  g_object_unref (builder);
}

static void
test_list_store (void)
{
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"guint\"/>"
    "    </columns>"
    "  </object>"
    "</interface>";
  const char buffer2[] = 
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gint\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\" translatable=\"yes\">John</col>"
    "        <col id=\"1\" context=\"foo\">Doe</col>"
    "        <col id=\"2\" comments=\"foobar\">25</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\">Johan</col>"
    "        <col id=\"1\">Dole</col>"
    "        <col id=\"2\">50</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "</interface>";
  const char buffer3[] = 
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gint\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"1\" context=\"foo\">Doe</col>"
    "        <col id=\"0\" translatable=\"yes\">John</col>"
    "        <col id=\"2\" comments=\"foobar\">25</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"2\">50</col>"
    "        <col id=\"1\">Dole</col>"
    "        <col id=\"0\">Johan</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"2\">19</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "</interface>";
  GtkBuilder *builder;
  GObject *store;
  GtkTreeIter iter;
  gchar *surname, *lastname;
  int age;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  store = ctk_builder_get_object (builder, "liststore1");
  g_assert (ctk_tree_model_get_n_columns (CTK_TREE_MODEL (store)) == 2);
  g_assert (ctk_tree_model_get_column_type (CTK_TREE_MODEL (store), 0) == G_TYPE_STRING);
  g_assert (ctk_tree_model_get_column_type (CTK_TREE_MODEL (store), 1) == G_TYPE_UINT);
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer2, -1, NULL);
  store = ctk_builder_get_object (builder, "liststore1");
  g_assert (ctk_tree_model_get_n_columns (CTK_TREE_MODEL (store)) == 3);
  g_assert (ctk_tree_model_get_column_type (CTK_TREE_MODEL (store), 0) == G_TYPE_STRING);
  g_assert (ctk_tree_model_get_column_type (CTK_TREE_MODEL (store), 1) == G_TYPE_STRING);
  g_assert (ctk_tree_model_get_column_type (CTK_TREE_MODEL (store), 2) == G_TYPE_INT);
  
  g_assert (ctk_tree_model_get_iter_first (CTK_TREE_MODEL (store), &iter) == TRUE);
  ctk_tree_model_get (CTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert (surname != NULL);
  g_assert (strcmp (surname, "John") == 0);
  g_free (surname);
  g_assert (lastname != NULL);
  g_assert (strcmp (lastname, "Doe") == 0);
  g_free (lastname);
  g_assert (age == 25);
  g_assert (ctk_tree_model_iter_next (CTK_TREE_MODEL (store), &iter) == TRUE);
  
  ctk_tree_model_get (CTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert (surname != NULL);
  g_assert (strcmp (surname, "Johan") == 0);
  g_free (surname);
  g_assert (lastname != NULL);
  g_assert (strcmp (lastname, "Dole") == 0);
  g_free (lastname);
  g_assert (age == 50);
  g_assert (ctk_tree_model_iter_next (CTK_TREE_MODEL (store), &iter) == FALSE);

  g_object_unref (builder);  

  builder = builder_new_from_string (buffer3, -1, NULL);
  store = ctk_builder_get_object (builder, "liststore1");
  g_assert (ctk_tree_model_get_n_columns (CTK_TREE_MODEL (store)) == 3);
  g_assert (ctk_tree_model_get_column_type (CTK_TREE_MODEL (store), 0) == G_TYPE_STRING);
  g_assert (ctk_tree_model_get_column_type (CTK_TREE_MODEL (store), 1) == G_TYPE_STRING);
  g_assert (ctk_tree_model_get_column_type (CTK_TREE_MODEL (store), 2) == G_TYPE_INT);
  
  g_assert (ctk_tree_model_get_iter_first (CTK_TREE_MODEL (store), &iter) == TRUE);
  ctk_tree_model_get (CTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert (surname != NULL);
  g_assert (strcmp (surname, "John") == 0);
  g_free (surname);
  g_assert (lastname != NULL);
  g_assert (strcmp (lastname, "Doe") == 0);
  g_free (lastname);
  g_assert (age == 25);
  g_assert (ctk_tree_model_iter_next (CTK_TREE_MODEL (store), &iter) == TRUE);
  
  ctk_tree_model_get (CTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert (surname != NULL);
  g_assert (strcmp (surname, "Johan") == 0);
  g_free (surname);
  g_assert (lastname != NULL);
  g_assert (strcmp (lastname, "Dole") == 0);
  g_free (lastname);
  g_assert (age == 50);
  g_assert (ctk_tree_model_iter_next (CTK_TREE_MODEL (store), &iter) == TRUE);
  
  ctk_tree_model_get (CTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert (surname == NULL);
  g_assert (lastname == NULL);
  g_assert (age == 19);
  g_assert (ctk_tree_model_iter_next (CTK_TREE_MODEL (store), &iter) == FALSE);

  g_object_unref (builder);
}

static void
test_tree_store (void)
{
  const gchar buffer[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkTreeStore\" id=\"treestore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"guint\"/>"
    "    </columns>"
    "  </object>"
    "</interface>";
  GtkBuilder *builder;
  GObject *store;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  store = ctk_builder_get_object (builder, "treestore1");
  g_assert (ctk_tree_model_get_n_columns (CTK_TREE_MODEL (store)) == 2);
  g_assert (ctk_tree_model_get_column_type (CTK_TREE_MODEL (store), 0) == G_TYPE_STRING);
  g_assert (ctk_tree_model_get_column_type (CTK_TREE_MODEL (store), 1) == G_TYPE_UINT);
  
  g_object_unref (builder);
}

static void
test_types (void)
{
  const gchar buffer[] = 
    "<interface>"
    "  <object class=\"GtkAction\" id=\"action\"/>"
    "  <object class=\"GtkActionGroup\" id=\"actiongroup\"/>"
    "  <object class=\"GtkAlignment\" id=\"alignment\"/>"
    "  <object class=\"GtkArrow\" id=\"arrow\"/>"
    "  <object class=\"GtkButton\" id=\"button\"/>"
    "  <object class=\"GtkCheckButton\" id=\"checkbutton\"/>"
    "  <object class=\"GtkDialog\" id=\"dialog\"/>"
    "  <object class=\"GtkDrawingArea\" id=\"drawingarea\"/>"
    "  <object class=\"GtkEventBox\" id=\"eventbox\"/>"
    "  <object class=\"GtkEntry\" id=\"entry\"/>"
    "  <object class=\"GtkFontButton\" id=\"fontbutton\"/>"
    "  <object class=\"GtkHButtonBox\" id=\"hbuttonbox\"/>"
    "  <object class=\"GtkHBox\" id=\"hbox\"/>"
    "  <object class=\"GtkHPaned\" id=\"hpaned\"/>"
    "  <object class=\"GtkHScale\" id=\"hscale\"/>"
    "  <object class=\"GtkHScrollbar\" id=\"hscrollbar\"/>"
    "  <object class=\"GtkHSeparator\" id=\"hseparator\"/>"
    "  <object class=\"GtkImage\" id=\"image\"/>"
    "  <object class=\"GtkLabel\" id=\"label\"/>"
    "  <object class=\"GtkListStore\" id=\"liststore\"/>"
    "  <object class=\"GtkMenuBar\" id=\"menubar\"/>"
    "  <object class=\"GtkNotebook\" id=\"notebook\"/>"
    "  <object class=\"GtkProgressBar\" id=\"progressbar\"/>"
    "  <object class=\"GtkRadioButton\" id=\"radiobutton\"/>"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup\"/>"
    "  <object class=\"GtkScrolledWindow\" id=\"scrolledwindow\"/>"
    "  <object class=\"GtkSpinButton\" id=\"spinbutton\"/>"
    "  <object class=\"GtkStatusbar\" id=\"statusbar\"/>"
    "  <object class=\"GtkTextView\" id=\"textview\"/>"
    "  <object class=\"GtkToggleAction\" id=\"toggleaction\"/>"
    "  <object class=\"GtkToggleButton\" id=\"togglebutton\"/>"
    "  <object class=\"GtkToolbar\" id=\"toolbar\"/>"
    "  <object class=\"GtkTreeStore\" id=\"treestore\"/>"
    "  <object class=\"GtkTreeView\" id=\"treeview\"/>"
    "  <object class=\"GtkTable\" id=\"table\"/>"
    "  <object class=\"GtkVBox\" id=\"vbox\"/>"
    "  <object class=\"GtkVButtonBox\" id=\"vbuttonbox\"/>"
    "  <object class=\"GtkVScrollbar\" id=\"vscrollbar\"/>"
    "  <object class=\"GtkVSeparator\" id=\"vseparator\"/>"
    "  <object class=\"GtkViewport\" id=\"viewport\"/>"
    "  <object class=\"GtkVPaned\" id=\"vpaned\"/>"
    "  <object class=\"GtkVScale\" id=\"vscale\"/>"
    "  <object class=\"GtkWindow\" id=\"window\"/>"
    "  <object class=\"GtkUIManager\" id=\"uimanager\"/>"
    "</interface>";
  const gchar buffer2[] = 
    "<interface>"
    "  <object type-func=\"ctk_window_get_type\" id=\"window\"/>"
    "</interface>";
  const gchar buffer3[] = 
    "<interface>"
    "  <object class=\"XXXInvalidType\" type-func=\"ctk_window_get_type\" id=\"window\"/>"
    "</interface>";
  const gchar buffer4[] =
    "<interface>"
    "  <object type-func=\"xxx_invalid_get_type_function\" id=\"window\"/>"
    "</interface>";
  GtkBuilder *builder;
  GObject *window;
  GError *error;

  builder = builder_new_from_string (buffer, -1, NULL);
  ctk_widget_destroy (CTK_WIDGET (ctk_builder_get_object (builder, "dialog")));
  ctk_widget_destroy (CTK_WIDGET (ctk_builder_get_object (builder, "window")));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  window = ctk_builder_get_object (builder, "window");
  g_assert (CTK_IS_WINDOW (window));
  ctk_widget_destroy (CTK_WIDGET (window));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer3, -1, NULL);
  window = ctk_builder_get_object (builder, "window");
  g_assert (CTK_IS_WINDOW (window));
  ctk_widget_destroy (CTK_WIDGET (window));
  g_object_unref (builder);
  
  error = NULL;
  builder = ctk_builder_new ();
  ctk_builder_add_from_string (builder, buffer4, -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_TYPE_FUNCTION);
  g_error_free (error);
  g_object_unref (builder);
}

static void
test_spin_button (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "<object class=\"GtkAdjustment\" id=\"adjustment1\">"
    "<property name=\"lower\">0</property>"
    "<property name=\"upper\">10</property>"
    "<property name=\"step-increment\">2</property>"
    "<property name=\"page-increment\">3</property>"
    "<property name=\"page-size\">0</property>"
    "<property name=\"value\">1</property>"
    "</object>"
    "<object class=\"GtkSpinButton\" id=\"spinbutton1\">"
    "<property name=\"visible\">True</property>"
    "<property name=\"adjustment\">adjustment1</property>"
    "</object>"
    "</interface>";
  GObject *obj;
  GtkAdjustment *adjustment;
  gdouble value;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  obj = ctk_builder_get_object (builder, "spinbutton1");
  g_assert (CTK_IS_SPIN_BUTTON (obj));
  adjustment = ctk_spin_button_get_adjustment (CTK_SPIN_BUTTON (obj));
  g_assert (CTK_IS_ADJUSTMENT (adjustment));
  g_object_get (adjustment, "value", &value, NULL);
  g_assert (value == 1);
  g_object_get (adjustment, "lower", &value, NULL);
  g_assert (value == 0);
  g_object_get (adjustment, "upper", &value, NULL);
  g_assert (value == 10);
  g_object_get (adjustment, "step-increment", &value, NULL);
  g_assert (value == 2);
  g_object_get (adjustment, "page-increment", &value, NULL);
  g_assert (value == 3);
  g_object_get (adjustment, "page-size", &value, NULL);
  g_assert (value == 0);
  
  g_object_unref (builder);
}

static void
test_notebook (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkNotebook\" id=\"notebook1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label1\">"
    "        <property name=\"label\">label1</property>"
    "      </object>"
    "    </child>"
    "    <child type=\"tab\">"
    "      <object class=\"GtkLabel\" id=\"tablabel1\">"
    "        <property name=\"label\">tab_label1</property>"
    "      </object>"
    "    </child>"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label2\">"
    "        <property name=\"label\">label2</property>"
    "      </object>"
    "    </child>"
    "    <child type=\"tab\">"
    "      <object class=\"GtkLabel\" id=\"tablabel2\">"
    "        <property name=\"label\">tab_label2</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *notebook;
  GtkWidget *label;

  builder = builder_new_from_string (buffer, -1, NULL);
  notebook = ctk_builder_get_object (builder, "notebook1");
  g_assert (notebook != NULL);
  g_assert (ctk_notebook_get_n_pages (CTK_NOTEBOOK (notebook)) == 2);

  label = ctk_notebook_get_nth_page (CTK_NOTEBOOK (notebook), 0);
  g_assert (CTK_IS_LABEL (label));
  g_assert (strcmp (ctk_label_get_label (CTK_LABEL (label)), "label1") == 0);
  label = ctk_notebook_get_tab_label (CTK_NOTEBOOK (notebook), label);
  g_assert (CTK_IS_LABEL (label));
  g_assert (strcmp (ctk_label_get_label (CTK_LABEL (label)), "tab_label1") == 0);

  label = ctk_notebook_get_nth_page (CTK_NOTEBOOK (notebook), 1);
  g_assert (CTK_IS_LABEL (label));
  g_assert (strcmp (ctk_label_get_label (CTK_LABEL (label)), "label2") == 0);
  label = ctk_notebook_get_tab_label (CTK_NOTEBOOK (notebook), label);
  g_assert (CTK_IS_LABEL (label));
  g_assert (strcmp (ctk_label_get_label (CTK_LABEL (label)), "tab_label2") == 0);

  g_object_unref (builder);
}

static void
test_construct_only_property (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <property name=\"type\">CTK_WINDOW_POPUP</property>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkTextTagTable\" id=\"tagtable1\"/>"
    "  <object class=\"GtkTextBuffer\" id=\"textbuffer1\">"
    "    <property name=\"tag-table\">tagtable1</property>"
    "  </object>"
    "</interface>";
  GObject *widget, *tagtable, *textbuffer;
  GtkWindowType type;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  widget = ctk_builder_get_object (builder, "window1");
  g_object_get (widget, "type", &type, NULL);
  g_assert (type == CTK_WINDOW_POPUP);

  ctk_widget_destroy (CTK_WIDGET (widget));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  textbuffer = ctk_builder_get_object (builder, "textbuffer1");
  g_assert (textbuffer != NULL);
  g_object_get (textbuffer, "tag-table", &tagtable, NULL);
  g_assert (tagtable == ctk_builder_get_object (builder, "tagtable1"));
  g_object_unref (tagtable);
  g_object_unref (builder);
}

static void
test_object_properties (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"vbox\">"
    "        <property name=\"border-width\">10</property>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label1\">"
    "            <property name=\"mnemonic-widget\">spinbutton1</property>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkSpinButton\" id=\"spinbutton1\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window2\"/>"
    "</interface>";
  GObject *label, *spinbutton, *window;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  label = ctk_builder_get_object (builder, "label1");
  g_assert (label != NULL);
  spinbutton = ctk_builder_get_object (builder, "spinbutton1");
  g_assert (spinbutton != NULL);
  g_assert (spinbutton == (GObject*)ctk_label_get_mnemonic_widget (CTK_LABEL (label)));

  ctk_builder_add_from_string (builder, buffer2, -1, NULL);
  window = ctk_builder_get_object (builder, "window2");
  g_assert (window != NULL);
  ctk_widget_destroy (CTK_WIDGET (window));

  g_object_unref (builder);
}

static void
test_children (void)
{
  GtkBuilder * builder;
  GtkWidget *content_area, *dialog_action_area;
  GList *children;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "        <property name=\"label\">Hello</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkDialog\" id=\"dialog1\">"
    "    <property name=\"use_header_bar\">1</property>"
    "    <child internal-child=\"vbox\">"
    "      <object class=\"GtkVBox\" id=\"dialog1-vbox\">"
    "        <property name=\"border-width\">10</property>"
    "          <child internal-child=\"action_area\">"
    "            <object class=\"GtkHButtonBox\" id=\"dialog1-action_area\">"
    "              <property name=\"border-width\">20</property>"
    "            </object>"
    "          </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  GObject *window, *button;
  GObject *dialog, *vbox, *action_area;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  window = ctk_builder_get_object (builder, "window1");
  g_assert (window != NULL);
  g_assert (CTK_IS_WINDOW (window));

  button = ctk_builder_get_object (builder, "button1");
  g_assert (button != NULL);
  g_assert (CTK_IS_BUTTON (button));
  g_assert (ctk_widget_get_parent (CTK_WIDGET(button)) != NULL);
  g_assert (strcmp (ctk_buildable_get_name (CTK_BUILDABLE (ctk_widget_get_parent (CTK_WIDGET (button)))), "window1") == 0);

  ctk_widget_destroy (CTK_WIDGET (window));
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer2, -1, NULL);
  dialog = ctk_builder_get_object (builder, "dialog1");
  g_assert (dialog != NULL);
  g_assert (CTK_IS_DIALOG (dialog));
  children = ctk_container_get_children (CTK_CONTAINER (dialog));
  g_assert_cmpint (g_list_length (children), ==, 2);
  g_list_free (children);
  
  vbox = ctk_builder_get_object (builder, "dialog1-vbox");
  content_area = ctk_dialog_get_content_area (CTK_DIALOG (dialog));
  g_assert (vbox != NULL);
  g_assert (CTK_IS_BOX (vbox));
  g_assert (ctk_orientable_get_orientation (CTK_ORIENTABLE (vbox)) == CTK_ORIENTATION_VERTICAL);
  g_assert (strcmp (ctk_buildable_get_name (CTK_BUILDABLE (ctk_widget_get_parent (CTK_WIDGET (vbox)))), "dialog1") == 0);
  g_assert (ctk_container_get_border_width (CTK_CONTAINER (vbox)) == 10);
  g_assert (strcmp (ctk_buildable_get_name (CTK_BUILDABLE (content_area)), "dialog1-vbox") == 0);

  action_area = ctk_builder_get_object (builder, "dialog1-action_area");
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  dialog_action_area = ctk_dialog_get_action_area (CTK_DIALOG (dialog));
G_GNUC_END_IGNORE_DEPRECATIONS
  g_assert (action_area != NULL);
  g_assert (CTK_IS_BUTTON_BOX (action_area));
  g_assert (ctk_orientable_get_orientation (CTK_ORIENTABLE (action_area)) == CTK_ORIENTATION_HORIZONTAL);
  g_assert (ctk_widget_get_parent (CTK_WIDGET (action_area)) != NULL);
  g_assert (ctk_container_get_border_width (CTK_CONTAINER (action_area)) == 20);
  g_assert (dialog_action_area != NULL);
  g_assert (ctk_buildable_get_name (CTK_BUILDABLE (action_area)) != NULL);
  g_assert (strcmp (ctk_buildable_get_name (CTK_BUILDABLE (dialog_action_area)), "dialog1-action_area") == 0);
  ctk_widget_destroy (CTK_WIDGET (dialog));
  g_object_unref (builder);
}

static void
test_child_properties (void)
{
  GtkBuilder * builder;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkBox\" id=\"vbox1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label1\"/>"
    "      <packing>"
    "        <property name=\"pack-type\">start</property>"
    "      </packing>"
    "    </child>"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label2\"/>"
    "      <packing>"
    "        <property name=\"pack-type\">end</property>"
    "      </packing>"
    "    </child>"
    "  </object>"
    "</interface>";

  GObject *label, *vbox;
  GtkPackType pack_type;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  vbox = ctk_builder_get_object (builder, "vbox1");
  g_assert (CTK_IS_BOX (vbox));

  label = ctk_builder_get_object (builder, "label1");
  g_assert (CTK_IS_LABEL (label));
  ctk_container_child_get (CTK_CONTAINER (vbox),
                           CTK_WIDGET (label),
                           "pack-type",
                           &pack_type,
                           NULL);
  g_assert (pack_type == CTK_PACK_START);
  
  label = ctk_builder_get_object (builder, "label2");
  g_assert (CTK_IS_LABEL (label));
  ctk_container_child_get (CTK_CONTAINER (vbox),
                           CTK_WIDGET (label),
                           "pack-type",
                           &pack_type,
                           NULL);
  g_assert (pack_type == CTK_PACK_END);

  g_object_unref (builder);
}

static void
test_treeview_column (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "<object class=\"GtkListStore\" id=\"liststore1\">"
    "  <columns>"
    "    <column type=\"gchararray\"/>"
    "    <column type=\"guint\"/>"
    "  </columns>"
    "  <data>"
    "    <row>"
    "      <col id=\"0\">John</col>"
    "      <col id=\"1\">25</col>"
    "    </row>"
    "  </data>"
    "</object>"
    "<object class=\"GtkWindow\" id=\"window1\">"
    "  <child>"
    "    <object class=\"GtkTreeView\" id=\"treeview1\">"
    "      <property name=\"visible\">True</property>"
    "      <property name=\"model\">liststore1</property>"
    "      <child>"
    "        <object class=\"GtkTreeViewColumn\" id=\"column1\">"
    "          <property name=\"title\">Test</property>"
    "          <child>"
    "            <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">1</attribute>"
    "            </attributes>"
    "          </child>"
    "        </object>"
    "      </child>"
    "      <child>"
    "        <object class=\"GtkTreeViewColumn\" id=\"column2\">"
    "          <property name=\"title\">Number</property>"
    "          <child>"
    "            <object class=\"GtkCellRendererText\" id=\"renderer2\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">0</attribute>"
    "            </attributes>"
    "          </child>"
    "        </object>"
    "      </child>"
    "    </object>"
    "  </child>"
    "</object>"
    "</interface>";
  GObject *window, *treeview;
  GtkTreeViewColumn *column;
  GList *renderers;
  GObject *renderer;

  builder = builder_new_from_string (buffer, -1, NULL);
  treeview = ctk_builder_get_object (builder, "treeview1");
  g_assert (treeview);
  g_assert (CTK_IS_TREE_VIEW (treeview));
  column = ctk_tree_view_get_column (CTK_TREE_VIEW (treeview), 0);
  g_assert (CTK_IS_TREE_VIEW_COLUMN (column));
  g_assert (strcmp (ctk_tree_view_column_get_title (column), "Test") == 0);

  renderers = ctk_cell_layout_get_cells (CTK_CELL_LAYOUT (column));
  g_assert (g_list_length (renderers) == 1);
  renderer = g_list_nth_data (renderers, 0);
  g_assert (renderer);
  g_assert (CTK_IS_CELL_RENDERER_TEXT (renderer));
  g_list_free (renderers);

  window = ctk_builder_get_object (builder, "window1");
  ctk_widget_destroy (CTK_WIDGET (window));

  g_object_unref (builder);
}

static void
test_icon_view (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"GdkPixbuf\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">test</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkIconView\" id=\"iconview1\">"
    "        <property name=\"model\">liststore1</property>"
    "        <property name=\"text-column\">0</property>"
    "        <property name=\"pixbuf-column\">1</property>"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">0</attribute>"
    "          </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *iconview;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  iconview = ctk_builder_get_object (builder, "iconview1");
  g_assert (iconview);
  g_assert (CTK_IS_ICON_VIEW (iconview));

  window = ctk_builder_get_object (builder, "window1");
  ctk_widget_destroy (CTK_WIDGET (window));
  g_object_unref (builder);
}

static void
test_combo_box (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"guint\"/>"
    "      <column type=\"gchararray\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">1</col>"
    "        <col id=\"1\">Foo</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\">2</col>"
    "        <col id=\"1\">Bar</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkComboBox\" id=\"combobox1\">"
    "        <property name=\"model\">liststore1</property>"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">0</attribute>"
    "          </attributes>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer2\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">1</attribute>"
    "          </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *combobox;

  builder = builder_new_from_string (buffer, -1, NULL);
  combobox = ctk_builder_get_object (builder, "combobox1");
  g_assert (combobox);

  window = ctk_builder_get_object (builder, "window1");
  ctk_widget_destroy (CTK_WIDGET (window));

  g_object_unref (builder);
}

#if 0
static void
test_combo_box_entry (void)
{
  GtkBuilder *builder;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"guint\"/>"
    "      <column type=\"gchararray\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">1</col>"
    "        <col id=\"1\">Foo</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\">2</col>"
    "        <col id=\"1\">Bar</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkComboBox\" id=\"comboboxentry1\">"
    "        <property name=\"model\">liststore1</property>"
    "        <property name=\"has-entry\">True</property>"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">0</attribute>"
    "            </attributes>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer2\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">1</attribute>"
    "            </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *combobox, *renderer;
  gchar *text;

  builder = builder_new_from_string (buffer, -1, NULL);
  combobox = ctk_builder_get_object (builder, "comboboxentry1");
  g_assert (combobox);

  renderer = ctk_builder_get_object (builder, "renderer2");
  g_assert (renderer);
  g_object_get (renderer, "text", &text, NULL);
  g_assert (text);
  g_assert (strcmp (text, "Bar") == 0);
  g_free (text);

  renderer = ctk_builder_get_object (builder, "renderer1");
  g_assert (renderer);
  g_object_get (renderer, "text", &text, NULL);
  g_assert (text);
  g_assert (strcmp (text, "2") == 0);
  g_free (text);

  window = ctk_builder_get_object (builder, "window1");
  ctk_widget_destroy (CTK_WIDGET (window));

  g_object_unref (builder);
}
#endif

static void
test_cell_view (void)
{
  GtkBuilder *builder;
  const gchar *buffer =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">test</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkCellView\" id=\"cellview1\">"
    "        <property name=\"visible\">True</property>"
    "        <property name=\"model\">liststore1</property>"
    "        <accelerator key=\"f\" modifiers=\"GDK_CONTROL_MASK\" signal=\"grab_focus\"/>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">0</attribute>"
    "          </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *cellview;
  GObject *model, *window;
  GtkTreePath *path;
  GList *renderers;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  cellview = ctk_builder_get_object (builder, "cellview1");
  g_assert (builder);
  g_assert (cellview);
  g_assert (CTK_IS_CELL_VIEW (cellview));
  g_object_get (cellview, "model", &model, NULL);
  g_assert (model);
  g_assert (CTK_IS_TREE_MODEL (model));
  g_object_unref (model);
  path = ctk_tree_path_new_first ();
  ctk_cell_view_set_displayed_row (CTK_CELL_VIEW (cellview), path);
  
  renderers = ctk_cell_layout_get_cells (CTK_CELL_LAYOUT (cellview));
  g_assert (renderers);
  g_assert (g_list_length (renderers) == 1);

  window = ctk_builder_get_object (builder, "window1");
  g_assert (window);
  ctk_widget_destroy (CTK_WIDGET (window));
  
  g_object_unref (builder);
}

static void
test_dialog (void)
{
  GtkBuilder * builder;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkDialog\" id=\"dialog1\">"
    "    <child internal-child=\"vbox\">"
    "      <object class=\"GtkVBox\" id=\"dialog1-vbox\">"
    "          <child internal-child=\"action_area\">"
    "            <object class=\"GtkHButtonBox\" id=\"dialog1-action_area\">"
    "              <child>"
    "                <object class=\"GtkButton\" id=\"button_cancel\"/>"
    "              </child>"
    "              <child>"
    "                <object class=\"GtkButton\" id=\"button_ok\"/>"
    "              </child>"
    "            </object>"
    "          </child>"
    "      </object>"
    "    </child>"
    "    <action-widgets>"
    "      <action-widget response=\"3\">button_ok</action-widget>"
    "      <action-widget response=\"-5\">button_cancel</action-widget>"
    "    </action-widgets>"
    "  </object>"
    "</interface>";

  GObject *dialog1;
  GObject *button_ok;
  GObject *button_cancel;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  dialog1 = ctk_builder_get_object (builder, "dialog1");
  button_ok = ctk_builder_get_object (builder, "button_ok");
  g_assert (ctk_dialog_get_response_for_widget (CTK_DIALOG (dialog1), CTK_WIDGET (button_ok)) == 3);
  button_cancel = ctk_builder_get_object (builder, "button_cancel");
  g_assert (ctk_dialog_get_response_for_widget (CTK_DIALOG (dialog1), CTK_WIDGET (button_cancel)) == -5);
  
  ctk_widget_destroy (CTK_WIDGET (dialog1));
  g_object_unref (builder);
}

static void
test_message_dialog (void)
{
  GtkBuilder * builder;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkMessageDialog\" id=\"dialog1\">"
    "    <child internal-child=\"message_area\">"
    "      <object class=\"GtkVBox\" id=\"dialog-message-area\">"
    "        <child>"
    "          <object class=\"GtkExpander\" id=\"expander\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  GObject *dialog1;
  GObject *expander;

  builder = builder_new_from_string (buffer1, -1, NULL);
  dialog1 = ctk_builder_get_object (builder, "dialog1");
  expander = ctk_builder_get_object (builder, "expander");
  g_assert (CTK_IS_EXPANDER (expander));
  g_assert (ctk_widget_get_parent (CTK_WIDGET (expander)) == ctk_message_dialog_get_message_area (CTK_MESSAGE_DIALOG (dialog1)));

  ctk_widget_destroy (CTK_WIDGET (dialog1));
  g_object_unref (builder);
}

static void
test_accelerators (void)
{
  GtkBuilder *builder;
  const gchar *buffer =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "        <accelerator key=\"q\" modifiers=\"GDK_CONTROL_MASK\" signal=\"clicked\"/>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const gchar *buffer2 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkTreeView\" id=\"treeview1\">"
    "        <signal name=\"cursor-changed\" handler=\"ctk_main_quit\"/>"
    "        <accelerator key=\"f\" modifiers=\"GDK_CONTROL_MASK\" signal=\"grab_focus\"/>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window1;
  GSList *accel_groups;
  GObject *accel_group;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  window1 = ctk_builder_get_object (builder, "window1");
  g_assert (window1);
  g_assert (CTK_IS_WINDOW (window1));

  accel_groups = ctk_accel_groups_from_object (window1);
  g_assert (g_slist_length (accel_groups) == 1);
  accel_group = g_slist_nth_data (accel_groups, 0);
  g_assert (accel_group);

  ctk_widget_destroy (CTK_WIDGET (window1));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  window1 = ctk_builder_get_object (builder, "window1");
  g_assert (window1);
  g_assert (CTK_IS_WINDOW (window1));

  accel_groups = ctk_accel_groups_from_object (window1);
  g_assert (g_slist_length (accel_groups) == 1);
  accel_group = g_slist_nth_data (accel_groups, 0);
  g_assert (accel_group);

  ctk_widget_destroy (CTK_WIDGET (window1));
  g_object_unref (builder);
}

static void
test_widget (void)
{
  const gchar *buffer =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "         <property name=\"can-focus\">True</property>"
    "         <property name=\"has-focus\">True</property>"
    "      </object>"
    "    </child>"
    "  </object>"
   "</interface>";
  const gchar *buffer2 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "         <property name=\"can-default\">True</property>"
    "         <property name=\"has-default\">True</property>"
    "      </object>"
    "    </child>"
    "  </object>"
   "</interface>";
  const gchar *buffer3 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"vbox1\">"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label1\">"
    "            <child internal-child=\"accessible\">"
    "              <object class=\"AtkObject\" id=\"a11y-label1\">"
    "                <property name=\"AtkObject::accessible-name\">A Label</property>"
    "              </object>"
    "            </child>"
    "            <accessibility>"
    "              <relation target=\"button1\" type=\"label-for\"/>"
    "            </accessibility>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkButton\" id=\"button1\">"
    "            <accessibility>"
    "              <action action_name=\"click\" description=\"Sliff\"/>"
    "              <action action_name=\"clack\" translatable=\"yes\">Sniff</action>"
    "            </accessibility>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GtkBuilder *builder;
  GObject *window1, *button1, *label1;
  AtkObject *accessible;
  AtkRelationSet *relation_set;
  AtkRelation *relation;
  char *name;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  button1 = ctk_builder_get_object (builder, "button1");

#if 0
  g_assert (ctk_widget_has_focus (CTK_WIDGET (button1)));
#endif
  window1 = ctk_builder_get_object (builder, "window1");
  ctk_widget_destroy (CTK_WIDGET (window1));
  
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer2, -1, NULL);
  button1 = ctk_builder_get_object (builder, "button1");

  g_assert (ctk_widget_get_receives_default (CTK_WIDGET (button1)));
  
  g_object_unref (builder);
  
  builder = builder_new_from_string (buffer3, -1, NULL);

  window1 = ctk_builder_get_object (builder, "window1");
  label1 = ctk_builder_get_object (builder, "label1");

  accessible = ctk_widget_get_accessible (CTK_WIDGET (label1));
  relation_set = atk_object_ref_relation_set (accessible);
  g_return_if_fail (atk_relation_set_get_n_relations (relation_set) == 1);
  relation = atk_relation_set_get_relation (relation_set, 0);
  g_return_if_fail (relation != NULL);
  g_return_if_fail (ATK_IS_RELATION (relation));
  g_return_if_fail (atk_relation_get_relation_type (relation) != ATK_RELATION_LABELLED_BY);
  g_object_unref (relation_set);

  g_object_get (G_OBJECT (accessible), "accessible-name", &name, NULL);
  g_return_if_fail (strcmp (name, "A Label") == 0);
  g_free (name);
  
  ctk_widget_destroy (CTK_WIDGET (window1));
  g_object_unref (builder);
}

static void
test_window (void)
{
  const gchar *buffer1 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "     <property name=\"title\"></property>"
    "  </object>"
   "</interface>";
  const gchar *buffer2 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "  </object>"
   "</interface>";
  GtkBuilder *builder;
  GObject *window1;
  gchar *title;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  window1 = ctk_builder_get_object (builder, "window1");
  g_object_get (window1, "title", &title, NULL);
  g_assert (strcmp (title, "") == 0);
  g_free (title);
  ctk_widget_destroy (CTK_WIDGET (window1));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  window1 = ctk_builder_get_object (builder, "window1");
  ctk_widget_destroy (CTK_WIDGET (window1));
  g_object_unref (builder);
}

static void
test_value_from_string (void)
{
  GValue value = G_VALUE_INIT;
  GError *error = NULL;
  GtkBuilder *builder;

  builder = ctk_builder_new ();
  
  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_STRING, "test", &value, &error));
  g_assert (G_VALUE_HOLDS_STRING (&value));
  g_assert (strcmp (g_value_get_string (&value), "test") == 0);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "true", &value, &error));
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert (g_value_get_boolean (&value) == TRUE);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "false", &value, &error));
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert (g_value_get_boolean (&value) == FALSE);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "yes", &value, &error));
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert (g_value_get_boolean (&value) == TRUE);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "no", &value, &error));
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert (g_value_get_boolean (&value) == FALSE);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "0", &value, &error));
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert (g_value_get_boolean (&value) == FALSE);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "1", &value, &error));
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert (g_value_get_boolean (&value) == TRUE);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "tRuE", &value, &error));
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert (g_value_get_boolean (&value) == TRUE);
  g_value_unset (&value);
  
  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "blaurgh", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "yess", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;
  
  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "trueee", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;
  
  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;
  
  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_INT, "12345", &value, &error));
  g_assert (G_VALUE_HOLDS_INT (&value));
  g_assert (g_value_get_int (&value) == 12345);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_LONG, "9912345", &value, &error));
  g_assert (G_VALUE_HOLDS_LONG (&value));
  g_assert (g_value_get_long (&value) == 9912345);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_UINT, "2345", &value, &error));
  g_assert (G_VALUE_HOLDS_UINT (&value));
  g_assert (g_value_get_uint (&value) == 2345);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_INT64, "-2345", &value, &error));
  g_assert (G_VALUE_HOLDS_INT64 (&value));
  g_assert (g_value_get_int64 (&value) == -2345);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_UINT64, "2345", &value, &error));
  g_assert (G_VALUE_HOLDS_UINT64 (&value));
  g_assert (g_value_get_uint64 (&value) == 2345);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_FLOAT, "1.454", &value, &error));
  g_assert (G_VALUE_HOLDS_FLOAT (&value));
  g_assert (fabs (g_value_get_float (&value) - 1.454) < 0.00001);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_FLOAT, "abc", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert (ctk_builder_value_from_string_type (builder, G_TYPE_INT, "/-+,abc", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert (ctk_builder_value_from_string_type (builder, CTK_TYPE_WINDOW_TYPE, "toplevel", &value, &error) == TRUE);
  g_assert (G_VALUE_HOLDS_ENUM (&value));
  g_assert (g_value_get_enum (&value) == CTK_WINDOW_TOPLEVEL);
  g_value_unset (&value);

  g_assert (ctk_builder_value_from_string_type (builder, CTK_TYPE_WINDOW_TYPE, "sliff", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert (ctk_builder_value_from_string_type (builder, CTK_TYPE_WINDOW_TYPE, "foobar", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;
  
  g_object_unref (builder);
}

static gboolean model_freed = FALSE;

static void
model_weakref (gpointer data,
               GObject *model)
{
  model_freed = TRUE;
}

static void
test_reference_counting (void)
{
  GtkBuilder *builder;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\"/>"
    "  <object class=\"GtkListStore\" id=\"liststore2\"/>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkTreeView\" id=\"treeview1\">"
    "        <property name=\"model\">liststore1</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkVBox\" id=\"vbox1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label1\"/>"
    "      <packing>"
    "        <property name=\"pack-type\">start</property>"
    "      </packing>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *treeview, *model;
  
  builder = builder_new_from_string (buffer1, -1, NULL);
  window = ctk_builder_get_object (builder, "window1");
  treeview = ctk_builder_get_object (builder, "treeview1");
  model = ctk_builder_get_object (builder, "liststore1");
  g_object_unref (builder);

  g_object_weak_ref (model, (GWeakNotify)model_weakref, NULL);

  g_assert (model_freed == FALSE);
  ctk_tree_view_set_model (CTK_TREE_VIEW (treeview), NULL);
  g_assert (model_freed == TRUE);
  
  ctk_widget_destroy (CTK_WIDGET (window));

  builder = builder_new_from_string (buffer2, -1, NULL);
  g_object_unref (builder);
}

static void
test_icon_factory (void)
{
  GtkBuilder *builder;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkIconFactory\" id=\"iconfactory1\">"
    "    <sources>"
    "      <source stock-id=\"apple-red\" filename=\"apple-red.png\"/>"
    "    </sources>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkIconFactory\" id=\"iconfactory1\">"
    "    <sources>"
    "      <source stock-id=\"sliff\" direction=\"rtl\" state=\"active\""
    "              size=\"menu\" filename=\"sloff.png\"/>"
    "      <source stock-id=\"sliff\" direction=\"ltr\" state=\"selected\""
    "              size=\"dnd\" filename=\"slurf.png\"/>"
    "    </sources>"
    "  </object>"
    "</interface>";
  GObject *factory;
  GtkIconSet *icon_set;
  GtkIconSource *icon_source;
  GtkWidget *image;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  builder = builder_new_from_string (buffer1, -1, NULL);
  factory = ctk_builder_get_object (builder, "iconfactory1");
  g_assert (factory != NULL);

  icon_set = ctk_icon_factory_lookup (CTK_ICON_FACTORY (factory), "apple-red");
  g_assert (icon_set != NULL);
  ctk_icon_factory_add_default (CTK_ICON_FACTORY (factory));
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  image = ctk_image_new_from_stock ("apple-red", CTK_ICON_SIZE_BUTTON);
  G_GNUC_END_IGNORE_DEPRECATIONS;
  g_assert (image != NULL);
  g_object_ref_sink (image);
  g_object_unref (image);

  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  factory = ctk_builder_get_object (builder, "iconfactory1");
  g_assert (factory != NULL);

  icon_set = ctk_icon_factory_lookup (CTK_ICON_FACTORY (factory), "sliff");
  g_assert (icon_set != NULL);
  g_assert (g_slist_length (icon_set->sources) == 2);

  icon_source = icon_set->sources->data;
  g_assert (ctk_icon_source_get_direction (icon_source) == CTK_TEXT_DIR_RTL);
  g_assert (ctk_icon_source_get_state (icon_source) == CTK_STATE_ACTIVE);
  g_assert (ctk_icon_source_get_size (icon_source) == CTK_ICON_SIZE_MENU);
  g_assert (g_str_has_suffix (ctk_icon_source_get_filename (icon_source), "sloff.png"));
  
  icon_source = icon_set->sources->next->data;
  g_assert (ctk_icon_source_get_direction (icon_source) == CTK_TEXT_DIR_LTR);
  g_assert (ctk_icon_source_get_state (icon_source) == CTK_STATE_SELECTED);
  g_assert (ctk_icon_source_get_size (icon_source) == CTK_ICON_SIZE_DND);
  g_assert (g_str_has_suffix (ctk_icon_source_get_filename (icon_source), "slurf.png"));

  g_object_unref (builder);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

}

typedef struct {
  gboolean weight;
  gboolean foreground;
  gboolean underline;
  gboolean size;
  gboolean font_desc;
  gboolean language;
} FoundAttrs;

static gboolean 
filter_pango_attrs (PangoAttribute *attr, 
		    gpointer        data)
{
  FoundAttrs *found = (FoundAttrs *)data;

  if (attr->klass->type == PANGO_ATTR_WEIGHT)
    found->weight = TRUE;
  else if (attr->klass->type == PANGO_ATTR_FOREGROUND)
    found->foreground = TRUE;
  else if (attr->klass->type == PANGO_ATTR_UNDERLINE)
    found->underline = TRUE;
  /* Make sure optional start/end properties are working */
  else if (attr->klass->type == PANGO_ATTR_SIZE && 
	   attr->start_index == 5 &&
	   attr->end_index   == 10)
    found->size = TRUE;
  else if (attr->klass->type == PANGO_ATTR_FONT_DESC)
    found->font_desc = TRUE;
  else if (attr->klass->type == PANGO_ATTR_LANGUAGE)
    found->language = TRUE;

  return TRUE;
}

static void
test_pango_attributes (void)
{
  GtkBuilder *builder;
  FoundAttrs found = { 0, };
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkLabel\" id=\"label1\">"
    "    <attributes>"
    "      <attribute name=\"weight\" value=\"PANGO_WEIGHT_BOLD\"/>"
    "      <attribute name=\"foreground\" value=\"DarkSlateGray\"/>"
    "      <attribute name=\"underline\" value=\"True\"/>"
    "      <attribute name=\"size\" value=\"4\" start=\"5\" end=\"10\"/>"
    "      <attribute name=\"font-desc\" value=\"Sans Italic 22\"/>"
    "      <attribute name=\"language\" value=\"pt_BR\"/>"
    "    </attributes>"
    "  </object>"
    "</interface>";
  const gchar err_buffer1[] =
    "<interface>"
    "  <object class=\"GtkLabel\" id=\"label1\">"
    "    <attributes>"
    "      <attribute name=\"weight\"/>"
    "    </attributes>"
    "  </object>"
    "</interface>";
  const gchar err_buffer2[] =
    "<interface>"
    "  <object class=\"GtkLabel\" id=\"label1\">"
    "    <attributes>"
    "      <attribute name=\"weight\" value=\"PANGO_WEIGHT_BOLD\" unrecognized=\"True\"/>"
    "    </attributes>"
    "  </object>"
    "</interface>";

  GObject *label;
  GError  *error = NULL;
  PangoAttrList *attrs, *filtered;
  
  /* Test attributes are set */
  builder = builder_new_from_string (buffer, -1, NULL);
  label = ctk_builder_get_object (builder, "label1");
  g_assert (label != NULL);

  attrs = ctk_label_get_attributes (CTK_LABEL (label));
  g_assert (attrs != NULL);

  filtered = pango_attr_list_filter (attrs, filter_pango_attrs, &found);
  g_assert (filtered);
  pango_attr_list_unref (filtered);

  g_assert (found.weight);
  g_assert (found.foreground);
  g_assert (found.underline);
  g_assert (found.size);
  g_assert (found.language);
  g_assert (found.font_desc);

  g_object_unref (builder);

  /* Test errors are set */
  builder = ctk_builder_new ();
  ctk_builder_add_from_string (builder, err_buffer1, -1, &error);
  label = ctk_builder_get_object (builder, "label1");
  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE);
  g_object_unref (builder);
  g_error_free (error);
  error = NULL;

  builder = ctk_builder_new ();
  ctk_builder_add_from_string (builder, err_buffer2, -1, &error);
  label = ctk_builder_get_object (builder, "label1");

  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE);
  g_object_unref (builder);
  g_error_free (error);
}

static void
test_requires (void)
{
  GtkBuilder *builder;
  GError     *error = NULL;
  gchar      *buffer;
  const gchar buffer_fmt[] =
    "<interface>"
    "  <requires lib=\"gtk+\" version=\"%d.%d\"/>"
    "</interface>";

  buffer = g_strdup_printf (buffer_fmt, CTK_MAJOR_VERSION, CTK_MINOR_VERSION + 1);
  builder = ctk_builder_new ();
  ctk_builder_add_from_string (builder, buffer, -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_VERSION_MISMATCH);
  g_object_unref (builder);
  g_error_free (error);
  g_free (buffer);
}

static void
test_add_objects (void)
{
  GtkBuilder *builder;
  GError *error;
  gint ret;
  GObject *obj;
  GtkUIManager *manager;
  GtkWidget *menubar;
  GObject *menu, *label;
  GList *children;
  gchar *objects[2] = {"mainbox", NULL};
  gchar *objects2[3] = {"mainbox", "window2", NULL};
  gchar *objects3[3] = {"uimgr1", "menubar1"};
  gchar *objects4[2] = {"uimgr1", NULL};
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"mainbox\">"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label1\">"
    "            <property name=\"visible\">True</property>"
    "            <property name=\"label\" translatable=\"no\">first label</property>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label2\">"
    "            <property name=\"visible\">True</property>"
    "            <property name=\"label\" translatable=\"no\">second label</property>"
    "          </object>"
    "          <packing>"
    "            <property name=\"position\">1</property>"
    "          </packing>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window2\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label3\">"
    "        <property name=\"label\" translatable=\"no\">second label</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "<interface/>";
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkUIManager\" id=\"uimgr1\">"
    "    <child>"
    "      <object class=\"GtkActionGroup\" id=\"ag1\">"
    "        <child>"
    "          <object class=\"GtkAction\" id=\"file\">"
    "            <property name=\"label\">_File</property>"
    "          </object>"
    "          <accelerator key=\"n\" modifiers=\"GDK_CONTROL_MASK\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "    <ui>"
    "      <menubar name=\"menubar1\">"
    "        <menu action=\"file\">"
    "        </menu>"
    "      </menubar>"
    "    </ui>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkMenuBar\" id=\"menubar1\" constructor=\"uimgr1\"/>"
    "    </child>"
    "  </object>"
    "</interface>";

  error = NULL;
  builder = ctk_builder_new ();
  ret = ctk_builder_add_objects_from_string (builder, buffer, -1, objects, &error);
  g_assert (ret);
  g_assert (error == NULL);
  obj = ctk_builder_get_object (builder, "window");
  g_assert (obj == NULL);
  obj = ctk_builder_get_object (builder, "window2");
  g_assert (obj == NULL);
  obj = ctk_builder_get_object (builder, "mainbox");  
  g_assert (CTK_IS_WIDGET (obj));
  g_object_unref (builder);

  error = NULL;
  builder = ctk_builder_new ();
  ret = ctk_builder_add_objects_from_string (builder, buffer, -1, objects2, &error);
  g_assert (ret);
  g_assert (error == NULL);
  obj = ctk_builder_get_object (builder, "window");
  g_assert (obj == NULL);
  obj = ctk_builder_get_object (builder, "window2");
  g_assert (CTK_IS_WINDOW (obj));
  ctk_widget_destroy (CTK_WIDGET (obj));
  obj = ctk_builder_get_object (builder, "mainbox");  
  g_assert (CTK_IS_WIDGET (obj));
  g_object_unref (builder);

  /* test cherry picking a ui manager and menubar that depends on it */
  error = NULL;
  builder = ctk_builder_new ();
  ret = ctk_builder_add_objects_from_string (builder, buffer2, -1, objects3, &error);
  g_assert (ret);
  obj = ctk_builder_get_object (builder, "uimgr1");
  g_assert (CTK_IS_UI_MANAGER (obj));
  obj = ctk_builder_get_object (builder, "file");
  g_assert (CTK_IS_ACTION (obj));
  obj = ctk_builder_get_object (builder, "menubar1");
  g_assert (CTK_IS_MENU_BAR (obj));
  menubar = CTK_WIDGET (obj);

  children = ctk_container_get_children (CTK_CONTAINER (menubar));
  menu = children->data;
  g_assert (menu != NULL);
  g_assert (CTK_IS_MENU_ITEM (menu));
  g_assert (strcmp (ctk_widget_get_name (CTK_WIDGET (menu)), "file") == 0);
  g_list_free (children);
 
  label = G_OBJECT (ctk_bin_get_child (CTK_BIN (menu)));
  g_assert (label != NULL);
  g_assert (CTK_IS_LABEL (label));
  g_assert (strcmp (ctk_label_get_text (CTK_LABEL (label)), "File") == 0);

  g_object_unref (builder);

  /* test cherry picking just the ui manager */
  error = NULL;
  builder = ctk_builder_new ();
  ret = ctk_builder_add_objects_from_string (builder, buffer2, -1, objects4, &error);
  g_assert (ret);
  obj = ctk_builder_get_object (builder, "uimgr1");
  g_assert (CTK_IS_UI_MANAGER (obj));
  manager = CTK_UI_MANAGER (obj);
  obj = ctk_builder_get_object (builder, "file");
  g_assert (CTK_IS_ACTION (obj));
  menubar = ctk_ui_manager_get_widget (manager, "/menubar1");
  g_assert (CTK_IS_MENU_BAR (menubar));

  children = ctk_container_get_children (CTK_CONTAINER (menubar));
  menu = children->data;
  g_assert (menu != NULL);
  g_assert (CTK_IS_MENU_ITEM (menu));
  g_assert (strcmp (ctk_widget_get_name (CTK_WIDGET (menu)), "file") == 0);
  g_list_free (children);
 
  label = G_OBJECT (ctk_bin_get_child (CTK_BIN (menu)));
  g_assert (label != NULL);
  g_assert (CTK_IS_LABEL (label));
  g_assert (strcmp (ctk_label_get_text (CTK_LABEL (label)), "File") == 0);

  g_object_unref (builder);
}

static GtkWidget *
get_parent_menubar (GtkWidget *menuitem)
{
  GtkMenuShell *menu_shell;
  GtkWidget *attach = NULL;

  menu_shell = CTK_MENU_SHELL (ctk_widget_get_parent (menuitem));

  g_assert (CTK_IS_MENU_SHELL (menu_shell));

  while (menu_shell && !CTK_IS_MENU_BAR (menu_shell))
    {
      if (CTK_IS_MENU (menu_shell) && 
	  (attach = ctk_menu_get_attach_widget (CTK_MENU (menu_shell))) != NULL)
	menu_shell = CTK_MENU_SHELL (ctk_widget_get_parent (attach));
      else
	menu_shell = NULL;
    }

  return menu_shell ? CTK_WIDGET (menu_shell) : NULL;
}

static void
test_menus (void)
{
  const gchar *buffer =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <accel-groups>"
    "      <group name=\"accelgroup1\"/>"
    "    </accel-groups>"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"vbox1\">"
    "        <property name=\"visible\">True</property>"
    "        <property name=\"orientation\">vertical</property>"
    "        <child>"
    "          <object class=\"GtkMenuBar\" id=\"menubar1\">"
    "            <property name=\"visible\">True</property>"
    "            <child>"
    "              <object class=\"GtkMenuItem\" id=\"menuitem1\">"
    "                <property name=\"visible\">True</property>"
    "                <property name=\"label\" translatable=\"yes\">_File</property>"
    "                <property name=\"use_underline\">True</property>"
    "                <child type=\"submenu\">"
    "                  <object class=\"GtkMenu\" id=\"menu1\">"
    "                    <property name=\"visible\">True</property>"
    "                    <child>"
    "                      <object class=\"GtkImageMenuItem\" id=\"imagemenuitem1\">"
    "                        <property name=\"label\">gtk-new</property>"
    "                        <property name=\"visible\">True</property>"
    "                        <property name=\"use_stock\">True</property>"
    "                        <property name=\"accel_group\">accelgroup1</property>"
    "                      </object>"
    "                    </child>"
    "                  </object>"
    "                </child>"
    "              </object>"
    "            </child>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "<object class=\"GtkAccelGroup\" id=\"accelgroup1\"/>"
    "</interface>";

  const gchar *buffer1 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <accel-groups>"
    "      <group name=\"accelgroup1\"/>"
    "    </accel-groups>"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"vbox1\">"
    "        <property name=\"visible\">True</property>"
    "        <property name=\"orientation\">vertical</property>"
    "        <child>"
    "          <object class=\"GtkMenuBar\" id=\"menubar1\">"
    "            <property name=\"visible\">True</property>"
    "            <child>"
    "              <object class=\"GtkImageMenuItem\" id=\"imagemenuitem1\">"
    "                <property name=\"visible\">True</property>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"custom1\">"
    "                    <property name=\"visible\">True</property>"
    "                    <property name=\"label\">a label</property>"
    "                  </object>"
    "                </child>"
    "              </object>"
    "            </child>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "<object class=\"GtkAccelGroup\" id=\"accelgroup1\"/>"
    "</interface>";
  GtkBuilder *builder;
  GtkWidget *child;
  GtkWidget *window, *item;
  GtkAccelGroup *accel_group;
  GtkWidget *item_accel_label, *sample_accel_label, *sample_menu_item, *custom;

  /* Check that the item has the correct accel label string set
   */
  builder = builder_new_from_string (buffer, -1, NULL);
  window = (GtkWidget *)ctk_builder_get_object (builder, "window1");
  item = (GtkWidget *)ctk_builder_get_object (builder, "imagemenuitem1");
  accel_group = (GtkAccelGroup *)ctk_builder_get_object (builder, "accelgroup1");

  ctk_widget_show_all (window);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  sample_menu_item = ctk_image_menu_item_new_from_stock (CTK_STOCK_NEW, accel_group);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  child = ctk_bin_get_child (CTK_BIN (sample_menu_item));
  g_assert (child);
  g_assert (CTK_IS_ACCEL_LABEL (child));
  sample_accel_label = child;
  ctk_widget_show (sample_accel_label);

  child = ctk_bin_get_child (CTK_BIN (item));
  g_assert (child);
  g_assert (CTK_IS_ACCEL_LABEL (child));
  item_accel_label = child;

  ctk_accel_label_refetch (CTK_ACCEL_LABEL (sample_accel_label));
  ctk_accel_label_refetch (CTK_ACCEL_LABEL (item_accel_label));

  g_assert (ctk_label_get_text (CTK_LABEL (sample_accel_label)) != NULL);
  g_assert (ctk_label_get_text (CTK_LABEL (item_accel_label)) != NULL);
  g_assert (strcmp (ctk_label_get_text (CTK_LABEL (item_accel_label)),
		    ctk_label_get_text (CTK_LABEL (sample_accel_label))) == 0);

  /* Check the menu hierarchy worked here  */
  g_assert (get_parent_menubar (item));

  ctk_widget_destroy (CTK_WIDGET (window));
  ctk_widget_destroy (sample_menu_item);
  g_object_unref (builder);


  /* Check that we can add alien children to menu items via normal
   * GtkContainer apis.
   */
  builder = builder_new_from_string (buffer1, -1, NULL);
  window = (GtkWidget *)ctk_builder_get_object (builder, "window1");
  item = (GtkWidget *)ctk_builder_get_object (builder, "imagemenuitem1");
  custom = (GtkWidget *)ctk_builder_get_object (builder, "custom1");

  g_assert (ctk_widget_get_parent (custom) == item);

  ctk_widget_destroy (CTK_WIDGET (window));
  g_object_unref (builder);
}

static void
test_file (const gchar *filename)
{
  GtkBuilder *builder;
  GError *error = NULL;
  GSList *l, *objects;

  builder = ctk_builder_new ();

  if (!ctk_builder_add_from_file (builder, filename, &error))
    {
      g_error ("%s", error->message);
      g_error_free (error);
      return;
    }

  objects = ctk_builder_get_objects (builder);
  for (l = objects; l; l = l->next)
    {
      GObject *obj = (GObject*)l->data;

      if (CTK_IS_DIALOG (obj))
	{
	  g_print ("Running dialog %s.\n",
		   ctk_widget_get_name (CTK_WIDGET (obj)));
	  ctk_dialog_run (CTK_DIALOG (obj));
	}
      else if (CTK_IS_WINDOW (obj))
	{
	  g_signal_connect (obj, "destroy", G_CALLBACK (ctk_main_quit), NULL);
	  g_print ("Showing %s.\n",
		   ctk_widget_get_name (CTK_WIDGET (obj)));
	  ctk_widget_show_all (CTK_WIDGET (obj));
	}
    }

  ctk_main ();

  g_object_unref (builder);
}

static void
test_message_area (void)
{
  GtkBuilder *builder;
  GObject *obj, *obj1;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkInfoBar\" id=\"infobar1\">"
    "    <child internal-child=\"content_area\">"
    "      <object class=\"GtkHBox\" id=\"contentarea1\">"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"content\">"
    "            <property name=\"label\" translatable=\"yes\">Message</property>"
    "          </object>"
    "          <packing>"
    "            <property name='expand'>False</property>"
    "          </packing>"
    "        </child>"
    "      </object>"
    "    </child>"
    "    <child internal-child=\"action_area\">"
    "      <object class=\"GtkVButtonBox\" id=\"actionarea1\">"
    "        <child>"
    "          <object class=\"GtkButton\" id=\"button_ok\">"
    "            <property name=\"label\">gtk-ok</property>"
    "            <property name=\"use-stock\">yes</property>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "    <action-widgets>"
    "      <action-widget response=\"1\">button_ok</action-widget>"
    "    </action-widgets>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);
  obj = ctk_builder_get_object (builder, "infobar1");
  g_assert (CTK_IS_INFO_BAR (obj));
  obj1 = ctk_builder_get_object (builder, "content");
  g_assert (CTK_IS_LABEL (obj1));

  obj1 = ctk_builder_get_object (builder, "button_ok");
  g_assert (CTK_IS_BUTTON (obj1));

  g_object_unref (builder);
}

static void
test_gmenu (void)
{
  GtkBuilder *builder;
  GObject *obj, *obj1;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "  </object>"
    "  <menu id='edit-menu'>"
    "    <section>"
    "      <item>"
    "        <attribute name='label'>Undo</attribute>"
    "        <attribute name='action'>undo</attribute>"
    "      </item>"
    "      <item>"
    "        <attribute name='label'>Redo</attribute>"
    "        <attribute name='action'>redo</attribute>"
    "      </item>"
    "    </section>"
    "    <section></section>"
    "    <section>"
    "      <attribute name='label'>Copy &amp; Paste</attribute>"
    "      <item>"
    "        <attribute name='label'>Cut</attribute>"
    "        <attribute name='action'>cut</attribute>"
    "      </item>"
    "      <item>"
    "        <attribute name='label'>Copy</attribute>"
    "        <attribute name='action'>copy</attribute>"
    "      </item>"
    "      <item>"
    "        <attribute name='label'>Paste</attribute>"
    "        <attribute name='action'>paste</attribute>"
    "      </item>"
    "    </section>"
    "    <item><link name='section' id='blargh'>"
    "      <item>"
    "        <attribute name='label'>Bold</attribute>"
    "        <attribute name='action'>bold</attribute>"
    "      </item>"
    "      <submenu>"
    "        <attribute name='label'>Language</attribute>"
    "        <item>"
    "          <attribute name='label'>Latin</attribute>"
    "          <attribute name='action'>lang</attribute>"
    "          <attribute name='target'>'latin'</attribute>"
    "        </item>"
    "        <item>"
    "          <attribute name='label'>Greek</attribute>"
    "          <attribute name='action'>lang</attribute>"
    "          <attribute name='target'>'greek'</attribute>"
    "        </item>"
    "        <item>"
    "          <attribute name='label'>Urdu</attribute>"
    "          <attribute name='action'>lang</attribute>"
    "          <attribute name='target'>'urdu'</attribute>"
    "        </item>"
    "      </submenu>"
    "    </link></item>"
    "  </menu>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);
  obj = ctk_builder_get_object (builder, "window");
  g_assert (CTK_IS_WINDOW (obj));
  obj1 = ctk_builder_get_object (builder, "edit-menu");
  g_assert (G_IS_MENU_MODEL (obj1));
  obj1 = ctk_builder_get_object (builder, "blargh");
  g_assert (G_IS_MENU_MODEL (obj1));
  g_object_unref (builder);
}

static void
test_level_bar (void)
{
  GtkBuilder *builder;
  GError *error = NULL;
  GObject *obj, *obj1;
  const gchar buffer1[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <child>"
    "      <object class=\"GtkLevelBar\" id=\"levelbar\">"
    "        <property name=\"value\">4.70</property>"
    "        <property name=\"min-value\">2</property>"
    "        <property name=\"max-value\">5</property>"
    "        <offsets>"
    "          <offset name=\"low\" value=\"2.25\"/>"
    "          <offset name=\"custom\" value=\"3\"/>"
    "          <offset name=\"high\" value=\"3\"/>"
    "        </offsets>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const gchar buffer2[] =
    "<interface>"
    "  <object class=\"GtkLevelBar\" id=\"levelbar\">"
    "    <offsets>"
    "      <offset name=\"low\" bogus_attr=\"foo\"/>"
    "    </offsets>"
    "  </object>"
    "</interface>";
  const gchar buffer3[] =
    "<interface>"
    "  <object class=\"GtkLevelBar\" id=\"levelbar\">"
    "    <offsets>"
    "      <offset name=\"low\" value=\"1\"/>"
    "    </offsets>"
    "    <bogus_tag>"
    "    </bogus_tag>"
    "  </object>"
    "</interface>";

  builder = ctk_builder_new ();
  ctk_builder_add_from_string (builder, buffer1, -1, &error);
  g_assert (error == NULL);

  obj = ctk_builder_get_object (builder, "window");
  g_assert (CTK_IS_WINDOW (obj));
  obj1 = ctk_builder_get_object (builder, "levelbar");
  g_assert (CTK_IS_LEVEL_BAR (obj1));
  g_object_unref (builder);

  error = NULL;
  builder = ctk_builder_new ();
  ctk_builder_add_from_string (builder, buffer2, -1, &error);
  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE);
  g_error_free (error);
  g_object_unref (builder);

  error = NULL;
  builder = ctk_builder_new ();
  ctk_builder_add_from_string (builder, buffer3, -1, &error);
  g_assert_error (error, CTK_BUILDER_ERROR, CTK_BUILDER_ERROR_UNHANDLED_TAG);
  g_error_free (error);
  g_object_unref (builder);
}

static GObject *external_object = NULL, *external_object_swapped = NULL;

G_MODULE_EXPORT void
on_button_clicked (GtkButton *button, GObject *data)
{
  external_object = data;
}

G_MODULE_EXPORT void
on_button_clicked_swapped (GObject *data, GtkButton *button)
{
  external_object_swapped = data;
}

static void
test_expose_object (void)
{
  GtkBuilder *builder;
  GError *error = NULL;
  GtkWidget *image;
  GObject *obj;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkButton\" id=\"button\">"
    "    <property name=\"image\">external_image</property>"
    "    <signal name=\"clicked\" handler=\"on_button_clicked\" object=\"builder\" swapped=\"no\"/>"
    "    <signal name=\"clicked\" handler=\"on_button_clicked_swapped\" object=\"builder\"/>"
    "  </object>"
    "</interface>";

  image = ctk_image_new ();
  builder = ctk_builder_new ();
  ctk_builder_expose_object (builder, "external_image", G_OBJECT (image));
  ctk_builder_expose_object (builder, "builder", G_OBJECT (builder));
  ctk_builder_add_from_string (builder, buffer, -1, &error);
  g_assert (error == NULL);

  obj = ctk_builder_get_object (builder, "button");
  g_assert (CTK_IS_BUTTON (obj));

  g_assert (ctk_button_get_image (CTK_BUTTON (obj)) == image);

  /* Connect signals and fake clicked event */
  ctk_builder_connect_signals (builder, NULL);
  ctk_button_clicked (CTK_BUTTON (obj));

  g_assert (external_object == G_OBJECT (builder));
  g_assert (external_object_swapped == G_OBJECT (builder));

  g_object_unref (builder);
  g_object_unref (image);
}

static void
test_no_ids (void)
{
  GtkBuilder *builder;
  GError *error = NULL;
  GObject *obj;
  const gchar buffer[] =
    "<interface>"
    "  <object class=\"GtkInfoBar\">"
    "    <child internal-child=\"content_area\">"
    "      <object class=\"GtkHBox\">"
    "        <child>"
    "          <object class=\"GtkLabel\">"
    "            <property name=\"label\" translatable=\"yes\">Message</property>"
    "          </object>"
    "          <packing>"
    "            <property name='expand'>False</property>"
    "          </packing>"
    "        </child>"
    "      </object>"
    "    </child>"
    "    <child internal-child=\"action_area\">"
    "      <object class=\"GtkVButtonBox\">"
    "        <child>"
    "          <object class=\"GtkButton\" id=\"button_ok\">"
    "            <property name=\"label\">gtk-ok</property>"
    "            <property name=\"use-stock\">yes</property>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "    <action-widgets>"
    "      <action-widget response=\"1\">button_ok</action-widget>"
    "    </action-widgets>"
    "  </object>"
    "</interface>";

  builder = ctk_builder_new ();
  ctk_builder_add_from_string (builder, buffer, -1, &error);
  g_assert (error == NULL);

  obj = ctk_builder_get_object (builder, "button_ok");
  g_assert (CTK_IS_BUTTON (obj));

  g_object_unref (builder);
}

static void
test_property_bindings (void)
{
  const gchar *buffer =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <child>"
    "      <object class=\"GtkVBox\" id=\"vbox\">"
    "        <property name=\"visible\">True</property>"
    "        <property name=\"orientation\">vertical</property>"
    "        <child>"
    "          <object class=\"GtkCheckButton\" id=\"checkbutton\">"
    "            <property name=\"active\">false</property>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkButton\" id=\"button\">"
    "            <property name=\"sensitive\" bind-source=\"checkbutton\" bind-property=\"active\" bind-flags=\"sync-create\">false</property>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkButton\" id=\"button2\">"
    "            <property name=\"sensitive\" bind-source=\"checkbutton\" bind-property=\"active\" />"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  GtkBuilder *builder;
  GObject *checkbutton, *button, *button2, *window;
  
  builder = builder_new_from_string (buffer, -1, NULL);
  
  checkbutton = ctk_builder_get_object (builder, "checkbutton");
  g_assert (CTK_IS_CHECK_BUTTON (checkbutton));
  g_assert (!ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (checkbutton)));

  button = ctk_builder_get_object (builder, "button");
  g_assert (CTK_IS_BUTTON (button));
  g_assert (!ctk_widget_get_sensitive (CTK_WIDGET (button)));

  button2 = ctk_builder_get_object (builder, "button2");
  g_assert (CTK_IS_BUTTON (button2));
  g_assert (ctk_widget_get_sensitive (CTK_WIDGET (button2)));
  
  ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (checkbutton), TRUE);
  g_assert (ctk_widget_get_sensitive (CTK_WIDGET (button)));
  g_assert (ctk_widget_get_sensitive (CTK_WIDGET (button2)));
  
  window = ctk_builder_get_object (builder, "window");
  ctk_widget_destroy (CTK_WIDGET (window));
  g_object_unref (builder);
}

#define MY_CTK_GRID_TEMPLATE "\
<interface>\n\
 <template class=\"MyGtkGrid\" parent=\"GtkGrid\">\n\
   <property name=\"visible\">True</property>\n\
    <child>\n\
     <object class=\"GtkLabel\" id=\"label\">\n\
       <property name=\"visible\">True</property>\n\
     </object>\n\
  </child>\n\
 </template>\n\
</interface>\n"

#define MY_TYPE_CTK_GRID             (my_ctk_grid_get_type ())
#define MY_IS_CTK_GRID(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MY_TYPE_CTK_GRID))

typedef struct
{
  GtkGridClass parent_class;
} MyGtkGridClass;

typedef struct
{
  GtkLabel *label;
} MyGtkGridPrivate;

typedef struct
{
  GtkGrid parent_instance;
  GtkLabel *label;
  MyGtkGridPrivate *priv;
} MyGtkGrid;

G_DEFINE_TYPE_WITH_PRIVATE (MyGtkGrid, my_ctk_grid, CTK_TYPE_GRID);

static void
my_ctk_grid_init (MyGtkGrid *grid)
{
  grid->priv = my_ctk_grid_get_instance_private (grid);
  ctk_widget_init_template (CTK_WIDGET (grid));
}

static void
my_ctk_grid_class_init (MyGtkGridClass *klass)
{
  GBytes *template = g_bytes_new_static (MY_CTK_GRID_TEMPLATE, strlen (MY_CTK_GRID_TEMPLATE));
  GtkWidgetClass *widget_class = CTK_WIDGET_CLASS (klass);

  ctk_widget_class_set_template (widget_class, template);
  ctk_widget_class_bind_template_child (widget_class, MyGtkGrid, label);
  ctk_widget_class_bind_template_child_private (widget_class, MyGtkGrid, label);
}

static void
test_template ()
{
  MyGtkGrid *my_ctk_grid;

  /* make sure the type we are trying to register does not exist */
  g_assert (!g_type_from_name ("MyGtkGrid"));

  /* create the template object */
  my_ctk_grid = g_object_new (MY_TYPE_CTK_GRID, NULL);

  /* Check everything is fine */
  g_assert (g_type_from_name ("MyGtkGrid"));
  g_assert (MY_IS_CTK_GRID (my_ctk_grid));
  g_assert (my_ctk_grid->label == my_ctk_grid->priv->label);
  g_assert (CTK_IS_LABEL (my_ctk_grid->label));
  g_assert (CTK_IS_LABEL (my_ctk_grid->priv->label));
}

G_MODULE_EXPORT void
on_cellrenderertoggle1_toggled (GtkCellRendererToggle *cell)
{
}

static void
test_anaconda_signal (void)
{
  GtkBuilder *builder;
  const gchar buffer[] = 
    "<?xml version='1.0' encoding='UTF-8'?>"
    "<!-- Generated with glade 3.18.3 -->"
    "<interface>"
    "  <requires lib='gtk+' version='3.12'/>"
    "  <object class='GtkListStore' id='liststore1'>"
    "    <columns>"
    "      <!-- column-name use -->"
    "      <column type='gboolean'/>"
    "    </columns>"
    "  </object>"
    "  <object class='GtkWindow' id='window1'>"
    "    <property name='can_focus'>False</property>"
    "    <child>"
    "      <object class='GtkTreeView' id='treeview1'>"
    "        <property name='visible'>True</property>"
    "        <property name='can_focus'>True</property>"
    "        <property name='model'>liststore1</property>"
    "        <child internal-child='selection'>"
    "          <object class='GtkTreeSelection' id='treeview-selection1'/>"
    "        </child>"
    "        <child>"
    "          <object class='GtkTreeViewColumn' id='treeviewcolumn1'>"
    "            <property name='title' translatable='yes'>column</property>"
    "            <child>"
    "              <object class='GtkCellRendererToggle' id='cellrenderertoggle1'>"
    "                <signal name='toggled' handler='on_cellrenderertoggle1_toggled' swapped='no'/>"
    "              </object>"
    "              <attributes>"
    "                <attribute name='active'>0</attribute>"
    "              </attributes>"
    "            </child>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);
  ctk_builder_connect_signals (builder, NULL);

  g_object_unref (builder);
}

static void
test_file_filter (void)
{
  GtkBuilder *builder;
  GObject *obj;
  GtkFileFilter *filter;
  GtkFileFilterInfo info;

  const gchar buffer[] =
    "<interface>"
    "  <object class='GtkFileFilter' id='filter1'>"
    "    <mime-types>"
    "      <mime-type>text/plain</mime-type>"
    "      <mime-type>image/*</mime-type>"
    "    </mime-types>"
    "    <patterns>"
    "      <pattern>*.txt</pattern>"
    "      <pattern>*.png</pattern>"
    "    </patterns>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);
  obj = ctk_builder_get_object (builder, "filter1");
  g_assert (CTK_IS_FILE_FILTER (obj));
  filter = CTK_FILE_FILTER (obj);
  g_assert_cmpstr (ctk_file_filter_get_name (filter), ==, "filter1");
  g_assert (ctk_file_filter_get_needed (filter) & CTK_FILE_FILTER_MIME_TYPE);
  g_assert (ctk_file_filter_get_needed (filter) & CTK_FILE_FILTER_DISPLAY_NAME);

  info.filename = "test1.txt";
  info.display_name = "test1.txt";
  info.contains = CTK_FILE_FILTER_FILENAME | CTK_FILE_FILTER_DISPLAY_NAME;
  g_assert (ctk_file_filter_filter (filter, &info));

  info.mime_type = "application/x-pdf";
  info.contains = CTK_FILE_FILTER_MIME_TYPE;
  g_assert (!ctk_file_filter_filter (filter, &info));

  g_object_unref (builder);
}

int
main (int argc, char **argv)
{
  /* initialize test program */
  ctk_test_init (&argc, &argv);

  if (argc > 1)
    {
      test_file (argv[1]);
      return 0;
    }

  g_test_add_func ("/Builder/Parser", test_parser);
  g_test_add_func ("/Builder/Types", test_types);
  g_test_add_func ("/Builder/Construct-Only Properties", test_construct_only_property);
  g_test_add_func ("/Builder/Children", test_children);
  g_test_add_func ("/Builder/Child Properties", test_child_properties);
  g_test_add_func ("/Builder/Object Properties", test_object_properties);
  g_test_add_func ("/Builder/Notebook", test_notebook);
  g_test_add_func ("/Builder/Domain", test_domain);
  g_test_add_func ("/Builder/Signal Autoconnect", test_connect_signals);
  g_test_add_func ("/Builder/UIManager Simple", test_uimanager_simple);
  g_test_add_func ("/Builder/Spin Button", test_spin_button);
  g_test_add_func ("/Builder/SizeGroup", test_sizegroup);
  g_test_add_func ("/Builder/ListStore", test_list_store);
  g_test_add_func ("/Builder/TreeStore", test_tree_store);
  g_test_add_func ("/Builder/TreeView Column", test_treeview_column);
  g_test_add_func ("/Builder/IconView", test_icon_view);
  g_test_add_func ("/Builder/ComboBox", test_combo_box);
#if 0
  g_test_add_func ("/Builder/ComboBox Entry", test_combo_box_entry);
#endif
  g_test_add_func ("/Builder/CellView", test_cell_view);
  g_test_add_func ("/Builder/Dialog", test_dialog);
  g_test_add_func ("/Builder/Accelerators", test_accelerators);
  g_test_add_func ("/Builder/Widget", test_widget);
  g_test_add_func ("/Builder/Value From String", test_value_from_string);
  g_test_add_func ("/Builder/Reference Counting", test_reference_counting);
  g_test_add_func ("/Builder/Window", test_window);
  g_test_add_func ("/Builder/IconFactory", test_icon_factory);
  g_test_add_func ("/Builder/PangoAttributes", test_pango_attributes);
  g_test_add_func ("/Builder/Requires", test_requires);
  g_test_add_func ("/Builder/AddObjects", test_add_objects);
  g_test_add_func ("/Builder/Menus", test_menus);
  g_test_add_func ("/Builder/MessageArea", test_message_area);
  g_test_add_func ("/Builder/MessageDialog", test_message_dialog);
  g_test_add_func ("/Builder/GMenu", test_gmenu);
  g_test_add_func ("/Builder/LevelBar", test_level_bar);
  g_test_add_func ("/Builder/Expose Object", test_expose_object);
  g_test_add_func ("/Builder/Template", test_template);
  g_test_add_func ("/Builder/No IDs", test_no_ids);
  g_test_add_func ("/Builder/Property Bindings", test_property_bindings);
  g_test_add_func ("/Builder/anaconda-signal", test_anaconda_signal);
  g_test_add_func ("/Builder/FileFilter", test_file_filter);

  return g_test_run();
}

