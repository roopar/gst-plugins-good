/* -*- c-basic-offset: 2 -*-
 * 
 * GStreamer
 * Copyright (C) 1999-2001 Erik Walthinsen <omega@cse.ogi.edu>
 *               2006 Dreamlab Technologies Ltd. <mathis.hofer@dreamlab.net>
 *               2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
 * 
 */

#ifndef __GST_AUDIO_FX_BASE_FIR_FILTER_H__
#define __GST_AUDIO_FX_BASE_FIR_FILTER_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_AUDIO_FX_BASE_FIR_FILTER \
  (gst_audio_fx_base_fir_filter_get_type())
#define GST_AUDIO_FX_BASE_FIR_FILTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIO_FX_BASE_FIR_FILTER,GstAudioFXBaseFIRFilter))
#define GST_AUDIO_FX_BASE_FIR_FILTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIO_FX_BASE_FIR_FILTER,GstAudioFXBaseFIRFilterClass))
#define GST_IS_AUDIO_FX_BASE_FIR_FILTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIO_FX_BASE_FIR_FILTER))
#define GST_IS_AUDIO_FX_BASE_FIR_FILTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUDIO_FX_BASE_FIR_FILTER))

typedef struct _GstAudioFXBaseFIRFilter GstAudioFXBaseFIRFilter;
typedef struct _GstAudioFXBaseFIRFilterClass GstAudioFXBaseFIRFilterClass;

typedef void (*GstAudioFXBaseFIRFilterProcessFunc) (GstAudioFXBaseFIRFilter *, guint8 *, guint8 *, guint);

/**
 * GstAudioFXBaseFIRFilter:
 *
 * Opaque data structure.
 */
struct _GstAudioFXBaseFIRFilter {
  GstAudioFilter element;

  /* < private > */
  GstAudioFXBaseFIRFilterProcessFunc process;

  gdouble *kernel;              /* filter kernel */
  guint kernel_length;           /* length of the filter kernel */
  gdouble *residue;             /* buffer for left-over samples from previous buffer */
  guint residue_length;

  guint64 latency;

  GstClockTime next_ts;
  guint64 next_off;
};

struct _GstAudioFXBaseFIRFilterClass {
  GstAudioFilterClass parent_class;
};

GType gst_audio_fx_base_fir_filter_get_type (void);
void gst_audio_fx_base_fir_filter_set_kernel (GstAudioFXBaseFIRFilter *filter, gdouble *kernel, guint kernel_length, guint64 latency);
void gst_audio_fx_base_fir_filter_push_residue (GstAudioFXBaseFIRFilter *filter);

G_END_DECLS

#endif /* __GST_AUDIO_FX_BASE_FIR_FILTER_H__ */
