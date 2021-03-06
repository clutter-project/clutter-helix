/*
 * Clutter-Helix.
 *
 * Helix integration library for Clutter.
 *
 * Authored By Paul Bu <long.bu@intel.com>
 *
 * Derived from Clutter-Gst, Authored By 
 * Matthew Allum  <mallum@openedhand.com>
 * Jorn Baayen <jorn@openedhand.com>
 *
 * Copyright 2006 OpenedHand
 * Copyright 2008 Intel Corp.
 *
 * Contributors:
 * Rusty Lynch <rusty.lynch@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _HAVE_CLUTTER_HELIX_VIDEO_TEXTURE_H
#define _HAVE_CLUTTER_HELIX_VIDEO_TEXTURE_H

#include <glib-object.h>
#include <clutter/clutter.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS
#define CLUTTER_HELIX_TYPE_VIDEO_TEXTURE clutter_helix_video_texture_get_type()

#define CLUTTER_HELIX_VIDEO_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_HELIX_TYPE_VIDEO_TEXTURE, ClutterHelixVideoTexture))

#define CLUTTER_HELIX_VIDEO_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_HELIX_TYPE_VIDEO_TEXTURE, ClutterHelixVideoTextureClass))

#define CLUTTER_HELIX_IS_VIDEO_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_HELIX_TYPE_VIDEO_TEXTURE))

#define CLUTTER_HELIX_IS_VIDEO_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_HELIX_TYPE_VIDEO_TEXTURE))

#define CLUTTER_HELIX_VIDEO_TEXTURE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_HELIX_TYPE_VIDEO_TEXTURE, ClutterHelixVideoTextureClass))

typedef struct _ClutterHelixVideoTexture        ClutterHelixVideoTexture;
typedef struct _ClutterHelixVideoTextureClass   ClutterHelixVideoTextureClass;
typedef struct _ClutterHelixVideoTexturePrivate ClutterHelixVideoTexturePrivate;

/**
 * ClutterHelixVideoTexture:
 *
 * Subclass of #ClutterTexture that displays videos using Helix.
 *
 * The #ClutterHelixVideoTexture structure contains only private data and
 * should not be accessed directly.
 */
struct _ClutterHelixVideoTexture
{
  /*< private >*/
  ClutterTexture              parent;
  ClutterHelixVideoTexturePrivate *priv;
}; 

/**
 * ClutterHelixVideoTextureClass:
 *
 * Base class for #ClutterHelixVideoTexture.
 */
struct _ClutterHelixVideoTextureClass 
{
  /*< private >*/
  ClutterTextureClass parent_class;

  /* Future padding */
  void (* _clutter_reserved1) (void);
  void (* _clutter_reserved2) (void);
  void (* _clutter_reserved3) (void);
  void (* _clutter_reserved4) (void);
  void (* _clutter_reserved5) (void);
  void (* _clutter_reserved6) (void);
}; 

GType         clutter_helix_video_texture_get_type    (void) G_GNUC_CONST;
ClutterActor *clutter_helix_video_texture_new         (void);


G_END_DECLS

#endif
