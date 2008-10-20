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

/**
 * SECTION:clutter-helix-audio
 * @short_description: Actor for playback of audio files.
 *
 * #ClutterHelixAudio is a #ClutterActor that plays audio files.
 */

#include "config.h"

#include "clutter-helix-audio.h"
#include "player.h"

#include <glib.h>

struct _ClutterHelixAudioPrivate
{
  void             *player;
  char             *uri;
  gboolean          can_seek;
  int               buffer_percent;
  int               duration;
  guint             tick_timeout_id;
  EHXPlayerState    state;
  GAsyncQueue      *async_queue;
  unsigned int      x;
  unsigned int      y;
  unsigned int      width;
  unsigned int      height;
};

enum {
  PROP_0,
  /* ClutterMedia proprs */
  PROP_URI,
  PROP_PLAYING,
  PROP_POSITION,
  PROP_VOLUME,
  PROP_CAN_SEEK,
  PROP_BUFFER_PERCENT,
  PROP_DURATION
};

#define TICK_TIMEOUT 0.5

static void clutter_media_init (ClutterMediaInterface *iface);

static gboolean tick_timeout (ClutterHelixAudio *audio);

G_DEFINE_TYPE_WITH_CODE (ClutterHelixAudio,
                         clutter_helix_audio,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_MEDIA,
                                                clutter_media_init));

/* Interface implementation */

static void
set_uri (ClutterMedia *media,
	 const char      *uri)
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);
  ClutterHelixAudioPrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_AUDIO (audio));

  priv = audio->priv;

  if (!priv->player)
    return;

  g_free (priv->uri);

  if (uri) 
    {
      priv->uri = g_strdup (uri);
    
      /**
       * Ensure the tick timeout is installed.
       * 
       * We also have it installed in PAUSED state, because
       * seeks etc may have a delayed effect on the position.
       **/
      if (priv->tick_timeout_id == 0)
        {
	  priv->tick_timeout_id = g_timeout_add (TICK_TIMEOUT * 1000,
						 (GSourceFunc) tick_timeout,
						 audio);
	}
       
      player_openurl(priv->player, priv->uri);
    } 
  else 
    {
      priv->uri = NULL;
    
      if (priv->tick_timeout_id > 0) 
	{
	  g_source_remove (priv->tick_timeout_id);
	  priv->tick_timeout_id = 0;
	}
    }
  
  priv->can_seek = FALSE;
  priv->duration = 0;

  
  /*
   * Emit notififications for all these to make sure UI is not showing
   * any properties of the old URI.
   */
  g_object_notify (G_OBJECT (audio), "uri");
  g_object_notify (G_OBJECT (audio), "can-seek");
  g_object_notify (G_OBJECT (audio), "duration");
  g_object_notify (G_OBJECT (audio), "position");
}

static const char *
get_uri (ClutterMedia *media)
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);
  ClutterHelixAudioPrivate *priv; 

  g_return_val_if_fail (CLUTTER_HELIX_IS_AUDIO (audio), NULL);

  priv = audio->priv;

  return priv->uri;
}

static void
set_playing (ClutterMedia *media,
	     gboolean         playing)
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);
  ClutterHelixAudioPrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_AUDIO (audio));

  priv = audio->priv;

  if (!priv->player)
    return;
        
  if (priv->uri) 
    {
      if (playing)
	player_begin(priv->player);
      else
	player_pause(priv->player);
    } 
  else 
    {
      if (playing)
	g_warning ("Tried to play, but no URI is loaded.");
    }
  
  g_object_notify (G_OBJECT (audio), "playing");
  g_object_notify (G_OBJECT (audio), "position");
}

static gboolean
get_playing (ClutterMedia *media)
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);
  ClutterHelixAudioPrivate *priv; 

  g_return_val_if_fail (CLUTTER_HELIX_IS_AUDIO (audio), FALSE);

  priv = audio->priv;

  if (!priv->player)
    return FALSE;

  return (priv->state == PLAYER_STATE_PLAYING);
}

static void
set_position (ClutterMedia *media,
	      int              position) /* seconds */
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);
  ClutterHelixAudioPrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_AUDIO (audio));

  priv = audio->priv;

  if (!priv->player)
    return;

  player_seek(priv->player, position * 1000);
}

static int
get_position (ClutterMedia *media)
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);
  ClutterHelixAudioPrivate *priv; 
  gint32 position;

  g_return_val_if_fail (CLUTTER_HELIX_IS_AUDIO (audio), -1);

  priv = audio->priv;

  if (!priv->player)
    return -1;
  
  position = get_curr_playtime(priv->player);
  return (position / 1000);
}

static void
set_volume (ClutterMedia *media,
	    double           volume)
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);
  ClutterHelixAudioPrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_AUDIO (audio));

  priv = audio->priv;
  
  if (!priv->player)
    return;
 
  unsigned short volume_in_u16 = volume * (0xffff);
  player_setvolume(priv->player, volume_in_u16);  
  g_object_notify (G_OBJECT (audio), "volume");
}

static double
get_volume (ClutterMedia *media)
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);
  ClutterHelixAudioPrivate *priv; 
  double volume;

  g_return_val_if_fail (CLUTTER_HELIX_IS_AUDIO (audio), 0.0);

  priv = audio->priv;
  
  if (!priv->player)
    return 0.0;

  int ret;
  ret = player_getvolume(priv->player);
  if (ret < 0)
    ret = 0;

  volume = (double)ret/(double)(0xffff);

  return volume;
}

static gboolean
can_seek (ClutterMedia *media)
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);

  g_return_val_if_fail (CLUTTER_HELIX_IS_AUDIO (audio), FALSE);
  
  return audio->priv->can_seek;
}

static int
get_buffer_percent (ClutterMedia *media)
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);

  g_return_val_if_fail (CLUTTER_HELIX_IS_AUDIO (audio), -1);
  
  return audio->priv->buffer_percent;
}

static int
get_duration (ClutterMedia *media)
{
  ClutterHelixAudio *audio = CLUTTER_HELIX_AUDIO(media);

  g_return_val_if_fail (CLUTTER_HELIX_IS_AUDIO (audio), -1);

  return audio->priv->duration;
}


static void
clutter_media_init (ClutterMediaInterface *iface)
{
  iface->set_uri            = set_uri;
  iface->get_uri            = get_uri;
  iface->set_playing        = set_playing;
  iface->get_playing        = get_playing;
  iface->set_position       = set_position;
  iface->get_position       = get_position;
  iface->set_volume         = set_volume;
  iface->get_volume         = get_volume;
  iface->can_seek           = can_seek;
  iface->get_buffer_percent = get_buffer_percent;
  iface->get_duration       = get_duration;
}

static void
clutter_helix_audio_dispose (GObject *object)
{
  ClutterHelixAudio        *self;
  ClutterHelixAudioPrivate *priv; 

  self = CLUTTER_HELIX_AUDIO(object); 
  priv = self->priv;

  if (priv->player) 
    {
      put_player(priv->player);
      priv->player = NULL;
    }

  if (priv->tick_timeout_id > 0) 
    {
      g_source_remove (priv->tick_timeout_id);
      priv->tick_timeout_id = 0;
    }

  if (priv->async_queue)
    {
      g_async_queue_unref (priv->async_queue);
      priv->async_queue = NULL;
    }

    G_OBJECT_CLASS (clutter_helix_audio_parent_class)->dispose (object);
}

static void
clutter_helix_audio_finalize (GObject *object)
{
  ClutterHelixAudio        *self;
  ClutterHelixAudioPrivate *priv; 

  self = CLUTTER_HELIX_AUDIO(object); 
  priv = self->priv;

  if (priv->uri)
    g_free (priv->uri);

  deinit_main();

  G_OBJECT_CLASS (clutter_helix_audio_parent_class)->finalize (object);
}

static void
clutter_helix_audio_set_property (GObject      *object, 
					  guint         property_id,
					  const GValue *value, 
					  GParamSpec   *pspec)
{
  ClutterHelixAudio *audio;

  audio = CLUTTER_HELIX_AUDIO(object);

  switch (property_id)
    {
    case PROP_URI:
      clutter_media_set_uri (CLUTTER_MEDIA(audio), 
			     g_value_get_string (value));
      break;
    case PROP_PLAYING:
      clutter_media_set_playing (CLUTTER_MEDIA(audio),
				 g_value_get_boolean (value));
      break;
    case PROP_POSITION:
      clutter_media_set_position (CLUTTER_MEDIA(audio),
				  g_value_get_int (value));
      break;
    case PROP_VOLUME:
      clutter_media_set_volume (CLUTTER_MEDIA(audio),
				g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_helix_audio_get_property (GObject    *object, 
				        guint       property_id,
				        GValue     *value, 
				        GParamSpec *pspec)
{
  ClutterHelixAudio *audio;
  ClutterMedia             *media;

  audio = CLUTTER_HELIX_AUDIO (object);
  media         = CLUTTER_MEDIA (audio);

  switch (property_id)
    {
    case PROP_URI:
      g_value_set_string (value, clutter_media_get_uri (media));
      break;
    case PROP_PLAYING:
      g_value_set_boolean (value, clutter_media_get_playing (media));
      break;
    case PROP_POSITION:
      g_value_set_int (value, clutter_media_get_position (media));
      break;
    case PROP_VOLUME:
      g_value_set_double (value, clutter_media_get_volume (media));
      break;
    case PROP_CAN_SEEK:
      g_value_set_boolean (value, clutter_media_get_can_seek (media));
      break;
    case PROP_BUFFER_PERCENT:
      g_value_set_int (value, clutter_media_get_buffer_percent (media));
      break;
    case PROP_DURATION:
      g_value_set_int (value, clutter_media_get_duration (media));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_helix_audio_class_init (ClutterHelixAudioClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  init_main();
  g_type_class_add_private (klass, sizeof (ClutterHelixAudioPrivate));

  object_class->dispose      = clutter_helix_audio_dispose;
  object_class->finalize     = clutter_helix_audio_finalize;
  object_class->set_property = clutter_helix_audio_set_property;
  object_class->get_property = clutter_helix_audio_get_property;

  /* Interface props */
  g_object_class_override_property (object_class, PROP_URI, "uri");
  g_object_class_override_property (object_class, PROP_PLAYING, "playing");
  g_object_class_override_property (object_class, PROP_POSITION, "position");
  g_object_class_override_property (object_class, PROP_VOLUME, "volume");
  g_object_class_override_property (object_class, PROP_CAN_SEEK, "can-seek");
  g_object_class_override_property (object_class, PROP_DURATION, "duration");
  g_object_class_override_property (object_class, PROP_BUFFER_PERCENT, 
				    "buffer-percent" );
}

static void
on_buffering_cb (unsigned int flags,
		 unsigned short percentage,
		 void *context)
{
  ClutterHelixAudio *audio = (ClutterHelixAudio *)context;

  if (audio && audio->priv)
    audio->priv->buffer_percent = percentage;
  
  g_object_notify (G_OBJECT (audio), "buffer-percent");
}

static void
on_pos_length_cb (unsigned int pos, unsigned int ulLength, void *context)
{
  ClutterHelixAudio *audio = (ClutterHelixAudio *)context;
  ClutterHelixAudioPrivate *priv;

  priv = audio->priv;

  if (!priv->player)
    return;

  /**
   * Determine the duration.
   **/
  priv->duration = ulLength / 1000;

  g_object_notify (G_OBJECT (audio), "duration");
}

static void
on_state_change_cb (unsigned short old_state, unsigned short new_state, void *context)
{
  ClutterHelixAudio *audio = (ClutterHelixAudio *)context;
  ClutterHelixAudioPrivate *priv;

  priv = audio->priv;

  if (!priv->player)
    return;

  priv->state = new_state;
  priv->can_seek  = player_canseek(priv->player);
    
  g_object_notify (G_OBJECT (audio), "can-seek");

  if (old_state != new_state && new_state == PLAYER_STATE_READY) 
    {
      g_object_notify (G_OBJECT (audio), "position");
      g_signal_emit_by_name (CLUTTER_MEDIA(audio), "eos");
    }
}

static gboolean
tick_timeout (ClutterHelixAudio *audio)
{
  ClutterHelixAudioPrivate *priv;

  priv = audio->priv;

  if (!priv->player)
    return FALSE;

  g_object_notify (G_OBJECT (audio), "position");

  return TRUE;
}

static void
clutter_helix_audio_init (ClutterHelixAudio *audio)
{
  ClutterHelixAudioPrivate *priv;

  audio->priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (audio,
                                 CLUTTER_HELIX_TYPE_AUDIO,
                                 ClutterHelixAudioPrivate);
  priv->state = PLAYER_STATE_READY;
  get_player(&priv->player, 
	     on_buffering_cb, 
	     on_pos_length_cb, 
	     on_state_change_cb,
	     NULL, /* on_new_frame_cb */
	     (void *)audio);
  priv->async_queue = g_async_queue_new ();
}

/**
 * clutter_helix_audio_new:
 *
 * Creates a audio texture.
 *
 * Return value: A #ClutterActor implementing an audio player.
 */
ClutterActor*
clutter_helix_audio_new (void)
{
  return g_object_new (CLUTTER_HELIX_TYPE_AUDIO, NULL);
}

