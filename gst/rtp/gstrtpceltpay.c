/* GStreamer
 * Copyright (C) <2009> Wim Taymans <wim.taymans@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <gst/rtp/gstrtpbuffer.h>

#include "gstrtpceltpay.h"

GST_DEBUG_CATEGORY_STATIC (rtpceltpay_debug);
#define GST_CAT_DEFAULT (rtpceltpay_debug)

/* elementfactory information */
static const GstElementDetails gst_rtp_celt_pay_details =
GST_ELEMENT_DETAILS ("RTP CELT payloader",
    "Codec/Payloader/Network",
    "Payload-encodes CELT audio into a RTP packet",
    "Wim Taymans <wim.taymans@gmail.com>");

static GstStaticPadTemplate gst_rtp_celt_pay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-celt, "
        "rate = (int) [ 32000, 64000 ], " "channels = (int) [1, 2]")
    );

static GstStaticPadTemplate gst_rtp_celt_pay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "media = (string) \"audio\", "
        "payload = (int) " GST_RTP_PAYLOAD_DYNAMIC_STRING ", "
        "clock-rate =  (int) [ 32000, 48000 ], "
        "encoding-name = (string) \"CELT\"")
    );

static void gst_rtp_celt_pay_finalize (GObject * object);

static GstStateChangeReturn gst_rtp_celt_pay_change_state (GstElement *
    element, GstStateChange transition);

static gboolean gst_rtp_celt_pay_setcaps (GstBaseRTPPayload * payload,
    GstCaps * caps);
static GstCaps *gst_rtp_celt_pay_getcaps (GstBaseRTPPayload * payload,
    GstPad * pad);
static GstFlowReturn gst_rtp_celt_pay_handle_buffer (GstBaseRTPPayload *
    payload, GstBuffer * buffer);

GST_BOILERPLATE (GstRtpCELTPay, gst_rtp_celt_pay, GstBaseRTPPayload,
    GST_TYPE_BASE_RTP_PAYLOAD);

static void
gst_rtp_celt_pay_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_rtp_celt_pay_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_rtp_celt_pay_src_template));
  gst_element_class_set_details (element_class, &gst_rtp_celt_pay_details);

  GST_DEBUG_CATEGORY_INIT (rtpceltpay_debug, "rtpceltpay", 0,
      "CELT RTP Payloader");
}

static void
gst_rtp_celt_pay_class_init (GstRtpCELTPayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseRTPPayloadClass *gstbasertppayload_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasertppayload_class = (GstBaseRTPPayloadClass *) klass;

  gobject_class->finalize = gst_rtp_celt_pay_finalize;

  gstelement_class->change_state = gst_rtp_celt_pay_change_state;

  gstbasertppayload_class->set_caps = gst_rtp_celt_pay_setcaps;
  gstbasertppayload_class->get_caps = gst_rtp_celt_pay_getcaps;
  gstbasertppayload_class->handle_buffer = gst_rtp_celt_pay_handle_buffer;
}

static void
gst_rtp_celt_pay_init (GstRtpCELTPay * rtpceltpay, GstRtpCELTPayClass * klass)
{
  rtpceltpay->queue = g_queue_new ();
}

static void
gst_rtp_celt_pay_finalize (GObject * object)
{
  GstRtpCELTPay *rtpceltpay;

  rtpceltpay = GST_RTP_CELT_PAY (object);

  g_queue_free (rtpceltpay->queue);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_rtp_celt_pay_clear_queued (GstRtpCELTPay * rtpceltpay)
{
  GstBuffer *buf;

  while ((buf = g_queue_pop_head (rtpceltpay->queue)))
    gst_buffer_unref (buf);

  rtpceltpay->bytes = 0;
  rtpceltpay->sbytes = 0;
  rtpceltpay->qduration = 0;
}

static void
gst_rtp_celt_pay_add_queued (GstRtpCELTPay * rtpceltpay, GstBuffer * buffer,
    guint ssize, guint size, GstClockTime duration)
{
  g_queue_push_tail (rtpceltpay->queue, buffer);
  rtpceltpay->sbytes += ssize;
  rtpceltpay->bytes += size;
  rtpceltpay->qduration += duration;
}

static gboolean
gst_rtp_celt_pay_setcaps (GstBaseRTPPayload * payload, GstCaps * caps)
{
  /* don't configure yet, we wait for the ident packet */
  return TRUE;
}


static GstCaps *
gst_rtp_celt_pay_getcaps (GstBaseRTPPayload * payload, GstPad * pad)
{
  GstCaps *otherpadcaps;
  GstCaps *caps;

  otherpadcaps = gst_pad_get_allowed_caps (payload->srcpad);
  caps = gst_caps_copy (gst_pad_get_pad_template_caps (pad));

  if (otherpadcaps) {
    if (!gst_caps_is_empty (otherpadcaps)) {
      GstStructure *ps = gst_caps_get_structure (otherpadcaps, 0);
      GstStructure *s = gst_caps_get_structure (caps, 0);
      gint clock_rate;

      if (gst_structure_get_int (ps, "clock-rate", &clock_rate)) {
        gst_structure_fixate_field_nearest_int (s, "rate", clock_rate);
      }
    }
    gst_caps_unref (otherpadcaps);
  }

  return caps;
}

static gboolean
gst_rtp_celt_pay_parse_ident (GstRtpCELTPay * rtpceltpay,
    const guint8 * data, guint size)
{
  guint32 version, header_size, rate, nb_channels, frame_size, overlap;
  guint32 bytes_per_packet;
  GstBaseRTPPayload *payload;
  gchar *cstr, *fsstr;
  gboolean res;

  /* we need the header string (8), the version string (20), the version
   * and the header length. */
  if (size < 36)
    goto too_small;

  if (!g_str_has_prefix ((const gchar *) data, "CELT    "))
    goto wrong_header;

  /* skip header and version string */
  data += 28;

  version = GST_READ_UINT32_LE (data);
  GST_DEBUG_OBJECT (rtpceltpay, "version %08x", version);
#if 0
  if (version != 1)
    goto wrong_version;
#endif

  data += 4;
  /* ensure sizes */
  header_size = GST_READ_UINT32_LE (data);
  if (header_size < 56)
    goto header_too_small;

  if (size < header_size)
    goto payload_too_small;

  data += 4;
  rate = GST_READ_UINT32_LE (data);
  data += 4;
  nb_channels = GST_READ_UINT32_LE (data);
  data += 4;
  frame_size = GST_READ_UINT32_LE (data);
  data += 4;
  overlap = GST_READ_UINT32_LE (data);
  data += 4;
  bytes_per_packet = GST_READ_UINT32_LE (data);

  GST_DEBUG_OBJECT (rtpceltpay, "rate %d, nb_channels %d, frame_size %d",
      rate, nb_channels, frame_size);
  GST_DEBUG_OBJECT (rtpceltpay, "overlap %d, bytes_per_packet %d",
      overlap, bytes_per_packet);

  payload = GST_BASE_RTP_PAYLOAD (rtpceltpay);

  gst_basertppayload_set_options (payload, "audio", FALSE, "CELT", rate);
  cstr = g_strdup_printf ("%d", nb_channels);
  fsstr = g_strdup_printf ("%d", frame_size);
  res = gst_basertppayload_set_outcaps (payload, "encoding-params",
      G_TYPE_STRING, cstr, "frame-size", G_TYPE_STRING, fsstr, NULL);
  g_free (cstr);
  g_free (fsstr);

  return res;

  /* ERRORS */
too_small:
  {
    GST_DEBUG_OBJECT (rtpceltpay,
        "ident packet too small, need at least 32 bytes");
    return FALSE;
  }
wrong_header:
  {
    GST_DEBUG_OBJECT (rtpceltpay,
        "ident packet does not start with \"CELT    \"");
    return FALSE;
  }
#if 0
wrong_version:
  {
    GST_DEBUG_OBJECT (rtpceltpay, "can only handle version 1, have version %d",
        version);
    return FALSE;
  }
#endif
header_too_small:
  {
    GST_DEBUG_OBJECT (rtpceltpay,
        "header size too small, need at least 80 bytes, " "got only %d",
        header_size);
    return FALSE;
  }
payload_too_small:
  {
    GST_DEBUG_OBJECT (rtpceltpay,
        "payload too small, need at least %d bytes, got only %d", header_size,
        size);
    return FALSE;
  }
}

static GstFlowReturn
gst_rtp_celt_pay_flush_queued (GstRtpCELTPay * rtpceltpay)
{
  GstFlowReturn ret;
  GstBuffer *buf, *outbuf;
  guint8 *payload, *spayload;
  guint payload_len;
  GstClockTime duration;

  payload_len = rtpceltpay->bytes + rtpceltpay->sbytes;
  duration = rtpceltpay->qduration;

  GST_DEBUG_OBJECT (rtpceltpay, "flushing out %u, duration %" GST_TIME_FORMAT,
      payload_len, GST_TIME_ARGS (rtpceltpay->qduration));

  /* get a big enough packet for the sizes + payloads */
  outbuf = gst_rtp_buffer_new_allocate (payload_len, 0, 0);

  GST_BUFFER_DURATION (outbuf) = duration;

  /* point to the payload for size headers and data */
  spayload = gst_rtp_buffer_get_payload (outbuf);
  payload = spayload + rtpceltpay->sbytes;

  while ((buf = g_queue_pop_head (rtpceltpay->queue))) {
    guint size;

    /* copy first timestamp to output */
    if (GST_BUFFER_TIMESTAMP (outbuf) == -1)
      GST_BUFFER_TIMESTAMP (outbuf) = GST_BUFFER_TIMESTAMP (buf);

    /* write the size to the header */
    size = GST_BUFFER_SIZE (buf);
    while (size > 0xff) {
      *spayload++ = 0xff;
      size -= 0xff;
    }
    *spayload++ = size;

    size = GST_BUFFER_SIZE (buf);
    /* copy payload */
    memcpy (payload, GST_BUFFER_DATA (buf), size);
    payload += size;

    gst_buffer_unref (buf);
  }

  /* we consumed it all */
  rtpceltpay->bytes = 0;
  rtpceltpay->sbytes = 0;
  rtpceltpay->qduration = 0;

  ret = gst_basertppayload_push (GST_BASE_RTP_PAYLOAD (rtpceltpay), outbuf);

  return ret;
}

static GstFlowReturn
gst_rtp_celt_pay_handle_buffer (GstBaseRTPPayload * basepayload,
    GstBuffer * buffer)
{
  GstFlowReturn ret;
  GstRtpCELTPay *rtpceltpay;
  guint size, payload_len;
  guint8 *data;
  GstClockTime duration, packet_dur;
  guint i, ssize, packet_len;

  rtpceltpay = GST_RTP_CELT_PAY (basepayload);

  ret = GST_FLOW_OK;

  size = GST_BUFFER_SIZE (buffer);
  data = GST_BUFFER_DATA (buffer);

  switch (rtpceltpay->packet) {
    case 0:
      /* ident packet. We need to parse the headers to construct the RTP
       * properties. */
      if (!gst_rtp_celt_pay_parse_ident (rtpceltpay, data, size))
        goto parse_error;

      goto done;
    case 1:
      /* comment packet, we ignore it */
      goto done;
    default:
      /* other packets go in the payload */
      break;
  }

  duration = GST_BUFFER_DURATION (buffer);

  GST_DEBUG_OBJECT (rtpceltpay,
      "got buffer of duration %" GST_TIME_FORMAT ", size %u",
      GST_TIME_ARGS (duration), size);

  /* calculate the size of the size field and the payload */
  ssize = 1;
  for (i = size; i > 0xff; i -= 0xff)
    ssize++;

  GST_DEBUG_OBJECT (rtpceltpay, "bytes for size %u", ssize);

  /* calculate the size and duration of the packet */
  payload_len = ssize + size + rtpceltpay->bytes + rtpceltpay->sbytes;
  packet_dur = rtpceltpay->qduration + duration;

  packet_len = gst_rtp_buffer_calc_packet_len (payload_len, 0, 0);

  if (gst_basertppayload_is_filled (basepayload, packet_len, packet_dur)) {
    /* size or duration would overflow the packet, flush the queued data */
    ret = gst_rtp_celt_pay_flush_queued (rtpceltpay);
  }

  /* queue the packet */
  gst_rtp_celt_pay_add_queued (rtpceltpay, buffer, ssize, size, duration);

done:
  rtpceltpay->packet++;

  return ret;

  /* ERRORS */
parse_error:
  {
    GST_ELEMENT_ERROR (rtpceltpay, STREAM, DECODE, (NULL),
        ("Error parsing first identification packet."));
    return GST_FLOW_ERROR;
  }
}

static GstStateChangeReturn
gst_rtp_celt_pay_change_state (GstElement * element, GstStateChange transition)
{
  GstRtpCELTPay *rtpceltpay;
  GstStateChangeReturn ret;

  rtpceltpay = GST_RTP_CELT_PAY (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      rtpceltpay->packet = 0;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_rtp_celt_pay_clear_queued (rtpceltpay);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }
  return ret;
}

gboolean
gst_rtp_celt_pay_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rtpceltpay",
      GST_RANK_NONE, GST_TYPE_RTP_CELT_PAY);
}
