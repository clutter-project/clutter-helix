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

/**
 * SECTION:clutter-helix-video-texture
 * @short_description: Actor for playback of video files.
 *
 * #ClutterHelixVideoTexture is a #ClutterTexture that plays video files.
 */

#include "config.h"

#include "clutter-helix-video-texture.h"
#include "player.h"

#include <glib.h>

struct _ClutterHelixVideoTexturePrivate
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

static gboolean tick_timeout (ClutterHelixVideoTexture *video_texture);

G_DEFINE_TYPE_WITH_CODE (ClutterHelixVideoTexture,
                         clutter_helix_video_texture,
                         CLUTTER_TYPE_TEXTURE,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_MEDIA,
                                                clutter_media_init));

/* Interface implementation */

static void
set_uri (ClutterMedia *media,
	 const char      *uri)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;

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
						 video_texture);
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
  g_object_notify (G_OBJECT (video_texture), "uri");
  g_object_notify (G_OBJECT (video_texture), "can-seek");
  g_object_notify (G_OBJECT (video_texture), "duration");
  g_object_notify (G_OBJECT (video_texture), "position");
}

static const char *
get_uri (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), NULL);

  priv = video_texture->priv;

  return priv->uri;
}

static void
set_playing (ClutterMedia *media,
	     gboolean         playing)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;

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
  
  g_object_notify (G_OBJECT (video_texture), "playing");
  g_object_notify (G_OBJECT (video_texture), "position");
}

static gboolean
get_playing (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), FALSE);

  priv = video_texture->priv;

  if (!priv->player)
    return FALSE;

  return (priv->state == PLAYER_STATE_PLAYING);
}

static void
set_position (ClutterMedia *media,
	      int              position) /* seconds */
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;

  if (!priv->player)
    return;

  player_seek(priv->player, position * 1000);
}

static int
get_position (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 
  gint32 position;

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), -1);

  priv = video_texture->priv;

  if (!priv->player)
    return -1;
  
  position = get_curr_playtime(priv->player);
  return (position / 1000);
}

static void
set_volume (ClutterMedia *media,
	    double           volume)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;
  
  if (!priv->player)
    return;
 
  unsigned short volume_in_u16 = volume * (0xffff);
  player_setvolume(priv->player, volume_in_u16);  
  g_object_notify (G_OBJECT (video_texture), "volume");
}

static double
get_volume (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 
  double volume;

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), 0.0);

  priv = video_texture->priv;
  
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
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), FALSE);
  
  return video_texture->priv->can_seek;
}

static int
get_buffer_percent (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), -1);
  
  return video_texture->priv->buffer_percent;
}

static int
get_duration (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), -1);

  return video_texture->priv->duration;
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
clutter_helix_video_texture_dispose (GObject *object)
{
  ClutterHelixVideoTexture        *self;
  ClutterHelixVideoTexturePrivate *priv; 

  self = CLUTTER_HELIX_VIDEO_TEXTURE(object); 
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

    G_OBJECT_CLASS (clutter_helix_video_texture_parent_class)->dispose (object);
}

static void
clutter_helix_video_texture_finalize (GObject *object)
{
  ClutterHelixVideoTexture        *self;
  ClutterHelixVideoTexturePrivate *priv; 

  self = CLUTTER_HELIX_VIDEO_TEXTURE(object); 
  priv = self->priv;

  if (priv->uri)
    g_free (priv->uri);

  deinit_main();

  G_OBJECT_CLASS (clutter_helix_video_texture_parent_class)->finalize (object);
}

static void
clutter_helix_video_texture_set_property (GObject      *object, 
					  guint         property_id,
					  const GValue *value, 
					  GParamSpec   *pspec)
{
  ClutterHelixVideoTexture *video_texture;

  video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(object);

  switch (property_id)
    {
    case PROP_URI:
      clutter_media_set_uri (CLUTTER_MEDIA(video_texture), 
			     g_value_get_string (value));
      break;
    case PROP_PLAYING:
      clutter_media_set_playing (CLUTTER_MEDIA(video_texture),
				 g_value_get_boolean (value));
      break;
    case PROP_POSITION:
      clutter_media_set_position (CLUTTER_MEDIA(video_texture),
				  g_value_get_int (value));
      break;
    case PROP_VOLUME:
      clutter_media_set_volume (CLUTTER_MEDIA(video_texture),
				g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_helix_video_texture_get_property (GObject    *object, 
				        guint       property_id,
				        GValue     *value, 
				        GParamSpec *pspec)
{
  ClutterHelixVideoTexture *video_texture;
  ClutterMedia             *media;

  video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (object);
  media         = CLUTTER_MEDIA (video_texture);

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
clutter_helix_video_texture_class_init (ClutterHelixVideoTextureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  init_main();
  g_type_class_add_private (klass, sizeof (ClutterHelixVideoTexturePrivate));

  object_class->dispose      = clutter_helix_video_texture_dispose;
  object_class->finalize     = clutter_helix_video_texture_finalize;
  object_class->set_property = clutter_helix_video_texture_set_property;
  object_class->get_property = clutter_helix_video_texture_get_property;

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
  ClutterHelixVideoTexture *video_texture = (ClutterHelixVideoTexture *)context;

  if (video_texture && video_texture->priv)
    video_texture->priv->buffer_percent = percentage;
  
  g_object_notify (G_OBJECT (video_texture), "buffer-percent");
}

static void
on_pos_length_cb (unsigned int pos, unsigned int ulLength, void *context)
{
  ClutterHelixVideoTexture *video_texture = (ClutterHelixVideoTexture *)context;
  ClutterHelixVideoTexturePrivate *priv;

  priv = video_texture->priv;

  if (!priv->player)
    return;

  /**
   * Determine the duration.
   **/
  priv->duration = ulLength / 1000;

  g_object_notify (G_OBJECT (video_texture), "duration");
}

static void
on_state_change_cb (unsigned short old_state, unsigned short new_state, void *context)
{
  ClutterHelixVideoTexture *video_texture = (ClutterHelixVideoTexture *)context;
  ClutterHelixVideoTexturePrivate *priv;

  priv = video_texture->priv;

  if (!priv->player)
    return;

  priv->state = new_state;
  priv->can_seek  = player_canseek(priv->player);
    
  g_object_notify (G_OBJECT (video_texture), "can-seek");

  if (old_state != new_state && new_state == PLAYER_STATE_READY) 
    {
      g_object_notify (G_OBJECT (video_texture), "position");
      g_signal_emit_by_name (CLUTTER_MEDIA(video_texture), "eos");
    }
}

static gboolean
clutter_helix_video_render_idle_func (gpointer data)
{
  GError *error;
  unsigned char *buffer;
  ClutterHelixVideoTexture *video_texture = (ClutterHelixVideoTexture *)data;
  ClutterHelixVideoTexturePrivate *priv;

  priv = video_texture->priv;

  buffer = g_async_queue_try_pop (priv->async_queue);
  if (buffer == NULL) 
    {
      return FALSE;
    }

  error = NULL;
  if (!clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (video_texture),
					  buffer,
					  FALSE,
					  priv->width,
					  priv->height,
					  3 * priv->width ,
					  3,
					  CLUTTER_TEXTURE_RGB_FLAG_BGR,
					  &error))
    g_error ("clutter_texture_set_from_rgb_data: %s", error->message);

    return FALSE;
}

static void on_new_frame_cb(unsigned char *p, unsigned int size, PlayerImgInfo *Info, void *context)
{
  ClutterHelixVideoTexture *video_texture = (ClutterHelixVideoTexture *)context;
  ClutterHelixVideoTexturePrivate *priv;

  priv = video_texture->priv;
  
  if (!priv->player)
    return;

  g_async_queue_push (priv->async_queue, p);
  priv->width = Info->cx;
  priv->height = Info->cy;
  clutter_threads_add_idle_full (G_PRIORITY_HIGH_IDLE,
				 clutter_helix_video_render_idle_func,
				 video_texture,
				 NULL);
}

static gboolean
tick_timeout (ClutterHelixVideoTexture *video_texture)
{
  ClutterHelixVideoTexturePrivate *priv;

  priv = video_texture->priv;

  if (!priv->player)
    return FALSE;

  g_object_notify (G_OBJECT (video_texture), "position");

  return TRUE;
}

static void
clutter_helix_video_texture_init (ClutterHelixVideoTexture *video_texture)
{
  ClutterHelixVideoTexturePrivate *priv;

  video_texture->priv  = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (video_texture,
                                 CLUTTER_HELIX_TYPE_VIDEO_TEXTURE,
                                 ClutterHelixVideoTexturePrivate);
  priv->state = PLAYER_STATE_READY;
  get_player(&priv->player, on_buffering_cb, on_pos_length_cb, on_state_change_cb, on_new_frame_cb,(void *)video_texture);

  priv->async_queue = g_async_queue_new ();
}

/**
 * clutter_helix_video_texture_new:
 *
 * Creates a video texture.
 *
 * Return value: A #ClutterActor implementing a displaying a video texture.
 */
ClutterActor*
clutter_helix_video_texture_new (void)
{
  return g_object_new (CLUTTER_HELIX_TYPE_VIDEO_TEXTURE,
                       "tiled", FALSE,
                       NULL);
}

