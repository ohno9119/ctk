/* CTK - The GIMP Toolkit
 * Copyright © 2014 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __CTK_POPOVER_PRIVATE_H__
#define __CTK_POPOVER_PRIVATE_H__

#include "ctkpopover.h"

G_BEGIN_DECLS

void ctk_popover_update_position (CtkPopover *popover);
CtkWidget *ctk_popover_get_prev_default (CtkPopover *popover);

G_END_DECLS

#endif /* __CTK_POPOVER_PRIVATE_H__ */
