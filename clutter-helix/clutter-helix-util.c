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

#include "clutter-helix-util.h"

/**
 * SECTION:clutter-helix-util
 * @short_description: Utility functions for ClutterHelix.
 *
 * Various Utility functions for ClutterHelix.
 */

/**
 * clutter_helix_init:
 * @argc: pointer to the argument list count
 * @argv: pointer to the argument list vector
 *
 * Utility function to call helix_init(), then clutter_init().
 *
 * Return value: A #ClutterInitError.
 */
ClutterInitError
clutter_helix_init (int *argc, char ***argv)
{
  static gboolean clutter_is_initialized = FALSE;
  ClutterInitError retval;

  if (!g_thread_supported ())
         g_thread_init (NULL);

  if (!clutter_is_initialized)
    {
      retval = clutter_init (argc, argv);

      clutter_is_initialized = TRUE;
    }
  else
    retval = CLUTTER_INIT_SUCCESS;

  return retval;
}
