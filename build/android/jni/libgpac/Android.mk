LOCAL_PATH:= $(call my-dir)
APP_ABI          := armeabi armeabi-v7a x86

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../common.mk

LOCAL_MODULE		:= gpac
LOCAL_C_INCLUDES 	:= $(LOCAL_PATH)

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../include
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/freetype
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/freetype/freetype
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/jpeg/
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/png/
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/faad
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/js/
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/ffmpeg_android/

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../modules

LOCAL_LDLIBS    += -L$(LOCAL_PATH)/../../../../extra_lib/lib/android/$(TARGET_ARCH_ABI)
LOCAL_LDLIBS    += -lGLESv2 -ldl
LOCAL_LDLIBS    += -ljs_osmo -leditline -lft2 -ljpegdroid -lopenjpeg -lpng -lfaad -lmad -lz

#ffmpeg
LOCAL_LDLIBS    += -lavcodec -lavformat -lswresample -lavfilter -lavutil -lavdevice -lswscale

#mediacodec - removed  ljavaenv from original settings
LOCAL_LDLIBS    += -llog -ldl -lOpenMAXAL -lmediandk -landroid -lGLESv2

#LOCAL_SHARED_LIBRARIES    += -L$(LOCAL_PATH)/../../../../extra_lib/lib/android/$(TARGET_ARCH_ABI)
#LOCAL_SHARED_LIBRARIES    += -lGLESv2 -ldl
#LOCAL_SHARED_LIBRARIES    += -ljs_osmo -leditline -lft2 -ljpegdroid -lopenjpeg -lpng -lz

#LOCAL_EXPORT_LDLIBS= -ljs_osmo -leditline -lft2 -ljpeg -lopenjpeg -lpng -lz


LOCAL_CFLAGS +=	-DGPAC_HAVE_CONFIG_H
LOCAL_CFLAGS += -DNO_MALLINFO
LOCAL_CFLAGS += -DGPAC_CONFIG_ANDROID
LOCAL_CFLAGS += -DGPAC_DISABLE_REMOTERY

#LOCAL_CFLAGS += -DGPAC_FIXED_POINT

LOCAL_SRC_FILES := \
	../../../../src/bifs/arith_decoder.c \
	../../../../src/bifs/bifs_codec.c \
	../../../../src/bifs/bifs_node_tables.c \
	../../../../src/bifs/com_dec.c \
	../../../../src/bifs/com_enc.c \
	../../../../src/bifs/conditional.c \
	../../../../src/bifs/field_decode.c \
	../../../../src/bifs/field_encode.c \
	../../../../src/bifs/memory_decoder.c \
	../../../../src/bifs/predictive_mffield.c \
	../../../../src/bifs/quantize.c \
	../../../../src/bifs/script_dec.c \
	../../../../src/bifs/script_enc.c \
	../../../../src/bifs/unquantize.c \
	../../../../src/compositor/audio_input.c \
	../../../../src/compositor/audio_mixer.c \
	../../../../src/compositor/audio_render.c \
	../../../../src/compositor/bindable.c \
	../../../../src/compositor/camera.c \
	../../../../src/compositor/clock.c \
	../../../../src/compositor/compositor_2d.c \
	../../../../src/compositor/compositor_3d.c \
	../../../../src/compositor/compositor.c \
	../../../../src/compositor/compositor_node_init.c \
	../../../../src/compositor/drawable.c \
	../../../../src/compositor/events.c \
	../../../../src/compositor/font_engine.c \
	../../../../src/compositor/hardcoded_protos.c \
	../../../../src/compositor/hc_flash_shape.c \
	../../../../src/compositor/media_object.c \
	../../../../src/compositor/mesh.c \
	../../../../src/compositor/mesh_collide.c \
	../../../../src/compositor/mesh_tesselate.c \
	../../../../src/compositor/mpeg4_animstream.c \
	../../../../src/compositor/mpeg4_audio.c \
	../../../../src/compositor/mpeg4_background2d.c \
	../../../../src/compositor/mpeg4_background.c \
	../../../../src/compositor/mpeg4_bitmap.c \
	../../../../src/compositor/mpeg4_composite.c \
	../../../../src/compositor/mpeg4_form.c \
	../../../../src/compositor/mpeg4_geometry_2d.c \
	../../../../src/compositor/mpeg4_geometry_3d.c \
	../../../../src/compositor/mpeg4_geometry_ifs2d.c \
	../../../../src/compositor/mpeg4_geometry_ils2d.c \
	../../../../src/compositor/mpeg4_gradients.c \
	../../../../src/compositor/mpeg4_grouping_2d.c \
	../../../../src/compositor/mpeg4_grouping_3d.c \
	../../../../src/compositor/mpeg4_grouping.c \
	../../../../src/compositor/mpeg4_inline.c \
	../../../../src/compositor/mpeg4_inputsensor.c \
	../../../../src/compositor/mpeg4_layer_2d.c \
	../../../../src/compositor/mpeg4_layer_3d.c \
	../../../../src/compositor/mpeg4_layout.c \
	../../../../src/compositor/mpeg4_lighting.c \
	../../../../src/compositor/mpeg4_mediacontrol.c \
	../../../../src/compositor/mpeg4_mediasensor.c \
	../../../../src/compositor/mpeg4_path_layout.c \
	../../../../src/compositor/mpeg4_sensors.c \
	../../../../src/compositor/mpeg4_sound.c \
	../../../../src/compositor/mpeg4_text.c \
	../../../../src/compositor/mpeg4_textures.c \
	../../../../src/compositor/mpeg4_timesensor.c \
	../../../../src/compositor/mpeg4_viewport.c \
	../../../../src/compositor/navigate.c \
	../../../../src/compositor/object_manager.c \
	../../../../src/compositor/offscreen_cache.c \
	../../../../src/compositor/scene.c \
	../../../../src/compositor/scene_node_init.c \
	../../../../src/compositor/scene_ns.c \
	../../../../src/compositor/svg_base.c \
	../../../../src/compositor/svg_external.c \
	../../../../src/compositor/svg_filters.c \
	../../../../src/compositor/svg_font.c \
	../../../../src/compositor/svg_geometry.c \
	../../../../src/compositor/svg_grouping.c \
	../../../../src/compositor/svg_media.c \
	../../../../src/compositor/svg_paint_servers.c \
	../../../../src/compositor/svg_text.c \
	../../../../src/compositor/texturing.c \
	../../../../src/compositor/texturing_gl.c \
	../../../../src/compositor/visual_manager_2d.c \
	../../../../src/compositor/visual_manager_2d_draw.c \
	../../../../src/compositor/visual_manager_3d.c \
	../../../../src/compositor/visual_manager_3d_gl.c \
	../../../../src/compositor/visual_manager.c \
	../../../../src/compositor/x3d_geometry.c \
	../../../../src/crypto/g_crypt.c \
	../../../../src/crypto/g_crypt_openssl.c \
	../../../../src/crypto/g_crypt_tinyaes.c \
	../../../../src/crypto/tiny_aes.c \
	../../../../src/evg/ftgrays.c \
	../../../../src/evg/raster_565.c \
	../../../../src/evg/raster_argb.c\
	../../../../src/evg/raster_rgb.c \
	../../../../src/evg/raster_yuv.c \
	../../../../src/evg/stencil.c \
	../../../../src/evg/surface.c \
	../../../../src/filter_core/filter.c \
	../../../../src/filter_core/filter_pck.c \
	../../../../src/filter_core/filter_pid.c \
	../../../../src/filter_core/filter_props.c \
	../../../../src/filter_core/filter_queue.c \
	../../../../src/filter_core/filter_register.c \
	../../../../src/filter_core/filter_session.c \
	../../../../src/filters/compose.c \
	../../../../src/filters/dasher.c \
	../../../../src/filters/dec_ac52.c \
	../../../../src/filters/dec_bifs.c \
	../../../../src/filters/dec_faad.c \
	../../../../src/filters/dec_img.c \
	../../../../src/filters/dec_j2k.c \
	../../../../src/filters/dec_laser.c \
	../../../../src/filters/dec_mad.c \
	../../../../src/filters/dec_odf.c \
	../../../../src/filters/dec_openhevc.c \
	../../../../src/filters/dec_opensvc.c \
	../../../../src/filters/decrypt_cenc_isma.c \
	../../../../src/filters/dec_theora.c \
	../../../../src/filters/dec_ttxt.c \
	../../../../src/filters/dec_vorbis.c \
	../../../../src/filters/dec_vtb.c \
	../../../../src/filters/dec_webvtt.c \
	../../../../src/filters/dec_xvid.c \
	../../../../src/filters/dec_mediacodec.c \
	../../../../src/filters/dec_mediacodec_jni.c \
	../../../../src/filters/dmx_avi.c \
	../../../../src/filters/dmx_dash.c \
	../../../../src/filters/dmx_gsf.c \
	../../../../src/filters/dmx_m2ts.c \
	../../../../src/filters/dmx_mpegps.c \
	../../../../src/filters/dmx_nhml.c \
	../../../../src/filters/dmx_nhnt.c \
	../../../../src/filters/dmx_ogg.c \
	../../../../src/filters/dmx_saf.c \
	../../../../src/filters/dmx_vobsub.c \
	../../../../src/filters/enc_jpg.c \
	../../../../src/filters/enc_png.c \
	../../../../src/filters/encrypt_cenc_isma.c \
	../../../../src/filters/ff_common.c \
	../../../../src/filters/ff_dec.c \
	../../../../src/filters/ff_dmx.c \
	../../../../src/filters/ff_enc.c \
	../../../../src/filters/ff_rescale.c \
	../../../../src/filters/filelist.c \
	../../../../src/filters/hevcmerge.c \
	../../../../src/filters/hevcsplit.c \
	../../../../src/filters/in_atsc.c \
	../../../../src/filters/in_dvb4linux.c \
	../../../../src/filters/in_file.c \
	../../../../src/filters/in_http.c \
	../../../../src/filters/in_rtp.c \
	../../../../src/filters/in_rtp_rtsp.c \
	../../../../src/filters/in_rtp_sdp.c \
	../../../../src/filters/in_rtp_signaling.c \
	../../../../src/filters/in_rtp_stream.c \
	../../../../src/filters/in_sock.c \
	../../../../src/filters/inspect.c \
	../../../../src/filters/isoffin_load.c \
	../../../../src/filters/isoffin_read.c \
	../../../../src/filters/isoffin_read_ch.c \
	../../../../src/filters/load_bt_xmt.c \
	../../../../src/filters/load_svg.c \
	../../../../src/filters/load_text.c \
	../../../../src/filters/mux_avi.c \
	../../../../src/filters/mux_gsf.c \
	../../../../src/filters/mux_isom.c \
	../../../../src/filters/mux_ts.c \
	../../../../src/filters/out_audio.c \
	../../../../src/filters/out_file.c \
	../../../../src/filters/out_rtp.c \
	../../../../src/filters/out_rtsp.c \
	../../../../src/filters/out_sock.c \
	../../../../src/filters/reframe_ac3.c \
	../../../../src/filters/reframe_adts.c \
	../../../../src/filters/reframe_amr.c \
	../../../../src/filters/reframe_av1.c \
	../../../../src/filters/reframe_flac.c \
	../../../../src/filters/reframe_h263.c \
	../../../../src/filters/reframe_img.c \
	../../../../src/filters/reframe_latm.c \
	../../../../src/filters/reframe_mp3.c \
	../../../../src/filters/reframe_mpgvid.c \
	../../../../src/filters/reframe_nalu.c \
	../../../../src/filters/reframe_qcp.c \
	../../../../src/filters/reframe_rawpcm.c \
	../../../../src/filters/reframe_rawvid.c \
	../../../../src/filters/reframer.c \
	../../../../src/filters/resample_audio.c \
	../../../../src/filters/rewind.c \
	../../../../src/filters/rewrite_adts.c \
	../../../../src/filters/rewrite_mp4v.c \
	../../../../src/filters/rewrite_nalu.c \
	../../../../src/filters/rewrite_obu.c \
	../../../../src/filters/tileagg.c \
	../../../../src/filters/unit_test_filter.c \
	../../../../src/filters/vcrop.c \
	../../../../src/filters/vflip.c \
	../../../../src/filters/write_generic.c \
	../../../../src/filters/write_nhml.c \
	../../../../src/filters/write_nhnt.c \
	../../../../src/filters/write_qcp.c \
	../../../../src/filters/write_vtt.c \
	../../../../src/ietf/rtcp.c \
	../../../../src/ietf/rtp.c \
	../../../../src/ietf/rtp_depacketizer.c \
	../../../../src/ietf/rtp_packetizer.c \
	../../../../src/ietf/rtp_pck_3gpp.c \
	../../../../src/ietf/rtp_pck_mpeg12.c \
	../../../../src/ietf/rtp_pck_mpeg4.c \
	../../../../src/ietf/rtp_streamer.c \
	../../../../src/ietf/rtsp_command.c \
	../../../../src/ietf/rtsp_common.c \
	../../../../src/ietf/rtsp_response.c \
	../../../../src/ietf/rtsp_session.c \
	../../../../src/ietf/sdp.c \
	../../../../src/isomedia/avc_ext.c \
	../../../../src/isomedia/box_code_3gpp.c \
	../../../../src/isomedia/box_code_adobe.c \
	../../../../src/isomedia/box_code_apple.c \
	../../../../src/isomedia/box_code_base.c \
	../../../../src/isomedia/box_code_drm.c \
	../../../../src/isomedia/box_code_meta.c \
	../../../../src/isomedia/box_dump.c \
	../../../../src/isomedia/box_funcs.c \
	../../../../src/isomedia/data_map.c \
	../../../../src/isomedia/drm_sample.c \
	../../../../src/isomedia/hinting.c \
	../../../../src/isomedia/hint_track.c \
	../../../../src/isomedia/iff.c \
	../../../../src/isomedia/isom_intern.c \
	../../../../src/isomedia/isom_read.c \
	../../../../src/isomedia/isom_store.c \
	../../../../src/isomedia/isom_write.c \
	../../../../src/isomedia/media.c \
	../../../../src/isomedia/media_odf.c \
	../../../../src/isomedia/meta.c \
	../../../../src/isomedia/movie_fragments.c \
	../../../../src/isomedia/sample_descs.c \
	../../../../src/isomedia/stbl_read.c \
	../../../../src/isomedia/stbl_write.c \
	../../../../src/isomedia/track.c \
	../../../../src/isomedia/tx3g.c \
	../../../../src/laser/lsr_dec.c \
	../../../../src/laser/lsr_enc.c \
	../../../../src/laser/lsr_tables.c \
	../../../../src/media_tools/ait.c \
	../../../../src/media_tools/atsc_dmx.c \
	../../../../src/media_tools/avilib.c \
	../../../../src/media_tools/av_parsers.c \
	../../../../src/media_tools/crypt_tools.c \
	../../../../src/media_tools/dash_client.c \
	../../../../src/media_tools/dash_segmenter.c \
	../../../../src/media_tools/dsmcc.c \
	../../../../src/media_tools/dvb_mpe.c \
	../../../../src/media_tools/gpac_ogg.c \
	../../../../src/media_tools/img.c \
	../../../../src/media_tools/isom_hinter.c \
	../../../../src/media_tools/isom_tools.c \
	../../../../src/media_tools/m2ts_mux.c \
	../../../../src/media_tools/m3u8.c \
	../../../../src/media_tools/media_export.c \
	../../../../src/media_tools/media_import.c \
	../../../../src/media_tools/mpd.c \
	../../../../src/media_tools/mpeg2_ps.c \
	../../../../src/media_tools/mpegts.c \
	../../../../src/media_tools/reedsolomon.c \
	../../../../src/media_tools/saf.c \
	../../../../src/media_tools/vobsub.c \
	../../../../src/media_tools/webvtt.c \
	../../../../src/odf/desc_private.c \
	../../../../src/odf/descriptors.c \
	../../../../src/odf/ipmpx_code.c \
	../../../../src/odf/ipmpx_dump.c \
	../../../../src/odf/ipmpx_parse.c \
	../../../../src/odf/oci_codec.c \
	../../../../src/odf/odf_code.c \
	../../../../src/odf/odf_codec.c \
	../../../../src/odf/odf_command.c \
	../../../../src/odf/odf_dump.c \
	../../../../src/odf/odf_parse.c \
	../../../../src/odf/qos.c \
	../../../../src/odf/slc.c \
	../../../../src/scenegraph/base_scenegraph.c \
	../../../../src/scenegraph/commands.c \
	../../../../src/scenegraph/dom_events.c \
	../../../../src/scenegraph/dom_smjs.c \
	../../../../src/scenegraph/mpeg4_animators.c \
	../../../../src/scenegraph/mpeg4_nodes.c \
	../../../../src/scenegraph/mpeg4_valuator.c \
	../../../../src/scenegraph/smil_anim.c \
	../../../../src/scenegraph/smil_timing.c \
	../../../../src/scenegraph/svg_attributes.c \
	../../../../src/scenegraph/svg_properties.c \
	../../../../src/scenegraph/svg_smjs.c \
	../../../../src/scenegraph/svg_types.c \
	../../../../src/scenegraph/vrml_interpolators.c \
	../../../../src/scenegraph/vrml_proto.c \
	../../../../src/scenegraph/vrml_route.c \
	../../../../src/scenegraph/vrml_script.c \
	../../../../src/scenegraph/vrml_smjs.c \
	../../../../src/scenegraph/vrml_tools.c \
	../../../../src/scenegraph/x3d_nodes.c \
	../../../../src/scenegraph/xml_ns.c \
	../../../../src/scene_manager/encode_isom.c \
	../../../../src/scene_manager/loader_bt.c \
	../../../../src/scene_manager/loader_isom.c \
	../../../../src/scene_manager/loader_qt.c \
	../../../../src/scene_manager/loader_svg.c \
	../../../../src/scene_manager/loader_xmt.c \
	../../../../src/scene_manager/scene_dump.c \
	../../../../src/scene_manager/scene_engine.c \
	../../../../src/scene_manager/scene_manager.c \
	../../../../src/scene_manager/scene_stats.c \
	../../../../src/scene_manager/swf_bifs.c \
	../../../../src/scene_manager/swf_parse.c \
	../../../../src/scene_manager/swf_svg.c \
	../../../../src/scene_manager/text_to_bifs.c \
	../../../../src/terminal/terminal.c \
	../../../../src/utils/alloc.c \
	../../../../src/utils/base_encoding.c \
	../../../../src/utils/bitstream.c \
	../../../../src/utils/cache.c \
	../../../../src/utils/color.c \
	../../../../src/utils/configfile.c \
	../../../../src/utils/constants.c \
	../../../../src/utils/downloader.c \
	../../../../src/utils/error.c \
	../../../../src/utils/list.c \
	../../../../src/utils/math.c \
	../../../../src/utils/module.c \
	../../../../src/utils/os_config_init.c \
	../../../../src/utils/os_divers.c \
	../../../../src/utils/os_file.c \
	../../../../src/utils/os_module.c \
	../../../../src/utils/os_net.c \
	../../../../src/utils/os_thread.c \
	../../../../src/utils/path2d.c \
	../../../../src/utils/path2d_stroker.c \
	../../../../src/utils/sha1.c \
	../../../../src/utils/token.c \
	../../../../src/utils/uni_bidi.c \
	../../../../src/utils/unicode.c \
	../../../../src/utils/url.c \
	../../../../src/utils/utf.c \
	../../../../src/utils/xml_parser.c \
	../../../../src/utils/zutil.c

include $(BUILD_SHARED_LIBRARY)
