/* GStreamer
 *
 * unit test for avimux
 *
 * Copyright (C) <2006> Mark Nauwelaerts <manauw@skynet.be>
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

#include <unistd.h>

#include <gst/check/gstcheck.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#define AUDIO_CAPS_STRING "audio/x-ac3, " \
                        "channels = (int) 1, " \
                        "rate = (int) 8000"
#define VIDEO_CAPS_STRING "video/x-xvid, " \
                           "width = (int) 384, " \
                           "height = (int) 288, " \
                           "framerate = (fraction) 25/1"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-msvideo"));
static GstStaticPadTemplate srcvideotemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS_STRING));

static GstStaticPadTemplate srcaudiotemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (AUDIO_CAPS_STRING));


/* setup and teardown needs some special handling for muxer */
GstPad *
setup_src_pad (GstElement * element,
    GstStaticPadTemplate * template, GstCaps * caps, gchar * sinkname)
{
  GstPad *srcpad, *sinkpad;

  GST_DEBUG_OBJECT (element, "setting up sending pad");
  /* sending pad */
  srcpad = gst_pad_new_from_static_template (template, "src");
  fail_if (srcpad == NULL, "Could not create a srcpad");
  ASSERT_OBJECT_REFCOUNT (srcpad, "srcpad", 1);

  if (!(sinkpad = gst_element_get_static_pad (element, sinkname)))
    sinkpad = gst_element_get_request_pad (element, sinkname);
  fail_if (sinkpad == NULL, "Could not get sink pad from %s",
      GST_ELEMENT_NAME (element));
  /* references are owned by: 1) us, 2) avimux, 3) collect pads */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 3);
  if (caps)
    fail_unless (gst_pad_set_caps (srcpad, caps));
  fail_unless (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK,
      "Could not link source and %s sink pads", GST_ELEMENT_NAME (element));
  gst_object_unref (sinkpad);   /* because we got it higher up */

  /* references are owned by: 1) avimux, 2) collect pads */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 2);

  return srcpad;
}

void
teardown_src_pad (GstElement * element, gchar * sinkname)
{
  GstPad *srcpad, *sinkpad;
  gchar *padname;

  /* clean up floating src pad */
  /* hm, avimux uses _00 as suffixes for padnames */
  padname = g_strdup (sinkname);
  memcpy (strchr (padname, '%'), "00", 2);
  if (!(sinkpad = gst_element_get_static_pad (element, padname)))
    sinkpad = gst_element_get_request_pad (element, padname);
  g_free (padname);
  /* pad refs held by 1) avimux 2) collectpads and 3) us (through _get) */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 3);
  srcpad = gst_pad_get_peer (sinkpad);

  gst_pad_unlink (srcpad, sinkpad);

  /* after unlinking, pad refs still held by
   * 1) avimux and 2) collectpads and 3) us (through _get) */
  ASSERT_OBJECT_REFCOUNT (sinkpad, "sinkpad", 3);
  gst_object_unref (sinkpad);
  /* one more ref is held by element itself */

  /* pad refs held by both creator and this function (through _get_peer) */
  ASSERT_OBJECT_REFCOUNT (srcpad, "srcpad", 2);
  gst_object_unref (srcpad);
  gst_object_unref (srcpad);

}

GstElement *
setup_avimux (GstStaticPadTemplate * srctemplate, gchar * sinkname)
{
  GstElement *avimux;

  GST_DEBUG ("setup_avimux");
  avimux = gst_check_setup_element ("avimux");
  mysrcpad = setup_src_pad (avimux, srctemplate, NULL, sinkname);
  mysinkpad = gst_check_setup_sink_pad (avimux, &sinktemplate, NULL);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  return avimux;
}

void
cleanup_avimux (GstElement * avimux, gchar * sinkname)
{
  GST_DEBUG ("cleanup_avimux");
  gst_element_set_state (avimux, GST_STATE_NULL);

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  teardown_src_pad (avimux, sinkname);
  gst_check_teardown_sink_pad (avimux);
  gst_check_teardown_element (avimux);
}

void
check_avimux_pad (GstStaticPadTemplate * srctemplate, gchar * src_caps_string,
    gchar * chunk_id, gchar * sinkname)
{
  GstElement *avimux;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  int num_buffers;
  int i;
  guint8 data0[4] = "RIFF";
  guint8 data1[8] = "AVI LIST";
  guint8 data2[8] = "hdrlavih";
  guint8 data3[4] = "LIST";
  guint8 data4[8] = "strlstrh";
  guint8 data5[4] = "strf";
  guint8 data6[4] = "LIST";
  guint8 data7[4] = "movi";

  avimux = setup_avimux (srctemplate, sinkname);
  fail_unless (gst_element_set_state (avimux,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (1);
  caps = gst_caps_from_string (src_caps_string);
  gst_buffer_set_caps (inbuffer, caps);
  gst_caps_unref (caps);
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  num_buffers = g_list_length (buffers);
  /* at least expect avi header, chunk header, chunk and padding */
  fail_unless (num_buffers >= 4);

  for (i = 0; i < num_buffers; ++i) {
    outbuffer = GST_BUFFER (buffers->data);
    fail_if (outbuffer == NULL);
    buffers = g_list_remove (buffers, outbuffer);

    switch (i) {
      case 0:{                 /* check riff header */
        /* avi header */
        guint8 *data = GST_BUFFER_DATA (outbuffer);

        fail_unless (memcmp (data, data0, sizeof (data0)) == 0);
        fail_unless (memcmp (data + 8, data1, sizeof (data1)) == 0);
        fail_unless (memcmp (data + 20, data2, sizeof (data2)) == 0);
        /* video or audio header */
        data += 32 + 56;
        fail_unless (memcmp (data, data3, sizeof (data3)) == 0);
        fail_unless (memcmp (data + 8, data4, sizeof (data4)) == 0);
        fail_unless (memcmp (data + 76, data5, sizeof (data5)) == 0);
        /* avi data header */
        data = GST_BUFFER_DATA (outbuffer);
        data += GST_BUFFER_SIZE (outbuffer) - 12;
        fail_unless (memcmp (data, data6, sizeof (data6)) == 0);
        data += 8;
        fail_unless (memcmp (data, data7, sizeof (data7)) == 0);
        break;
      }
      case 1:                  /* chunk header */
        fail_unless (GST_BUFFER_SIZE (outbuffer) == 8);
        fail_unless (memcmp (GST_BUFFER_DATA (outbuffer), chunk_id, 4) == 0);
        break;
      case 2:
        fail_unless (GST_BUFFER_SIZE (outbuffer) == 1);
        break;
      case 3:                  /* buffer we put in, must be padded to even size */
        fail_unless (GST_BUFFER_SIZE (outbuffer) == 1);
        break;
      default:
        break;
    }

    ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
    gst_buffer_unref (outbuffer);
    outbuffer = NULL;
  }

  cleanup_avimux (avimux, sinkname);
  g_list_free (buffers);
  buffers = NULL;
}


GST_START_TEST (test_video_pad)
{
  check_avimux_pad (&srcvideotemplate, VIDEO_CAPS_STRING, "00db", "video_%d");
}

GST_END_TEST;


GST_START_TEST (test_audio_pad)
{
  check_avimux_pad (&srcaudiotemplate, AUDIO_CAPS_STRING, "00wb", "audio_%d");
}

GST_END_TEST;


Suite *
avimux_suite (void)
{
  Suite *s = suite_create ("avimux");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_video_pad);
  tcase_add_test (tc_chain, test_audio_pad);

  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = avimux_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
