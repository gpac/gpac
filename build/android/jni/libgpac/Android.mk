LOCAL_PATH:= $(call my-dir)
APP_ABI          := armeabi armeabi-v7a

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
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../modules

LOCAL_LDLIBS    += -L$(LOCAL_PATH)/../../../../extra_lib/lib/android/$(TARGET_ARCH_ABI)
LOCAL_LDLIBS    += -lGLESv1_CM -ldl
LOCAL_LDLIBS    += -ljs_osmo -leditline -lft2 -ljpegdroid -lopenjpeg -lpng -lz
#LOCAL_EXPORT_LDLIBS= -ljs_osmo -leditline -lft2 -ljpeg -lopenjpeg -lpng -lz


LOCAL_CFLAGS +=	-DGPAC_HAVE_CONFIG_H
LOCAL_CFLAGS += -DNO_MALLINFO
LOCAL_CFLAGS += -DGPAC_ANDROID
#LOCAL_CFLAGS += -DGPAC_FIXED_POINT

LOCAL_SRC_FILES := \
	../../../../src/compositor/mpeg4_textures.c \
	../../../../src/compositor/navigate.c \
	../../../../src/compositor/texturing_gl.c \
	../../../../src/compositor/svg_paint_servers.c \
	../../../../src/compositor/audio_mixer.c \
	../../../../src/compositor/mpeg4_animstream.c \
	../../../../src/compositor/mpeg4_geometry_ifs2d.c \
	../../../../src/compositor/compositor.c \
	../../../../src/compositor/mesh_collide.c \
	../../../../src/compositor/svg_media.c \
	../../../../src/compositor/mesh_tesselate.c \
	../../../../src/compositor/mpeg4_bitmap.c \
	../../../../src/compositor/camera.c \
	../../../../src/compositor/svg_grouping.c \
	../../../../src/compositor/mpeg4_grouping.c \
	../../../../src/compositor/drawable.c \
	../../../../src/compositor/events.c \
	../../../../src/compositor/offscreen_cache.c \
	../../../../src/compositor/visual_manager.c \
	../../../../src/compositor/mpeg4_sound.c \
	../../../../src/compositor/mpeg4_grouping_3d.c \
	../../../../src/compositor/mpeg4_layer_3d.c \
	../../../../src/compositor/mpeg4_path_layout.c \
	../../../../src/compositor/mpeg4_geometry_2d.c \
	../../../../src/compositor/audio_input.c \
	../../../../src/compositor/compositor_node_init.c \
	../../../../src/compositor/mpeg4_geometry_3d.c \
	../../../../src/compositor/svg_base.c \
	../../../../src/compositor/mpeg4_geometry_ils2d.c \
	../../../../src/compositor/visual_manager_2d.c \
	../../../../src/compositor/mpeg4_sensors.c \
	../../../../src/compositor/mpeg4_form.c \
	../../../../src/compositor/x3d_geometry.c \
	../../../../src/compositor/texturing.c \
	../../../../src/compositor/mpeg4_audio.c \
	../../../../src/compositor/svg_font.c \
	../../../../src/compositor/mpeg4_background.c \
	../../../../src/compositor/audio_render.c \
	../../../../src/compositor/mpeg4_layout.c \
	../../../../src/compositor/mpeg4_grouping_2d.c \
	../../../../src/compositor/mesh.c \
	../../../../src/compositor/mpeg4_layer_2d.c \
	../../../../src/compositor/compositor_2d.c \
	../../../../src/compositor/svg_geometry.c \
	../../../../src/compositor/visual_manager_3d_gl.c \
	../../../../src/compositor/font_engine.c \
	../../../../src/compositor/mpeg4_viewport.c \
	../../../../src/compositor/bindable.c \
	../../../../src/compositor/mpeg4_text.c \
	../../../../src/compositor/mpeg4_timesensor.c \
	../../../../src/compositor/mpeg4_composite.c \
	../../../../src/compositor/visual_manager_2d_draw.c \
	../../../../src/compositor/mpeg4_background2d.c \
	../../../../src/compositor/visual_manager_3d.c \
	../../../../src/compositor/mpeg4_lighting.c \
	../../../../src/compositor/mpeg4_gradients.c \
	../../../../src/compositor/compositor_3d.c \
	../../../../src/compositor/hardcoded_protos.c \
	../../../../src/compositor/svg_text.c \
	../../../../src/compositor/hc_flash_shape.c \
	../../../../src/compositor/svg_filters.c \
	../../../../src/media_tools/avilib.c \
	../../../../src/media_tools/filestreamer.c \
	../../../../src/media_tools/isom_tools.c \
	../../../../src/media_tools/dash_segmenter.c \
	../../../../src/media_tools/mpeg2_ps.c \
	../../../../src/media_tools/vobsub.c \
	../../../../src/media_tools/media_import.c \
	../../../../src/media_tools/text_import.c \
	../../../../src/media_tools/webvtt.c \
	../../../../src/media_tools/ismacryp.c \
	../../../../src/media_tools/img.c \
	../../../../src/media_tools/mpegts.c \
	../../../../src/media_tools/saf.c \
	../../../../src/media_tools/av_parsers.c \
	../../../../src/media_tools/isom_hinter.c \
	../../../../src/media_tools/gpac_ogg.c \
	../../../../src/media_tools/media_export.c \
	../../../../src/media_tools/dvb_mpe.c \
	../../../../src/media_tools/m2ts_mux.c \
	../../../../src/media_tools/reedsolomon.c \
	../../../../src/media_tools/dash_client.c \
	../../../../src/media_tools/mpd.c \
	../../../../src/media_tools/m3u8.c \
	../../../../src/media_tools/ait.c \
	../../../../src/media_tools/dsmcc.c \
	../../../../src/media_tools/html5_media.c \
	../../../../src/media_tools/html5_mse.c \
	../../../../src/laser/lsr_tables.c \
	../../../../src/laser/lsr_dec.c \
	../../../../src/laser/lsr_enc.c \
	../../../../src/scene_manager/scene_dump.c \
	../../../../src/scene_manager/loader_bt.c \
	../../../../src/scene_manager/loader_qt.c \
	../../../../src/scene_manager/encode_isom.c \
	../../../../src/scene_manager/swf_parse.c \
	../../../../src/scene_manager/scene_engine.c \
	../../../../src/scene_manager/scene_stats.c \
	../../../../src/scene_manager/scene_manager.c \
	../../../../src/scene_manager/text_to_bifs.c \
	../../../../src/scene_manager/loader_isom.c \
	../../../../src/scene_manager/swf_bifs.c \
	../../../../src/scene_manager/swf_svg.c \
	../../../../src/scene_manager/loader_xmt.c \
	../../../../src/scene_manager/loader_svg.c \
	../../../../src/utils/alloc.c \
	../../../../src/utils/os_net.c \
	../../../../src/utils/path2d_stroker.c \
	../../../../src/utils/zutil.c \
	../../../../src/utils/os_divers.c \
	../../../../src/utils/os_file.c \
	../../../../src/utils/path2d.c \
	../../../../src/utils/base_encoding.c \
	../../../../src/utils/module.c \
	../../../../src/utils/uni_bidi.c \
	../../../../src/utils/dlmalloc.c \
	../../../../src/utils/math.c \
	../../../../src/utils/xml_parser.c \
	../../../../src/utils/os_module.c \
	../../../../src/utils/url.c \
	../../../../src/utils/downloader.c \
	../../../../src/utils/list.c \
	../../../../src/utils/error.c \
	../../../../src/utils/bitstream.c \
	../../../../src/utils/color.c \
	../../../../src/utils/token.c \
	../../../../src/utils/configfile.c \
	../../../../src/utils/utf.c \
	../../../../src/utils/os_thread.c \
	../../../../src/utils/cache.c \
	../../../../src/utils/sha1.c \
	../../../../src/bifs/predictive_mffield.c \
	../../../../src/bifs/script_dec.c \
	../../../../src/bifs/memory_decoder.c \
	../../../../src/bifs/unquantize.c \
	../../../../src/bifs/script_enc.c \
	../../../../src/bifs/field_encode.c \
	../../../../src/bifs/conditional.c \
	../../../../src/bifs/bifs_node_tables.c \
	../../../../src/bifs/arith_decoder.c \
	../../../../src/bifs/field_decode.c \
	../../../../src/bifs/com_enc.c \
	../../../../src/bifs/quantize.c \
	../../../../src/bifs/bifs_codec.c \
	../../../../src/bifs/com_dec.c \
	../../../../src/ietf/rtsp_common.c \
	../../../../src/ietf/sdp.c \
	../../../../src/ietf/rtcp.c \
	../../../../src/ietf/rtsp_command.c \
	../../../../src/ietf/rtsp_session.c \
	../../../../src/ietf/rtp_pck_3gpp.c \
	../../../../src/ietf/rtp_pck_mpeg12.c \
	../../../../src/ietf/rtp_pck_mpeg4.c \
	../../../../src/ietf/rtsp_response.c \
	../../../../src/ietf/rtp_depacketizer.c \
	../../../../src/ietf/rtp_streamer.c \
	../../../../src/ietf/rtp.c \
	../../../../src/ietf/rtp_packetizer.c \
	../../../../src/isomedia/avc_ext.c \
	../../../../src/isomedia/box_dump.c \
	../../../../src/isomedia/box_code_drm.c \
	../../../../src/isomedia/track.c \
	../../../../src/isomedia/box_funcs.c \
	../../../../src/isomedia/isom_read.c \
	../../../../src/isomedia/box_code_meta.c \
	../../../../src/isomedia/box_code_base.c \
	../../../../src/isomedia/box_code_adobe.c \
	../../../../src/isomedia/box_code_apple.c \
	../../../../src/isomedia/sample_descs.c \
	../../../../src/isomedia/meta.c \
	../../../../src/isomedia/stbl_read.c \
	../../../../src/isomedia/box_code_3gpp.c \
	../../../../src/isomedia/isom_intern.c \
	../../../../src/isomedia/isom_write.c \
	../../../../src/isomedia/drm_sample.c \
	../../../../src/isomedia/ttml.c \
	../../../../src/isomedia/tx3g.c \
	../../../../src/isomedia/hint_track.c \
	../../../../src/isomedia/stbl_write.c \
	../../../../src/isomedia/data_map.c \
	../../../../src/isomedia/media.c \
	../../../../src/isomedia/hinting.c \
	../../../../src/isomedia/isom_store.c \
	../../../../src/isomedia/movie_fragments.c \
	../../../../src/isomedia/media_odf.c \
	../../../../src/mcrypt/ofb.c \
	../../../../src/mcrypt/tripledes.c \
	../../../../src/mcrypt/cfb.c \
	../../../../src/mcrypt/stream.c \
	../../../../src/mcrypt/rijndael-256.c \
	../../../../src/mcrypt/ncfb.c \
	../../../../src/mcrypt/rijndael-192.c \
	../../../../src/mcrypt/ctr.c \
	../../../../src/mcrypt/nofb.c \
	../../../../src/mcrypt/des.c \
	../../../../src/mcrypt/g_crypt.c \
	../../../../src/mcrypt/ecb.c \
	../../../../src/mcrypt/cbc.c \
	../../../../src/mcrypt/rijndael-128.c \
	../../../../src/terminal/scene.c \
	../../../../src/terminal/terminal.c \
	../../../../src/terminal/network_service.c \
	../../../../src/terminal/input_sensor.c \
	../../../../src/terminal/media_sensor.c \
	../../../../src/terminal/media_object.c \
	../../../../src/terminal/channel.c \
	../../../../src/terminal/term_node_init.c \
	../../../../src/terminal/object_browser.c \
	../../../../src/terminal/mpeg4_inline.c \
	../../../../src/terminal/decoder.c \
	../../../../src/terminal/media_manager.c \
	../../../../src/terminal/media_memory.c \
	../../../../src/terminal/clock.c \
	../../../../src/terminal/svg_external.c \
	../../../../src/terminal/media_control.c \
	../../../../src/terminal/object_manager.c \
	../../../../src/scenegraph/xml_ns.c \
	../../../../src/scenegraph/commands.c \
	../../../../src/scenegraph/vrml_tools.c \
	../../../../src/scenegraph/vrml_route.c \
	../../../../src/scenegraph/dom_events.c \
	../../../../src/scenegraph/smil_anim.c \
	../../../../src/scenegraph/mpeg4_valuator.c \
	../../../../src/scenegraph/svg_smjs.c \
	../../../../src/scenegraph/vrml_interpolators.c \
	../../../../src/scenegraph/svg_attributes.c \
	../../../../src/scenegraph/mpeg4_nodes.c \
	../../../../src/scenegraph/vrml_smjs.c \
	../../../../src/scenegraph/mpeg4_animators.c \
	../../../../src/scenegraph/dom_smjs.c \
	../../../../src/scenegraph/svg_properties.c \
	../../../../src/scenegraph/xbl_process.c \
	../../../../src/scenegraph/svg_types.c \
	../../../../src/scenegraph/x3d_nodes.c \
	../../../../src/scenegraph/base_scenegraph.c \
	../../../../src/scenegraph/smil_timing.c \
	../../../../src/scenegraph/vrml_script.c \
	../../../../src/scenegraph/vrml_proto.c \
	../../../../src/scenegraph/html5_media_smjs.c \
	../../../../src/scenegraph/html5_mse_smjs.c \
	../../../../src/odf/ipmpx_dump.c \
	../../../../src/odf/odf_code.c \
	../../../../src/odf/desc_private.c \
	../../../../src/odf/slc.c \
	../../../../src/odf/odf_codec.c \
	../../../../src/odf/qos.c \
	../../../../src/odf/ipmpx_parse.c \
	../../../../src/odf/ipmpx_code.c \
	../../../../src/odf/odf_dump.c \
	../../../../src/odf/odf_parse.c \
	../../../../src/odf/odf_command.c \
	../../../../src/odf/oci_codec.c \
	../../../../src/odf/descriptors.c

include $(BUILD_SHARED_LIBRARY)
