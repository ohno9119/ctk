/*
 * Copyright © 2012 Red Hat Inc.
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
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __CTK_CSS_ANIMATION_PRIVATE_H__
#define __CTK_CSS_ANIMATION_PRIVATE_H__

#include "ctkstyleanimationprivate.h"

#include "ctkcsskeyframesprivate.h"
#include "ctkprogresstrackerprivate.h"

G_BEGIN_DECLS

#define CTK_TYPE_CSS_ANIMATION           (_ctk_css_animation_get_type ())
#define CTK_CSS_ANIMATION(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, CTK_TYPE_CSS_ANIMATION, GtkCssAnimation))
#define CTK_CSS_ANIMATION_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, CTK_TYPE_CSS_ANIMATION, GtkCssAnimationClass))
#define CTK_IS_CSS_ANIMATION(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, CTK_TYPE_CSS_ANIMATION))
#define CTK_IS_CSS_ANIMATION_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, CTK_TYPE_CSS_ANIMATION))
#define CTK_CSS_ANIMATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), CTK_TYPE_CSS_ANIMATION, GtkCssAnimationClass))

typedef struct _GtkCssAnimation           GtkCssAnimation;
typedef struct _GtkCssAnimationClass      GtkCssAnimationClass;

struct _GtkCssAnimation
{
  GtkStyleAnimation parent;

  char            *name;
  GtkCssKeyframes *keyframes;
  GtkCssValue     *ease;
  GtkCssDirection  direction;
  GtkCssPlayState  play_state;
  GtkCssFillMode   fill_mode;
  GtkProgressTracker tracker;
};

struct _GtkCssAnimationClass
{
  GtkStyleAnimationClass parent_class;
};

GType                   _ctk_css_animation_get_type        (void) G_GNUC_CONST;

GtkStyleAnimation *     _ctk_css_animation_new             (const char         *name,
                                                            GtkCssKeyframes    *keyframes,
                                                            gint64              timestamp,
                                                            gint64              delay_us,
                                                            gint64              duration_us,
                                                            GtkCssValue        *ease,
                                                            GtkCssDirection     direction,
                                                            GtkCssPlayState     play_state,
                                                            GtkCssFillMode      fill_mode,
                                                            double              iteration_count);

GtkStyleAnimation *     _ctk_css_animation_advance_with_play_state (GtkCssAnimation   *animation,
                                                                    gint64             timestamp,
                                                                    GtkCssPlayState    play_state);

const char *            _ctk_css_animation_get_name        (GtkCssAnimation   *animation);

G_END_DECLS

#endif /* __CTK_CSS_ANIMATION_PRIVATE_H__ */
