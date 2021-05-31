/* GTK - The GIMP Toolkit
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
 *
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 *
 */

#ifndef __CTK_IM_CONTEXT_THAI_H__
#define __CTK_IM_CONTEXT_THAI_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

extern GType ctk_type_im_context_thai;

#define CTK_TYPE_IM_CONTEXT_THAI            (ctk_type_im_context_thai)
#define CTK_IM_CONTEXT_THAI(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CTK_TYPE_IM_CONTEXT_THAI, GtkIMContextThai))
#define CTK_IM_CONTEXT_THAI_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CTK_TYPE_IM_CONTEXT_THAI, GtkIMContextThaiClass))
#define CTK_IS_IM_CONTEXT_THAI(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CTK_TYPE_IM_CONTEXT_THAI))
#define CTK_IS_IM_CONTEXT_THAI_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CTK_TYPE_IM_CONTEXT_THAI))
#define CTK_IM_CONTEXT_THAI_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CTK_TYPE_IM_CONTEXT_THAI, GtkIMContextThaiClass))


typedef struct _GtkIMContextThai       GtkIMContextThai;
typedef struct _GtkIMContextThaiClass  GtkIMContextThaiClass;

typedef enum
{
  ISC_PASSTHROUGH,
  ISC_BASICCHECK,
  ISC_STRICT
} GtkIMContextThaiISCMode;
#define CTK_IM_CONTEXT_THAI_BUFF_SIZE 2

struct _GtkIMContextThai
{
  GtkIMContext object;

#ifndef CTK_IM_CONTEXT_THAI_NO_FALLBACK
  gunichar                char_buff[CTK_IM_CONTEXT_THAI_BUFF_SIZE];
#endif /* !CTK_IM_CONTEXT_THAI_NO_FALLBACK */
  GtkIMContextThaiISCMode isc_mode;
};

struct _GtkIMContextThaiClass
{
  GtkIMContextClass parent_class;
};

void ctk_im_context_thai_register_type (GTypeModule *type_module);
GtkIMContext *ctk_im_context_thai_new (void);

GtkIMContextThaiISCMode
  ctk_im_context_thai_get_isc_mode (GtkIMContextThai *context_thai);

GtkIMContextThaiISCMode
  ctk_im_context_thai_set_isc_mode (GtkIMContextThai *context_thai,
                                    GtkIMContextThaiISCMode mode);

G_END_DECLS

#endif /* __CTK_IM_CONTEXT_THAI_H__ */
