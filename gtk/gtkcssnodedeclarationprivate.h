/*
 * Copyright © 2014 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CTK_CSS_NODE_DECLARATION_PRIVATE_H__
#define __CTK_CSS_NODE_DECLARATION_PRIVATE_H__

#include "gtkcsstypesprivate.h"
#include "gtkenums.h"
#include "gtkwidgetpath.h"

G_BEGIN_DECLS

GtkCssNodeDeclaration * ctk_css_node_declaration_new                    (void);
GtkCssNodeDeclaration * ctk_css_node_declaration_ref                    (GtkCssNodeDeclaration         *decl);
void                    ctk_css_node_declaration_unref                  (GtkCssNodeDeclaration         *decl);

gboolean                ctk_css_node_declaration_set_junction_sides     (GtkCssNodeDeclaration        **decl,
                                                                         GtkJunctionSides               junction_sides);
GtkJunctionSides        ctk_css_node_declaration_get_junction_sides     (const GtkCssNodeDeclaration   *decl);
gboolean                ctk_css_node_declaration_set_type               (GtkCssNodeDeclaration        **decl,
                                                                         GType                          type);
GType                   ctk_css_node_declaration_get_type               (const GtkCssNodeDeclaration   *decl);
gboolean                ctk_css_node_declaration_set_name               (GtkCssNodeDeclaration        **decl,
                                                                         /*interned*/ const char       *name);
/*interned*/ const char*ctk_css_node_declaration_get_name               (const GtkCssNodeDeclaration   *decl);
gboolean                ctk_css_node_declaration_set_id                 (GtkCssNodeDeclaration        **decl,
                                                                         const char                    *id);
const char *            ctk_css_node_declaration_get_id                 (const GtkCssNodeDeclaration   *decl);
gboolean                ctk_css_node_declaration_set_state              (GtkCssNodeDeclaration        **decl,
                                                                         GtkStateFlags                  flags);
GtkStateFlags           ctk_css_node_declaration_get_state              (const GtkCssNodeDeclaration   *decl);

gboolean                ctk_css_node_declaration_add_class              (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         class_quark);
gboolean                ctk_css_node_declaration_remove_class           (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         class_quark);
gboolean                ctk_css_node_declaration_clear_classes          (GtkCssNodeDeclaration        **decl);
gboolean                ctk_css_node_declaration_has_class              (const GtkCssNodeDeclaration   *decl,
                                                                         GQuark                         class_quark);
const GQuark *          ctk_css_node_declaration_get_classes            (const GtkCssNodeDeclaration   *decl,
                                                                         guint                         *n_classes);

gboolean                ctk_css_node_declaration_add_region             (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         region_quark,
                                                                         GtkRegionFlags                 flags);
gboolean                ctk_css_node_declaration_remove_region          (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         region_quark);
gboolean                ctk_css_node_declaration_clear_regions          (GtkCssNodeDeclaration        **decl);
gboolean                ctk_css_node_declaration_has_region             (const GtkCssNodeDeclaration   *decl,
                                                                         GQuark                         region_quark,
                                                                         GtkRegionFlags                *flags_return);
GList *                 ctk_css_node_declaration_list_regions           (const GtkCssNodeDeclaration   *decl);

guint                   ctk_css_node_declaration_hash                   (gconstpointer                  elem);
gboolean                ctk_css_node_declaration_equal                  (gconstpointer                  elem1,
                                                                         gconstpointer                  elem2);

void                    ctk_css_node_declaration_add_to_widget_path     (const GtkCssNodeDeclaration   *decl,
                                                                         GtkWidgetPath                 *path,
                                                                         guint                          pos);

void                    ctk_css_node_declaration_print                  (const GtkCssNodeDeclaration   *decl,
                                                                         GString                       *string);

G_END_DECLS

#endif /* __CTK_CSS_NODE_DECLARATION_PRIVATE_H__ */
