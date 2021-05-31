/* GTK - The GIMP Toolkit
 * ctkprintoperation.h: Print Operation
 * Copyright (C) 2006, Red Hat, Inc.
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

#ifndef __CTK_PRINTER_PRIVATE_H__
#define __CTK_PRINTER_PRIVATE_H__

#include <ctk/ctk.h>
#include <ctk/ctkunixprint.h>
#include "ctkprinteroptionset.h"

G_BEGIN_DECLS

GtkPrinterOptionSet *_ctk_printer_get_options               (GtkPrinter          *printer,
							     GtkPrintSettings    *settings,
							     GtkPageSetup        *page_setup,
							     GtkPrintCapabilities capabilities);
gboolean             _ctk_printer_mark_conflicts            (GtkPrinter          *printer,
							     GtkPrinterOptionSet *options);
void                 _ctk_printer_get_settings_from_options (GtkPrinter          *printer,
							     GtkPrinterOptionSet *options,
							     GtkPrintSettings    *settings);
void                 _ctk_printer_prepare_for_print         (GtkPrinter          *printer,
							     GtkPrintJob         *print_job,
							     GtkPrintSettings    *settings,
							     GtkPageSetup        *page_setup);
cairo_surface_t *    _ctk_printer_create_cairo_surface      (GtkPrinter          *printer,
							     GtkPrintSettings    *settings,
							     gdouble              width,
							     gdouble              height,
							     GIOChannel          *cache_io);
GHashTable *         _ctk_printer_get_custom_widgets        (GtkPrinter          *printer);
gboolean             _ctk_printer_get_hard_margins_for_paper_size (GtkPrinter       *printer,
								   GtkPaperSize     *paper_size,
								   gdouble          *top,
								   gdouble          *bottom,
								   gdouble          *left,
								   gdouble          *right);

/* GtkPrintJob private methods: */
GDK_AVAILABLE_IN_ALL
void ctk_print_job_set_status (GtkPrintJob   *job,
			       GtkPrintStatus status);

G_END_DECLS
#endif /* __CTK_PRINT_OPERATION_PRIVATE_H__ */
