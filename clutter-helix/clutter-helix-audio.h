/*
 * Clutter-Helix.
 *
 * Helix integration library for Clutter.
 *
 * Authored By Rusty Lynch <rusty.lynch@intel.com>
 *
 * Derived from Clutter-Gst, Authored By 
 * Matthew Allum  <mallum@openedhand.com>
 * Jorn Baayen <jorn@openedhand.com>
 *
 * Copyright 2006 OpenedHand
 * Copyright 2008 Intel Corp.
 *
 * Contributors:
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

#ifndef _HAVE_CLUTTER_HELIX_AUDIO_H
#define _HAVE_CLUTTER_HELIX_AUDIO_H

#include <glib-object.h>
#include <clutter/clutter.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "player.h"

G_BEGIN_DECLS
#define CLUTTER_HELIX_TYPE_AUDIO clutter_helix_audio_get_type()

#define CLUTTER_HELIX_AUDIO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_HELIX_TYPE_AUDIO, ClutterHelixAudio))

#define CLUTTER_HELIX_AUDIO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_HELIX_TYPE_AUDIO, ClutterHelixAudioClass))

#define CLUTTER_HELIX_IS_AUDIO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_HELIX_TYPE_AUDIO))

#define CLUTTER_HELIX_IS_AUDIO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_HELIX_TYPE_AUDIO))

#define CLUTTER_HELIX_AUDIO_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_HELIX_TYPE_AUDIO, ClutterHelixAudioClass))

typedef struct _ClutterHelixAudio        ClutterHelixAudio;
typedef struct _ClutterHelixAudioClass   ClutterHelixAudioClass;
typedef struct _ClutterHelixAudioPrivate ClutterHelixAudioPrivate;

/**
 * ClutterHelixAudio:
 *
 * Base class for #ClutterHelixAudio.
 */
struct _ClutterHelixAudio
{
  /*< private >*/
  GObject                   parent;
  ClutterHelixAudioPrivate *priv;
}; 

/**
 * ClutterHelixAudioClass:
 *
 * Base class for #ClutterHelixAudio.
 */
struct _ClutterHelixAudioClass 
{
  /*< private >*/
  GObjectClass parent_class;

  /* Future padding */
  void (* _clutter_reserved1) (void);
  void (* _clutter_reserved2) (void);
  void (* _clutter_reserved3) (void);
  void (* _clutter_reserved4) (void);
  void (* _clutter_reserved5) (void);
  void (* _clutter_reserved6) (void);
}; 

GType         clutter_helix_audio_get_type    (void) G_GNUC_CONST;
ClutterActor *clutter_helix_audio_new         (void);


G_END_DECLS

#endif
