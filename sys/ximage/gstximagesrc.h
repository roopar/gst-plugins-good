/* screenshotsrc: Screenshot plugin for GStreamer
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_XIMAGESRC_H__
#define __GST_XIMAGESRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include "ximageutil.h"

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif

G_BEGIN_DECLS

#define GST_TYPE_XIMAGESRC (gst_ximagesrc_get_type())
#define GST_XIMAGESRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_XIMAGESRC,GstXImageSrc))
#define GST_XIMAGESRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_XIMAGESRC,GstXImageSrc))
#define GST_IS_XIMAGESRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_XIMAGESRC))
#define GST_IS_XIMAGESRC_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_XIMAGESRC))

typedef struct _GstXImageSrc GstXImageSrc;
typedef struct _GstXImageSrcClass GstXImageSrcClass;

GType gst_ximagesrc_get_type (void) G_GNUC_CONST;

struct _GstXImageSrc
{
  GstPushSrc parent;

  /* Information on display */
  GstXContext *xcontext;
  gint width;
  gint height;
  
  Window xwindow;
  gchar *display_name;
  guint screen_num;
  
  /* Desired output framerate */
  gint fps_n;
  gint fps_d;
  
  /* for framerate sync */
  GstClockID clock_id; 
  gint64 last_frame_no;
  
  /* Protect X Windows calls */
  GMutex *x_lock;

  /* Gathered pool of emitted buffers */
  GMutex *pool_lock;
  GSList *buffer_pool;

  /* XFixes and XDamage support */
  gboolean have_xfixes;
  gboolean have_xdamage;
  gboolean show_pointer;
#ifdef HAVE_XFIXES
  int fixes_event_base;
#endif
#ifdef HAVE_XDAMAGE
  Damage damage;
  int damage_event_base;
  XserverRegion damage_region;
  GC damage_copy_gc;
#endif

};

struct _GstXImageSrcClass
{
  GstPushSrcClass parent_class;
};

G_END_DECLS

#endif /* __GST_XIMAGESRC_H__ */