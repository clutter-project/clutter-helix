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
 * Copyright 2008-2009 Intel Corp.
 *
 * Referenced/Copied a lot from clutter-gst-video-sink.c,
 * Authored by Jonathan Matthew  <jonathan@kaolin.wh9.net>,
 *             Chris Lord        <chris@openedhand.com>
 *             Damien Lespiau    <damien.lespiau@intel.com>
 *     
 * Copyright (C) 2007,2008 OpenedHand
 * Copyright (C) 2009 Intel Corporation
 *
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
#include <string.h>
#include <glib.h>
#include <stdlib.h>

#include "clutter-helix-video-texture.h"
#include "clutter-helix-shaders.h"
#include "player.h"



enum
{
  PROP_0,
  /* ClutterMedia proprs */
  PROP_URI,
  PROP_PLAYING,
  PROP_PROGRESS,
  PROP_AUDIO_VOLUME,
  PROP_CAN_SEEK,
  PROP_BUFFER_FILL,
  PROP_DURATION
};

typedef enum _ClutterHelixVideoFormat
{
  CLUTTER_HELIX_NOFORMAT,
  CLUTTER_HELIX_RGB32,
  CLUTTER_HELIX_I420,
} ClutterHelixVideoFormat;

/* GL_ARB_fragment_program */
typedef void (APIENTRYP GLGENPROGRAMSPROC)(GLsizei n, GLuint *programs);
typedef void (APIENTRYP GLBINDPROGRAMPROC)(GLenum target, GLint program);
typedef void (APIENTRYP GLPROGRAMSTRINGPROC)(GLenum target, GLenum format,
              GLsizei len, const void *string);

typedef struct _ClutterHelixSymbols
{
  /* GL_ARB_fragment_program */
  GLGENPROGRAMSPROC   glGenProgramsARB;
  GLBINDPROGRAMPROC   glBindProgramARB;
  GLPROGRAMSTRINGPROC glProgramStringARB;
} ClutterHelixSymbols;

/*
 * features: what does the underlaying video card supports ?
 */
typedef enum _ClutterHelixFeatures
{
  CLUTTER_HELIX_FP             = 0x1, /* fragment programs (ARB fp1.0) */
  CLUTTER_HELIX_GLSL           = 0x2, /* GLSL */
  CLUTTER_HELIX_MULTI_TEXTURE  = 0x4, /* multi-texturing */
} ClutterHelixFeatures;

 
/*
 * renderer: abstracts a backend to render a frame.
 */
typedef void (ClutterHelixRendererPaint) (ClutterHelixVideoTexture *, void *dummy);
typedef void (ClutterHelixRendererPostPaint) (ClutterHelixVideoTexture *, void *dummy);

typedef struct _ClutterHelixRenderer
{
  const char                *name;     /* user friendly name */
  ClutterHelixVideoFormat   format;   /* the format handled by this renderer */
  int                       flags;    /* ClutterHelixFeatures ORed flags */

  void (*init)       (ClutterHelixVideoTexture *video_texture);
  void (*deinit)     (ClutterHelixVideoTexture *video_texture);
  void (*upload)     (ClutterHelixVideoTexture *video_texture,
                      guchar                   *buffer);
} ClutterHelixRenderer;

typedef enum _ClutterHelixRendererState
{
  CLUTTER_HELIX_RENDERER_STOPPED,
  CLUTTER_HELIX_RENDERER_RUNNING,
  CLUTTER_HELIX_RENDERER_NEED_GC,
} ClutterHelixRendererState;

struct _ClutterHelixVideoTexturePrivate
{
  void                      *player;
  char                      *uri;
  gboolean                   can_seek;
  int                        buffer_percent;
  int                        duration;
  guint                      tick_timeout_id;
  EHXPlayerState             state;
  unsigned int               x;
  unsigned int               y;
  unsigned int               width;
  unsigned int               height;
  gint                       cid;
  gboolean                   shaders_init;
  CoglHandle                 u_tex;
  CoglHandle                 v_tex;
  CoglHandle                 program;
  CoglHandle                 shader;
  gboolean                   use_shaders;
  ClutterHelixSymbols        syms;          /* extra OpenGL functions */
  GLuint                     fp;
  GSList                    *renderers;
  ClutterHelixRendererState  renderer_state;
  ClutterHelixRenderer      *renderer;
  guint                      idle_id;
  GMutex                    *id_lock;
  guchar                    *buffer;
};


static void
_renderer_connect_signals (ClutterHelixVideoTexture     *video_texture,
                           ClutterHelixRendererPaint     paint_func,
                           ClutterHelixRendererPostPaint post_paint_func)
{
  gulong handler_id;
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;

  handler_id =
    g_signal_connect (video_texture, "paint", G_CALLBACK (paint_func), NULL);

  handler_id = g_signal_connect_after (video_texture,
      "paint",
      G_CALLBACK (post_paint_func),
      NULL);
}

static gchar *dummy_shader = \
     FRAGMENT_SHADER_VARS
     "void main () {"
     "}";

static gchar *yv12_to_rgba_shader = \
     FRAGMENT_SHADER_VARS
	   "uniform sampler2D ytex;"
	   "uniform sampler2D utex;"
	   "uniform sampler2D vtex;"
	   "void main () {"
	   "  vec2 coord = vec2(" TEX_COORD ");"
	   "  float y = 1.1640625 * (texture2D (ytex, coord).g - 0.0625);"
	   "  float u = texture2D (utex, coord).g - 0.5;"
	   "  float v = texture2D (vtex, coord).g - 0.5;"
	   "  vec4 color;"
	   "  color.r = y + 1.59765625 * v;"
	   "  color.g = y - 0.390625 * u - 0.8125 * v;"
	   "  color.b = y + 2.015625 * u;"
	   "  color.a = 1.0;"
	   "  gl_FragColor = color;"
	   FRAGMENT_SHADER_END
	   "}";

/* some renderers don't need all the ClutterHelixRenderer vtable */
static void
clutter_helix_dummy_init (ClutterHelixVideoTexture *v)
{
}

static void
clutter_helix_dummy_deinit (ClutterHelixVideoTexture *v)
{
}

/*
 * RGBA / BGRA 8888
 */

static void
clutter_helix_rgb32_upload (ClutterHelixVideoTexture *video_texture,
                            guchar                    *buffer)
{
  ClutterHelixVideoTexturePrivate *priv= video_texture->priv;

  clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (video_texture),
      buffer,
      TRUE,
      priv->width,
      priv->height,
      (4 * priv->width),
      4,
      CLUTTER_TEXTURE_RGB_FLAG_BGR,
      NULL);
}

static ClutterHelixRenderer rgb32_renderer =
{
  "RGB 32",
  CLUTTER_HELIX_RGB32,
  0,
  clutter_helix_dummy_init,
  clutter_helix_dummy_deinit,
  clutter_helix_rgb32_upload,
};

static void
clutter_helix_video_sink_set_glsl_shader (ClutterHelixVideoTexture *video_texture,
                                          const gchar              *shader_src)
{
  ClutterHelixVideoTexturePrivate *priv = video_texture->priv;

  if (video_texture)
    clutter_actor_set_shader (CLUTTER_ACTOR (video_texture), NULL);

  if (priv->program)
    {
      cogl_program_unref (priv->program);
      priv->program = NULL;
    }

  if (priv->shader)
    {
      cogl_shader_unref (priv->shader);
      priv->shader = NULL;
    }

  if (shader_src)
    {
      ClutterShader *shader;

      /* Set a dummy shader so we don't interfere with the shader stack */
      shader = clutter_shader_new ();
      clutter_shader_set_fragment_source (shader, dummy_shader, -1);
      clutter_actor_set_shader (CLUTTER_ACTOR (video_texture), shader);
      g_object_unref (shader);

      /* Create shader through COGL - necessary as we need to be able to set
      * integer uniform variables for multi-texturing.
      */

      priv->shader = cogl_create_shader (COGL_SHADER_TYPE_FRAGMENT);
      cogl_shader_source (priv->shader, shader_src);
      cogl_shader_compile (priv->shader);

      priv->program = cogl_create_program ();
      cogl_program_attach_shader (priv->program, priv->shader);
      cogl_program_link (priv->program);
    }
}

static void
clutter_helix_yv12_glsl_paint (ClutterHelixVideoTexture *video_texture,
                               void                     *dummy)
{
  ClutterHelixVideoTexturePrivate *priv = video_texture->priv;
  CoglHandle material;

  material = clutter_texture_get_cogl_material (CLUTTER_TEXTURE (video_texture));

  /* bind the shader */
  cogl_program_use (priv->program);

  /* Bind the U and V textures in layers 1 and 2 */
  if (priv->u_tex)
    cogl_material_set_layer (material, 1, priv->u_tex);
  if (priv->v_tex)
    cogl_material_set_layer (material, 2, priv->v_tex);
}

static void
clutter_helix_yv12_glsl_post_paint (ClutterHelixVideoTexture *video_texture,
                                     void                    *dummy)
{
  CoglHandle material;

  /* Remove the extra layers */
  material = clutter_texture_get_cogl_material (CLUTTER_TEXTURE (video_texture));
  cogl_material_remove_layer (material, 1);
  cogl_material_remove_layer (material, 2);

  /* disable the shader */
  cogl_program_use (COGL_INVALID_HANDLE);
}

/*
 * I420
 *
 * 8 bit Y plane followed by 8 bit 2x2 subsampled U and V planes.
 * Basically the same as YV12, but with the 2 chroma planes switched.
 */

static void
clutter_helix_i420_glsl_init (ClutterHelixVideoTexture *video_texture)
{
  ClutterHelixVideoTexturePrivate *priv = video_texture->priv;
  GLint location;

  clutter_helix_video_sink_set_glsl_shader (video_texture, yv12_to_rgba_shader);

  cogl_program_use (priv->program);
  location = cogl_program_get_uniform_location (priv->program, "ytex");
  cogl_program_uniform_1i (location, 0);
  location = cogl_program_get_uniform_location (priv->program, "vtex");
  cogl_program_uniform_1i (location, 1);
  location = cogl_program_get_uniform_location (priv->program, "utex");
  cogl_program_uniform_1i (location, 2);
  cogl_program_use (COGL_INVALID_HANDLE);

  _renderer_connect_signals (video_texture,
                             clutter_helix_yv12_glsl_paint,
                             clutter_helix_yv12_glsl_post_paint);
}

static void
clutter_helix_yv12_glsl_deinit (ClutterHelixVideoTexture *video_texture)
{
  clutter_helix_video_sink_set_glsl_shader (video_texture, NULL);
}

static void
clutter_helix_yv12_upload (ClutterHelixVideoTexture *video_texture,
                           guchar                    *buffer)
{
  ClutterHelixVideoTexturePrivate *priv = video_texture->priv;
  CoglHandle y_tex = cogl_texture_new_from_data (priv->width,
      priv->height,
      COGL_TEXTURE_NO_SLICING,
      COGL_PIXEL_FORMAT_G_8,
      COGL_PIXEL_FORMAT_G_8,
      priv->width,
      buffer);

  clutter_texture_set_cogl_texture (CLUTTER_TEXTURE (video_texture), y_tex);
  cogl_texture_unref (y_tex);

  if (priv->u_tex)
    cogl_texture_unref (priv->u_tex);

  if (priv->v_tex)
    cogl_texture_unref (priv->v_tex);

  priv->v_tex = cogl_texture_new_from_data (priv->width / 2,
      priv->height / 2,
      COGL_TEXTURE_NO_SLICING,
      COGL_PIXEL_FORMAT_G_8,
      COGL_PIXEL_FORMAT_G_8,
      priv->width / 2,
      buffer +
      (priv->width * priv->height));
  priv->u_tex = cogl_texture_new_from_data (priv->width / 2,
      priv->height / 2,
      COGL_TEXTURE_NO_SLICING,
      COGL_PIXEL_FORMAT_G_8,
      COGL_PIXEL_FORMAT_G_8,
      priv->width / 2,
      buffer
      + (priv->width * priv->height)
      + (priv->width / 2 * priv->height / 2));
}

static ClutterHelixRenderer i420_glsl_renderer =
{
  "I420 glsl",
  CLUTTER_HELIX_I420,
  CLUTTER_HELIX_GLSL | CLUTTER_HELIX_MULTI_TEXTURE,
  clutter_helix_i420_glsl_init,
  clutter_helix_yv12_glsl_deinit,
  clutter_helix_yv12_upload,
};

/*
 * I420 (fragment program version)
 *
 * 8 bit Y plane followed by 8 bit 2x2 subsampled U and V planes.
 * Basically the same as YV12, but with the 2 chroma planes switched.
 */

#ifdef CLUTTER_COGL_HAS_GL
static void
clutter_helix_video_sink_set_fp_shader (ClutterHelixVideoTexture *video_texture,
                                        const gchar              *shader_src,
                                        const int                 size)
{
  ClutterHelixVideoTexturePrivate *priv = video_texture->priv;

  /* FIXME: implement freeing the shader */
  if (!shader_src)
    return;

  priv->syms.glGenProgramsARB (1, &priv->fp);
  priv->syms.glBindProgramARB (GL_FRAGMENT_PROGRAM_ARB, priv->fp);
  priv->syms.glProgramStringARB (GL_FRAGMENT_PROGRAM_ARB,
                                 GL_PROGRAM_FORMAT_ASCII_ARB,
                                 size,
                                 (const GLbyte *)shader_src);
}

static void
clutter_helix_yv12_fp_paint (ClutterHelixVideoTexture *video_texture,
                             void                     *dummy)
{
  ClutterHelixVideoTexturePrivate *priv = video_texture->priv;
  CoglHandle material;

  material = clutter_texture_get_cogl_material (CLUTTER_TEXTURE (video_texture));

  /* Bind the U and V textures in layers 1 and 2 */
  if (priv->u_tex)
    cogl_material_set_layer (material, 1, priv->u_tex);
  if (priv->v_tex)
    cogl_material_set_layer (material, 2, priv->v_tex);

  /* Cogl doesn't support changing OpenGL state to modify how Cogl primitives
   * work, but it also doesn't support ARBfp which we currently depend on. For
   * now we at least ask Cogl to flush any batched primitives so we avoid
   * binding our shader across the wrong geometry, but there is a risk that
   * Cogl may start to use ARBfp internally which will conflict with us. */
  cogl_flush ();

  /* bind the shader */
  glEnable (GL_FRAGMENT_PROGRAM_ARB);
  priv->syms.glBindProgramARB (GL_FRAGMENT_PROGRAM_ARB, priv->fp);
}

static void
clutter_helix_yv12_fp_post_paint (ClutterHelixVideoTexture        *video_texture,
                                  void                            *dummy)
{
  CoglHandle material;

  /* Cogl doesn't support changing OpenGL state to modify how Cogl primitives
   * work, but it also doesn't support ARBfp which we currently depend on. For
   * now we at least ask Cogl to flush any batched primitives so we avoid
   * binding our shader across the wrong geometry, but there is a risk that
   * Cogl may start to use ARBfp internally which will conflict with us. */
  cogl_flush ();

  /* Remove the extra layers */
  material = clutter_texture_get_cogl_material (CLUTTER_TEXTURE (video_texture));
  cogl_material_remove_layer (material, 1);
  cogl_material_remove_layer (material, 2);

  /* Disable the shader */
  glDisable (GL_FRAGMENT_PROGRAM_ARB);
}

#define I420_FP_SZ    526

static const char *I420_fp[] =
{ 
  "!!ARBfp1.0\n",
  "PARAM c[2] = { { 1, 1.1640625, 0.0625, 2.015625 },\n",
  "{ 0.5, 0.390625, 0.8125, 1.5976562 } };\n",
  "TEMP R0;\n",
  "TEX R0.y, fragment.texcoord[0], texture[0], 2D;\n",
  "ADD R0.x, R0.y, -c[0].z;\n",
  "TEX R0.y, fragment.texcoord[0], texture[2], 2D;\n",
  "ADD R0.z, R0.y, -c[1].x;\n",
  "MUL R0.x, R0, c[0].y;\n",
  "MAD result.color.z, R0, c[0].w, R0.x;\n",
  "TEX R0.y, fragment.texcoord[0], texture[1], 2D;\n",
  "ADD R0.y, R0, -c[1].x;\n",
  "MAD R0.z, -R0, c[1].y, R0.x;\n",
  "MAD result.color.y, -R0, c[1].z, R0.z;\n",
  "MAD result.color.x, R0.y, c[1].w, R0;\n",
  "MOV result.color.w, c[0].x;\n",
  "END\n",
  NULL
};

/*
 * Small helpers
 */

static void
_string_array_to_char_array (char       *dst,
                             const char *src[])
{
  int i, n;

  for (i = 0; src[i]; i++)
    {
      n = strlen (src[i]);
      memcpy (dst, src[i], n);
      dst += n;
    }
  *dst = '\0';
}

static void
clutter_helix_i420_fp_init (ClutterHelixVideoTexture *video_texture)
{
  gchar *shader;

  shader = g_malloc (I420_FP_SZ + 1);
  _string_array_to_char_array (shader, I420_fp);

  /* the size given to glProgramStringARB is without the trailing '\0', which
   * is precisely I420_FP_SZ */
  clutter_helix_video_sink_set_fp_shader (video_texture, shader, I420_FP_SZ);
  g_free (shader);

  _renderer_connect_signals (video_texture,
                             clutter_helix_yv12_fp_paint,
                             clutter_helix_yv12_fp_post_paint);
}

static void
clutter_helix_yv12_fp_deinit (ClutterHelixVideoTexture *video_texture)
{
  clutter_helix_video_sink_set_fp_shader (video_texture, NULL, 0);
}



static ClutterHelixRenderer i420_fp_renderer =
{
  "I420 fp",
  CLUTTER_HELIX_I420,
  CLUTTER_HELIX_FP | CLUTTER_HELIX_MULTI_TEXTURE,
  clutter_helix_i420_fp_init,
  clutter_helix_yv12_fp_deinit,
  clutter_helix_yv12_upload,
};
#endif



#define TICK_TIMEOUT 0.5

static void clutter_media_init (ClutterMediaIface *iface);

static gboolean tick_timeout (ClutterHelixVideoTexture *video_texture);


G_DEFINE_TYPE_WITH_CODE (ClutterHelixVideoTexture,
                         clutter_helix_video_texture,
                         CLUTTER_TYPE_TEXTURE,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_MEDIA,
                                                clutter_media_init));

static void on_buffering_cb (unsigned int   flags,
                             unsigned short percentage,
			                       void          *context);

static void on_pos_length_cb (unsigned int pos,
                              unsigned int ulLength, 
                              void        *context);

static void on_state_change_cb (unsigned short old_state, 
                                unsigned short new_state, 
                                void          *context);

static void on_new_frame_cb (unsigned char *p, 
			                       unsigned int   size, 
			                       PlayerImgInfo *Info, 
			                       void          *context);

static void on_error_cb (unsigned long code,
                         char         *message,
                         void         *context);

/* Interface implementation */
static void
set_uri (ClutterMedia    *media,
         const char      *uri)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;

  if (!priv->player)
    return;

  if (priv->uri)
    {
      player_stop (priv->player);
      g_free (priv->uri);
    }

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
      player_openurl (priv->player, priv->uri);
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
  g_object_notify (G_OBJECT (video_texture), "progress");
}

static const char *
get_uri (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), NULL);

  priv = video_texture->priv;

  return priv->uri;
}

static gboolean
get_playing (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), FALSE);

  priv = video_texture->priv;

  if (!priv->player)
    return FALSE;

  return (priv->state == PLAYER_STATE_PLAYING);
}

static void
set_playing (ClutterMedia *media,
	           gboolean      playing)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;

  if (!priv->player)
    return;
        
  if (priv->uri) 
    {
      if (playing && !get_playing (media))
        {
          player_begin (priv->player);
        }
      else if (!playing && get_playing (media))
        {
          priv->renderer_state = CLUTTER_HELIX_RENDERER_STOPPED;
          player_pause(priv->player);
        }
    } 
  else 
    {
      if (playing)
        g_warning ("Tried to play, but no URI is loaded.");
    }
  
  g_object_notify (G_OBJECT (video_texture), "playing");
  g_object_notify (G_OBJECT (video_texture), "progress");
}
static void
set_position (ClutterMedia *media,
	            gdouble       position) /* seconds */
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;

  if (!priv->player)
    return;

  player_seek (priv->player, position * 1000);
}

static void
set_progress (ClutterMedia *media,
	            gdouble       progress)
{
  if (progress >= 0 && progress <= 1.0) {
    ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (media);
    ClutterHelixVideoTexturePrivate *priv; 

    g_return_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture));

    priv = video_texture->priv;
    
    gdouble position = 0;
    if (priv->duration > 0)
      {
        position = (gint)(progress * (gdouble)priv->duration);
      }
    set_position (media, position);
    g_object_notify (G_OBJECT (video_texture), "progress");
  }
}

static int
get_position (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (media);
  ClutterHelixVideoTexturePrivate *priv; 
  gint32 position;

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), -1);

  priv = video_texture->priv;

  if (!priv->player)
    return -1;
  
  position = get_curr_playtime (priv->player);
  return (position / 1000);
}

static gdouble get_progress (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 
  gint32 position;
  gdouble progress = 0.0;

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), -1);

  priv = video_texture->priv;

  if (!priv->player)
    return 0;
  
  position = get_position (media);
  if (priv->duration > 0)
    progress = (gdouble)position/(gdouble)priv->duration;

  return progress;
}

static void
set_volume (ClutterMedia *media,
	          double        volume)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (media);
  ClutterHelixVideoTexturePrivate *priv; 
  unsigned short volume_in_u16;

  g_return_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture));

  priv = video_texture->priv;
  
  if (!priv->player)
    return;

  if (volume >= 1.0)
    volume_in_u16 = 100;
  else if (volume <= 0.0)
    volume_in_u16 = 0;
  else
    volume_in_u16 = (int)(volume * (100.0));

  player_setvolume (priv->player, volume_in_u16);  
  g_object_notify (G_OBJECT (video_texture), "audio-volume");
}

static double
get_volume (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE(media);
  ClutterHelixVideoTexturePrivate *priv; 

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), 0.0);

  priv = video_texture->priv;
  
  if (!priv->player)
    return 0.0;

  int ret;
  ret = player_getvolume (priv->player);
  if (ret < 0)
    ret = 0;
  
  return (double)((double)ret/(double)100);
}

static gboolean
can_seek (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (media);

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), FALSE);
  
  return video_texture->priv->can_seek;
}

static int
get_buffer_percent (ClutterMedia *media)
{
  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (media);

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), -1);
  
  return video_texture->priv->buffer_percent;
}

static int
get_duration(ClutterMedia *media)
{

  ClutterHelixVideoTexture *video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (media);

  g_return_val_if_fail (CLUTTER_HELIX_IS_VIDEO_TEXTURE (video_texture), -1);
  
  return video_texture->priv->duration;
}

static void
clutter_media_init (ClutterMediaIface *iface)
{

}

static void
clutter_helix_video_texture_dispose (GObject *object)
{
  ClutterHelixVideoTexture        *self;
  ClutterHelixVideoTexturePrivate *priv; 

  self = CLUTTER_HELIX_VIDEO_TEXTURE (object); 
  priv = self->priv;

  if (priv->renderer_state == CLUTTER_HELIX_RENDERER_RUNNING ||
      priv->renderer_state == CLUTTER_HELIX_RENDERER_NEED_GC)
    {
      priv->renderer->deinit (self);
      priv->renderer_state = CLUTTER_HELIX_RENDERER_STOPPED;
    }

  if (priv->idle_id > 0)
    {
      g_source_remove (priv->idle_id);
      priv->idle_id = 0;
    }
  
  if (priv->id_lock)
    {
      g_mutex_free (priv->id_lock);
      priv->id_lock = NULL;
    }


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
    G_OBJECT_CLASS (clutter_helix_video_texture_parent_class)->dispose (object);
}

static void
clutter_helix_video_texture_finalize (GObject *object)
{
  ClutterHelixVideoTexture        *self;
  ClutterHelixVideoTexturePrivate *priv; 

  self = CLUTTER_HELIX_VIDEO_TEXTURE (object); 
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

  video_texture = CLUTTER_HELIX_VIDEO_TEXTURE (object);

  switch (property_id)
    {
    case PROP_URI:
      set_uri (CLUTTER_MEDIA (video_texture), g_value_get_string (value));
      break;
    case PROP_PLAYING:
      set_playing (CLUTTER_MEDIA (video_texture), g_value_get_boolean (value));
      break;
    case PROP_PROGRESS:
      set_progress (CLUTTER_MEDIA (video_texture), g_value_get_double (value));
      break;
    case PROP_AUDIO_VOLUME:
      set_volume (CLUTTER_MEDIA (video_texture), g_value_get_double (value));
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
      g_value_set_string (value, get_uri (media));
      break;
    case PROP_PLAYING:
      g_value_set_boolean (value, get_playing (media));
      break;
    case PROP_PROGRESS:
      g_value_set_double (value, get_progress (media));
      break;
    case PROP_AUDIO_VOLUME:
      g_value_set_double (value, get_volume (media));
      break;
    case PROP_CAN_SEEK:
      g_value_set_boolean (value, can_seek (media));
      break;
    case PROP_BUFFER_FILL:
      g_value_set_int (value, (gdouble)get_buffer_percent (media)/(gdouble)100);
      break;
    case PROP_DURATION:
      g_value_set_double (value, get_duration (media));
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
  g_object_class_override_property (object_class, PROP_PROGRESS, "progress");
  g_object_class_override_property (object_class, PROP_AUDIO_VOLUME, "audio-volume");
  g_object_class_override_property (object_class, PROP_CAN_SEEK, "can-seek");
  g_object_class_override_property (object_class, PROP_DURATION, "duration");
  g_object_class_override_property (object_class, PROP_BUFFER_FILL, "buffer-fill" );
}

static void
on_buffering_cb (unsigned int   flags,
		             unsigned short percentage,
		             void          *context)
{
  ClutterHelixVideoTexture *video_texture = (ClutterHelixVideoTexture *)context;

  if (video_texture && video_texture->priv)
    video_texture->priv->buffer_percent = percentage;
  
  g_object_notify (G_OBJECT (video_texture), "buffer-fill");
}

static gboolean
emit_eos_idle_func(gpointer data)
{
  if (!data)
    return FALSE;

  ClutterHelixVideoTexture *vtexture = (ClutterHelixVideoTexture *)data;
  g_signal_emit_by_name (CLUTTER_MEDIA(vtexture), "eos");
  return FALSE;
}

/* May get called with a mutex held in helix */
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
  if (pos >= ulLength)
    clutter_threads_add_idle_full (G_PRIORITY_HIGH_IDLE,
				                           emit_eos_idle_func,
				                           video_texture,
				                           NULL);
}

/* Probably gets called with a mutex held in helix */
static void
on_state_change_cb (unsigned short old_state,
                    unsigned short new_state,
                    void          *context)
{
  ClutterHelixVideoTexture *video_texture = (ClutterHelixVideoTexture *)context;
  ClutterHelixVideoTexturePrivate *priv;

  priv = video_texture->priv;

  if (!priv->player)
    return;

  priv->state = new_state;
  priv->can_seek  = player_canseek (priv->player);

  g_object_notify (G_OBJECT (video_texture), "can-seek");
}

static ClutterHelixRenderer *
clutter_helix_find_renderer_by_format (ClutterHelixVideoTexture *video_texture,
                                       ClutterHelixVideoFormat   format)
{
  ClutterHelixVideoTexturePrivate *priv = video_texture->priv;
  ClutterHelixRenderer *renderer = NULL;
  GSList *element;

  for (element = priv->renderers; element; element = g_slist_next(element))
    {
      ClutterHelixRenderer *candidate = (ClutterHelixRenderer *)element->data;

      if (candidate->format == format)
        {
          renderer = candidate;
          break;
        }
    }

  return renderer;
}

static gboolean
clutter_helix_video_render_idle_func (gpointer data)
{
  GError *error;
  unsigned char *buffer;
  gint cid;
  ClutterHelixVideoTexture *video_texture = (ClutterHelixVideoTexture *)data;
  ClutterHelixVideoTexturePrivate *priv;

  priv = video_texture->priv;

  g_mutex_lock (priv->id_lock);
  buffer = priv->buffer;
  priv->buffer = NULL;
  if (buffer == NULL) 
    {
      priv->idle_id = 0;
      g_mutex_unlock (priv->id_lock);
      return FALSE;
    }
  g_mutex_unlock (priv->id_lock);

  error = NULL;
  if (priv->renderer == NULL) 
    {
      ClutterHelixVideoFormat format = CLUTTER_HELIX_NOFORMAT;
      cid = priv->cid;
      if (cid == CID_ARGB32)
        {
          format = CLUTTER_HELIX_RGB32;
        }
      else if (cid == CID_I420)
        {
          format = CLUTTER_HELIX_I420;
        }
      else if (cid == CID_LIBVA)
        {
          /* TODO: Add implementation to perform libva blitting */
        }
      else {
          g_warning ("Unsupported colorspace id:%d", cid);
        }

      if (format == CLUTTER_HELIX_NOFORMAT)
        {
          g_mutex_lock (priv->id_lock);
          priv->idle_id = 0;
          g_mutex_unlock (priv->id_lock);
          free (buffer);
          return FALSE;
        }

      priv->renderer =  clutter_helix_find_renderer_by_format (video_texture,
                                                               format);

      if (priv->renderer == NULL)
        {
          g_warning ("No renderer for format:%d\n", format);
          g_mutex_lock (priv->id_lock);
          priv->idle_id = 0;
          g_mutex_unlock (priv->id_lock);
          free (buffer);
          return FALSE;
        }
    }

  /* The initialization / free functions of the renderers have to be called in
   * the clutter thread (OpenGL context) */
  if (G_UNLIKELY (priv->renderer_state == CLUTTER_HELIX_RENDERER_NEED_GC))
    {
      priv->renderer->deinit (video_texture);
      priv->renderer_state = CLUTTER_HELIX_RENDERER_STOPPED;
    }
  if (G_UNLIKELY (priv->renderer_state == CLUTTER_HELIX_RENDERER_STOPPED))
    {
      priv->renderer->init (video_texture);
      priv->renderer_state = CLUTTER_HELIX_RENDERER_RUNNING;
    }

  g_mutex_lock (priv->id_lock);
  priv->idle_id = 0;
  g_mutex_unlock (priv->id_lock);
  priv->renderer->upload (video_texture, buffer);
  free (buffer);

  return FALSE;
}

static void on_new_frame_cb(unsigned char *p,
                            unsigned int   size,
                            PlayerImgInfo *Info,
                            void          *context)
{
  ClutterHelixVideoTexture *video_texture = (ClutterHelixVideoTexture *)context;
  ClutterHelixVideoTexturePrivate *priv;

  priv = video_texture->priv;
  
  if (!priv->player)
    return;

  g_mutex_lock (priv->id_lock);
  if (priv->idle_id ==0) 
    {
      priv->buffer = p; 
      priv->width = Info->cx;
      priv->height = Info->cy;
      priv->cid = Info->cid;
      priv->idle_id = clutter_threads_add_idle_full (G_PRIORITY_HIGH_IDLE,
                                               clutter_helix_video_render_idle_func,
                                                     video_texture,
                                                     NULL);
      } else {
        free (p);
      }
  g_mutex_unlock (priv->id_lock);
}

static void 
on_error_cb (unsigned long code,
             char  *message,
             void         *context)
{
  GError *error;
  ClutterHelixVideoTexture *video_texture = (ClutterHelixVideoTexture *)context;

  error = g_error_new (g_quark_from_string ("clutter-helix"),
                       (int) code,
                       (const gchar *)message);
  g_signal_emit_by_name (CLUTTER_MEDIA(video_texture), "error", error);

  g_error_free (error);
}

static gboolean
tick_timeout (ClutterHelixVideoTexture *video_texture)
{
  ClutterHelixVideoTexturePrivate *priv;

  priv = video_texture->priv;

  if (!priv->player)
    return FALSE;

  g_object_notify (G_OBJECT (video_texture), "progress");

  return TRUE;
}

static GSList *
clutter_helix_build_renderers_list (ClutterHelixSymbols *syms)
{
  GSList             *list = NULL;
  const gchar        *gl_extensions;
  GLint               nb_texture_units = 0;
  gint                features = 0;
  gint                i;
  /* The order of the list of renderers is important. They will be prepended
   * to a GSList and we'll iterate over that list to choose the first matching
   * renderer. Thus if you want to use the fp renderer over the glsl one, the
   * fp renderer has to be put after the glsl one in this array */
  ClutterHelixRenderer *renderers[] =
  {
    &rgb32_renderer,
    &i420_glsl_renderer,
#ifdef CLUTTER_COGL_HAS_GL
    &i420_fp_renderer,
#endif
    NULL
  };

  /* get the features */
  gl_extensions = (const gchar*) glGetString (GL_EXTENSIONS);

  glGetIntegerv (CGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &nb_texture_units);

  if (nb_texture_units >= 3)
    features |= CLUTTER_HELIX_MULTI_TEXTURE;

#ifdef CLUTTER_COGL_HAS_GL
  if (cogl_check_extension ("GL_ARB_fragment_program", gl_extensions))
    {
      /* the shaders we'll feed to the GPU are simple enough, we don't need
       * to check GL limits for GL_FRAGMENT_PROGRAM_ARB */

      syms->glGenProgramsARB = (GLGENPROGRAMSPROC)
        cogl_get_proc_address ("glGenProgramsARB");
      syms->glBindProgramARB = (GLBINDPROGRAMPROC)
        cogl_get_proc_address ("glBindProgramARB");
      syms->glProgramStringARB = (GLPROGRAMSTRINGPROC)
        cogl_get_proc_address ("glProgramStringARB");

      if (syms->glGenProgramsARB &&
          syms->glBindProgramARB &&
          syms->glProgramStringARB)
        {
          features |= CLUTTER_HELIX_FP;
        }
    }
#endif

  if (cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    features |= CLUTTER_HELIX_GLSL;

  for (i = 0; renderers[i]; i++)
    {
      gint needed = renderers[i]->flags;

      if ((needed & features) == needed) 
        list = g_slist_prepend (list, renderers[i]);
    }

  return list;
}

static void
clutter_helix_video_texture_init (ClutterHelixVideoTexture *video_texture)
{
  ClutterHelixVideoTexturePrivate *priv;
  PlayerCallbacks callbacks = 
  {
    on_pos_length_cb,
    on_buffering_cb,
    on_state_change_cb,
    on_new_frame_cb,
    on_error_cb
  };


  video_texture->priv  = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (video_texture,
                                 CLUTTER_HELIX_TYPE_VIDEO_TEXTURE,
				                         ClutterHelixVideoTexturePrivate);
  get_player(&priv->player, &callbacks, (void *)video_texture);

  priv->id_lock = g_mutex_new();

  priv->renderers = clutter_helix_build_renderers_list (&priv->syms);
  priv->renderer_state = CLUTTER_HELIX_RENDERER_STOPPED;
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
                       "disable-slicing",
                       TRUE,
                       NULL);
}

