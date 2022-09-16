include ../config.mak

vpath %.c $(SRC_PATH)/src

#set libgpac cflags and ldflags before opt flags/libs since local includes/libs are first in LIBGPAC_CFLAGS and LIBGPAC_LDFLAGS
CFLAGS=-I"$(SRC_PATH)/include" $(LIBGPAC_CFLAGS) $(OPTFLAGS)

ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

## libgpac objects gathering: src/utils
LIBGPAC_UTILS=utils/os_divers.o utils/os_file.o utils/list.o utils/bitstream.o utils/constants.o utils/error.o utils/alloc.o utils/url.o utils/configfile.o utils/gltools.o utils/gzio.o utils/xml_parser.o
LIBGPAC_UTILS+=utils/sha1.o utils/base_encoding.o utils/math.o utils/os_net.o utils/os_thread.o utils/os_config_init.o utils/cache.o utils/downloader.o utils/utf.o utils/token.o utils/color.o utils/Remotery.o

ifeq ($(DISABLE_PLAYER),no)
LIBGPAC_UTILS+=utils/uni_bidi.o utils/unicode.o
endif
LIBGPAC_UTILS+=utils/os_module.o utils/module.o utils/path2d.o utils/path2d_stroker.o

## libgpac objects gathering: src/ietf
LIBGPAC_IETF=
ifeq ($(DISABLE_STREAMING),no)
LIBGPAC_IETF=ietf/rtcp.o ietf/rtp.o ietf/rtp_packetizer.o ietf/rtp_pck_3gpp.o ietf/rtp_pck_mpeg12.o ietf/rtp_pck_mpeg4.o ietf/rtsp_command.o ietf/rtsp_common.o ietf/rtsp_response.o ietf/rtsp_session.o ietf/sdp.o ietf/rtp_depacketizer.o ietf/rtp_streamer.o
endif

## libgpac objects gathering: src/bifs
LIBGPAC_BIFS=
ifeq ($(DISABLE_BIFS),no)
LIBGPAC_BIFS=bifs/bifs_codec.o bifs/bifs_node_tables.o bifs/com_dec.o bifs/com_enc.o bifs/conditional.o bifs/field_decode.o bifs/field_encode.o bifs/memory_decoder.o bifs/quantize.o bifs/script_dec.o bifs/script_enc.o bifs/unquantize.o
#LIBGPAC_BIFS+=bifs/arith_decoder.o bifs/predictive_mffield.o
endif

## libgpac objects gathering: src/isomedia
LIBGPAC_ISOM=isomedia/avc_ext.o isomedia/box_code_3gpp.o isomedia/box_code_apple.o isomedia/box_code_base.o isomedia/box_code_drm.o isomedia/box_code_meta.o isomedia/box_dump.o isomedia/box_funcs.o isomedia/data_map.o isomedia/drm_sample.o isomedia/isom_intern.o isomedia/isom_read.o isomedia/isom_store.o isomedia/isom_write.o isomedia/media.o isomedia/media_odf.o isomedia/meta.o isomedia/movie_fragments.o isomedia/sample_descs.o isomedia/stbl_read.o isomedia/stbl_write.o isomedia/track.o isomedia/tx3g.o isomedia/iff.o
ifeq ($(DISABLE_ISOFF_HINT),no)
LIBGPAC_ISOM+=isomedia/hint_track.o isomedia/hinting.o
endif
ifeq ($(DISABLE_ISOM_ADOBE),no)
LIBGPAC_ISOM+=isomedia/box_code_adobe.o
endif


## libgpac objects gathering: src/odf
LIBGPAC_ODF=odf/desc_private.o odf/descriptors.o odf/odf_code.o odf/odf_codec.o odf/odf_command.o odf/slc.o
ifeq ($(MINIMAL_OD),no)
#LIBGPAC_ODF+=odf/qos.o odf/ipmpx_code.o odf/oci_codec.o

ifeq ($(DISABLE_OD_DUMP),no)
#LIBGPAC_ODF+=odf/ipmpx_dump.o
endif

ifeq ($(DISABLE_OD_PARSE),no)
#LIBGPAC_ODF+=odf/ipmpx_parse.o
endif

endif

ifeq ($(DISABLE_OD_DUMP),no)
LIBGPAC_ODF+=odf/odf_dump.o
endif
ifeq ($(DISABLE_OD_PARSE),no)
LIBGPAC_ODF+=odf/odf_parse.o
endif

## libgpac objects gathering: src/scenegraph
LIBGPAC_SCENE=scenegraph/base_scenegraph.o scenegraph/mpeg4_animators.o scenegraph/commands.o scenegraph/mpeg4_nodes.o scenegraph/mpeg4_valuator.o scenegraph/vrml_interpolators.o scenegraph/vrml_proto.o scenegraph/vrml_route.o scenegraph/vrml_script.o scenegraph/vrml_js.o scenegraph/vrml_tools.o scenegraph/x3d_nodes.o scenegraph/svg_attributes.o scenegraph/svg_types.o scenegraph/svg_js.o scenegraph/smil_anim.o scenegraph/smil_timing.o scenegraph/svg_properties.o scenegraph/dom_events.o  scenegraph/dom_js.o scenegraph/xml_ns.o

## libgpac objects gathering: src/crypto
LIBGPAC_CRYPTO=
ifeq ($(DISABLE_CRYPTO),no)
LIBGPAC_CRYPTO+=crypto/g_crypt.o
ifeq ($(HAS_OPENSSL), no)
LIBGPAC_CRYPTO+=crypto/g_crypt_tinyaes.o crypto/tiny_aes.o
else
LIBGPAC_CRYPTO+=crypto/g_crypt_openssl.o
endif
endif

LIBGPAC_EVG=evg/ftgrays.o evg/raster3d.o evg/raster_565.o evg/raster_argb.o evg/raster_rgb.o evg/raster_yuv.o evg/stencil.o evg/surface.o

## libgpac objects gathering: src/media tools
LIBGPAC_MEDIATOOLS=media_tools/isom_tools.o media_tools/dash_segmenter.o media_tools/av_parsers.o media_tools/route_dmx.o

ifeq ($(DISABLE_AV_PARSERS),no)
LIBGPAC_MEDIATOOLS+=media_tools/img.o
endif
ifeq ($(DISABLE_MEDIA_IMPORT),no)
LIBGPAC_MEDIATOOLS+=media_tools/media_import.o
endif
ifeq ($(DISABLE_M2TS),no)
LIBGPAC_MEDIATOOLS+=media_tools/mpegts.o
endif
ifeq ($(DISABLE_MPD),no)
LIBGPAC_MEDIATOOLS+=media_tools/m3u8.o media_tools/mpd.o
endif
ifeq ($(DISABLE_DASH_CLIENT),no)
LIBGPAC_MEDIATOOLS+=media_tools/dash_client.o
endif
ifeq ($(DISABLE_MEDIA_EXPORT),no)
LIBGPAC_MEDIATOOLS+=media_tools/media_export.o
endif
ifeq ($(DISABLE_M2TS_MUX),no)
LIBGPAC_MEDIATOOLS+=media_tools/m2ts_mux.o
endif
ifeq ($(DISABLE_DVBX),no)
LIBGPAC_MEDIATOOLS+=media_tools/ait.o media_tools/dsmcc.o media_tools/dvb_mpe.o media_tools/reedsolomon.o
endif
ifeq ($(DISABLE_AVILIB),no)
LIBGPAC_MEDIATOOLS+=media_tools/avilib.o
endif
ifeq ($(DISABLE_M2PS),no)
LIBGPAC_MEDIATOOLS+=media_tools/mpeg2_ps.o
endif
ifeq ($(DISABLE_OGG),no)
LIBGPAC_MEDIATOOLS+=media_tools/gpac_ogg.o
endif
ifeq ($(DISABLE_CRYPTO),no)
LIBGPAC_MEDIATOOLS+=media_tools/crypt_tools.o
endif
ifeq ($(DISABLE_ISOFF_HINT),no)
LIBGPAC_MEDIATOOLS+=media_tools/isom_hinter.o
else
ifeq ($(DISABLE_STREAMING),no)
LIBGPAC_MEDIATOOLS+=media_tools/isom_hinter.o
endif
endif
ifeq ($(DISABLE_SAF),no)
LIBGPAC_MEDIATOOLS+=media_tools/saf.o
endif
ifeq ($(DISABLE_VOBSUB),no)
LIBGPAC_MEDIATOOLS+=media_tools/vobsub.o
endif

LIBGPAC_MEDIATOOLS+=media_tools/webvtt.o

LIBGPAC_FILTERS=filter_core/filter_pck.o filter_core/filter_pid.o filter_core/filter_props.o filter_core/filter_queue.o filter_core/filter_session.o filter_core/filter_register.o filter_core/filter.o filter_core/filter_session_js.o

LIBGPAC_QUICKJS=
LIBGPAC_JSMODS=
JSMODS_CFLAGS=
ifeq ($(CONFIG_JS),yes)
LIBGPAC_QUICKJS=quickjs/cutils.o quickjs/libbf.o quickjs/libregexp.o quickjs/libunicode.o quickjs/quickjs.o quickjs/quickjs-libc.o
LIBGPAC_JSMODS+=jsmods/core.o jsmods/evg.o jsmods/scene_js.o jsmods/storage.o jsmods/webgl.o jsmods/xhr.o
endif



## libgpac objects gathering: src/scene_manager
LIBGPAC_SCENEMANAGER=
ifeq ($(DISABLE_SMGR),no)
LIBGPAC_SCENEMANAGER+=scene_manager/scene_manager.o scene_manager/text_to_bifs.o
endif
ifeq ($(DISABLE_LOADER_BT),no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_bt.o
endif
ifeq ($(DISABLE_LOADER_XMT),no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_xmt.o
endif
ifeq ($(DISABLE_LOADER_ISOFF),no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_isom.o
endif
ifeq ($(DISABLE_LOADER_QTVR),no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_qt.o
endif
ifeq ($(DISABLE_SVG),no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_svg.o
endif
ifeq ($(DISABLE_LOADER_SWF),no)
LIBGPAC_SCENEMANAGER+=scene_manager/swf_parse.o scene_manager/swf_bifs.o scene_manager/swf_svg.o
endif
ifeq ($(DISABLE_SCENE_DUMP),no)
LIBGPAC_SCENEMANAGER+=scene_manager/scene_dump.o
endif
ifeq ($(DISABLE_SCENE_STATS),no)
LIBGPAC_SCENEMANAGER+=scene_manager/scene_stats.o
endif
ifeq ($(DISABLE_SENG),no)
LIBGPAC_SCENEMANAGER+=scene_manager/scene_engine.o
endif
ifeq ($(DISABLE_SCENE_ENCODE),no)
LIBGPAC_SCENEMANAGER+=scene_manager/encode_isom.o
endif

## libgpac objects gathering: src/compositor
LIBGPAC_COMPOSITOR=compositor/audio_input.o compositor/audio_mixer.o compositor/audio_render.o compositor/bindable.o compositor/camera.o compositor/compositor.o compositor/compositor_2d.o compositor/compositor_3d.o compositor/compositor_node_init.o compositor/drawable.o compositor/events.o compositor/font_engine.o compositor/hardcoded_protos.o compositor/mesh.o compositor/mesh_collide.o compositor/mesh_tesselate.o compositor/mpeg4_animstream.o compositor/mpeg4_audio.o compositor/mpeg4_background.o compositor/mpeg4_background2d.o compositor/mpeg4_bitmap.o compositor/mpeg4_composite.o compositor/mpeg4_form.o compositor/mpeg4_geometry_2d.o compositor/mpeg4_geometry_3d.o compositor/mpeg4_geometry_ifs2d.o compositor/mpeg4_geometry_ils2d.o compositor/mpeg4_gradients.o compositor/mpeg4_grouping.o compositor/mpeg4_grouping_2d.o compositor/mpeg4_grouping_3d.o compositor/mpeg4_layer_2d.o compositor/mpeg4_layer_3d.o compositor/mpeg4_layout.o compositor/mpeg4_lighting.o compositor/mpeg4_path_layout.o compositor/mpeg4_sensors.o compositor/mpeg4_sound.o compositor/mpeg4_text.o compositor/mpeg4_textures.o compositor/mpeg4_timesensor.o compositor/mpeg4_viewport.o compositor/navigate.o compositor/offscreen_cache.o compositor/svg_base.o compositor/svg_font.o compositor/svg_geometry.o compositor/svg_grouping.o compositor/svg_media.o compositor/svg_paint_servers.o compositor/svg_text.o compositor/texturing.o compositor/texturing_gl.o compositor/visual_manager.o compositor/visual_manager_2d.o compositor/visual_manager_2d_draw.o compositor/visual_manager_3d.o compositor/visual_manager_3d_gl.o compositor/x3d_geometry.o compositor/clock.o compositor/mpeg4_inputsensor.o compositor/mpeg4_mediacontrol.o compositor/media_object.o compositor/mpeg4_mediasensor.o compositor/mpeg4_inline.o compositor/scene_ns.o compositor/object_manager.o compositor/scene.o compositor/svg_external.o compositor/scene_node_init.o
#LIBGPAC_COMPOSITOR+=compositor/hc_flash_shape.o compositor/svg_filters.o

ifeq ($(DISABLE_PLAYER),yes)
LIBGPAC_COMPOSITOR=compositor/font_engine.o
endif
ifeq ($(DISABLE_SCENEGRAPH),yes)
LIBGPAC_SCENE=scenegraph/vrml_js.o
endif

ifeq ($(DISABLE_EVG),yes)
LIBGPAC_EVG=
endif


## libgpac objects gathering: src/laser
LIBGPAC_LASER=
ifeq ($(DISABLE_LASER),no)
LIBGPAC_LASER=laser/lsr_enc.o laser/lsr_dec.o laser/lsr_tables.o
endif


SCENEGRAPH_CFLAGS=
MEDIATOOLS_CFLAGS=

LIBGPAC_FILTERS+=filters/bs_agg.o filters/bs_split.o filters/bsrw.o filters/compose.o filters/dasher.o filters/dec_ac52.o filters/dec_bifs.o filters/dec_faad.o filters/dec_img.o filters/dec_j2k.o filters/dec_laser.o filters/dec_mad.o filters/dec_mediacodec.o filters/dec_nvdec.o filters/dec_nvdec_sdk.o filters/dec_odf.o filters/dec_theora.o filters/dec_ttml.o filters/dec_ttxt.o filters/dec_vorbis.o filters/dec_vtb.o filters/dec_webvtt.o filters/dec_xvid.o filters/decrypt_cenc_isma.o filters/dmx_avi.o filters/dmx_dash.o filters/dmx_gsf.o filters/dmx_m2ts.o filters/dmx_mpegps.o filters/dmx_nhml.o filters/dmx_nhnt.o filters/dmx_ogg.o filters/dmx_saf.o filters/dmx_vobsub.o filters/enc_jpg.o filters/enc_png.o filters/encrypt_cenc_isma.o filters/filelist.o filters/hevcmerge.o filters/hevcsplit.o filters/in_dvb4linux.o filters/in_file.o filters/in_http.o filters/in_pipe.o filters/in_route.o filters/in_rtp.o filters/in_rtp_rtsp.o filters/in_rtp_sdp.o filters/in_rtp_signaling.o filters/in_rtp_stream.o filters/in_sock.o filters/inspect.o filters/io_fcryp.o filters/isoffin_load.o filters/isoffin_read.o filters/isoffin_read_ch.o filters/jsfilter.o filters/load_bt_xmt.o filters/load_svg.o filters/load_text.o filters/mux_avi.o filters/mux_gsf.o filters/mux_isom.o filters/mux_ts.o filters/mux_ogg.o filters/out_audio.o  filters/out_file.o filters/out_http.o filters/out_pipe.o filters/out_route.o filters/out_rtp.o filters/out_rtsp.o filters/out_sock.o filters/out_video.o filters/reframer.o filters/reframe_ac3.o filters/reframe_adts.o filters/reframe_latm.o filters/reframe_amr.o filters/reframe_av1.o filters/reframe_flac.o filters/reframe_h263.o filters/reframe_img.o filters/reframe_mhas.o filters/reframe_mp3.o filters/reframe_mpgvid.o filters/reframe_nalu.o filters/reframe_prores.o filters/reframe_qcp.o filters/reframe_rawvid.o filters/reframe_rawpcm.o filters/reframe_truehd.o filters/resample_audio.o filters/restamp.o  filters/tileagg.o filters/tilesplit.o filters/tssplit.o filters/ttml_conv.o filters/unit_test_filter.o filters/rewind.o filters/rewrite_adts.o filters/rewrite_mhas.o filters/rewrite_mp4v.o filters/rewrite_nalu.o filters/rewrite_obu.o filters/vflip.o filters/vcrop.o filters/write_generic.o filters/write_nhml.o filters/write_nhnt.o filters/write_qcp.o filters/write_tx3g.o filters/write_vtt.o ../modules/dektec_out/dektec_video_decl.o filters/dec_opensvc.o
LIBGPAC_FILTERS_FFMPEG=filters/ff_common.o filters/ff_avf.o filters/ff_dec.o filters/ff_dmx.o filters/ff_enc.o filters/ff_rescale.o filters/ff_mx.o

ifeq ($(STATIC_MODULES),yes)
LIBGPAC_FILTERS_OHEVC=filters/dec_openhevc.o
FILTERS_OHEVC_CFLAGS=$(openhevc_cflags)
endif

## libgpac objects gathering:
OBJS=$(LIBGPAC_UTILS) $(LIBGPAC_CRYPTO) $(LIBGPAC_SCENE) $(LIBGPAC_IETF) $(LIBGPAC_BIFS) $(LIBGPAC_ISOM) $(LIBGPAC_ODF) $(LIBGPAC_MEDIATOOLS) $(LIBGPAC_SCENEMANAGER) $(LIBGPAC_COMPOSITOR) $(LIBGPAC_LASER) $(LIBGPAC_EVG) $(LIBGPAC_FILTERS) $(LIBGPAC_FILTERS_FFMPEG) $(LIBGPAC_FILTERS_OHEVC) $(LIBGPAC_QUICKJS) $(LIBGPAC_JSMODS)


##include static modules and other deps for libgpac
include ../static.mak

#todo, we may need to trick ld for library path...
EXTRALIBS+=$(zlib_ldflags) $(opensvc_ldflags) $(ssl_ldflags) $(jpeg_ldflags) $(openjpeg_ldflags) $(png_ldflags) $(mad_ldflags) $(a52_ldflags) $(xvid_ldflags) $(faad_ldflags)
EXTRALIBS+=$(ffmpeg_ldflags) $(ogg_ldflags) $(vorbis_ldflags) $(theora_ldflags) $(nghttp2_ldflags) $(vtb_ldflags)


##libgpac library output
LIB=libgpac$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LDFLAGS+=-Wl,-out-implib=../bin/gcc/libgpac.dll.a
else
ifeq ($(CONFIG_DARWIN),yes)
LDFLAGS+=-dynamiclib -install_name $(prefix)/lib/$(LIB)
endif
EXTRALIBS+=$(GPAC_SH_FLAGS)
endif

LD_SONAME="-Wl,-soname,libgpac.so.$(VERSION_MAJOR)"
ifeq ($(CONFIG_DARWIN),yes)
LD_SONAME=
endif

ifeq ($(CONFIG_SUNOS),yes)
LD_SONAME="-Wl,-h,$(LIB)"
EXTRALIBS+= -lrt
endif

ALL_LIBS=

ALL_LIBS+=$(LIBGPAC_LDFLAGS) $(EXTRALIBS)

ifeq ($(STATIC_MODULES),yes)
ifneq ($(CONFIG_OPENHEVC),no)
ALL_LIBS+=$(openhevc_ldflags)
endif
endif

SRCS := $(OBJS:.o=.c)

all: lib

lib: ../bin/gcc/$(LIB)

crypto: $(LIBGPAC_CRYPTO)

scenegraph: CFLAGS += $(SCENEGRAPH_CFLAGS)
scenegraph: $(LIBGPAC_SCENE)
.deps/scenegraph/%: CFLAGS += $(SCENEGRAPH_CFLAGS)

media_tools: CFLAGS += $(MEDIATOOLS_CFLAGS)
media_tools: $(LIBGPAC_MEDIATOOLS)
.deps/media_tools/%: CFLAGS += $(MEDIATOOLS_CFLAGS)

compositor: CFLAGS+=$(COMPOSITOR_CFLAGS)
compositor: $(LIBGPAC_COMPOSITOR)
.deps/compositor/%: CFLAGS += $(COMPOSITOR_CFLAGS)

filters: CFLAGS += $(FILTERS_CFLAGS)
filters: $(LIBGPAC_FILTERS)
.deps/filters/%: CFLAGS += $(FILTERS_CFLAGS)

filters_ffmpeg: CFLAGS += $(FILTERS_CFLAGS) $(ffmpeg_cflags)
filters_ffmpeg: $(LIBGPAC_FILTERS_FFMPEG)
.deps/filters/%: CFLAGS += $(FILTERS_CFLAGS) $(ffmpeg_cflags)


filters_ohevc: CFLAGS += $(FILTERS_OHEVC_CFLAGS)
filters_ohevc: $(LIBGPAC_FILTERS_OHEVC)
.deps/filters/dec_openhevc.dep: CFLAGS += $(FILTERS_OHEVC_CFLAGS)

jsmods: CFLAGS += $(JSMODS_CFLAGS)
jsmods: $(LIBGPAC_JSMODS)
.deps/jsmods/%: CFLAGS += $(JSMODS_CFLAGS)

jsmods/evg.o: CFLAGS += $(ffmpeg_cflags)
.deps/jsmods/evg.%: CFLAGS += $(ffmpeg_cflags)

#set cflags for all ext lib
filters/dec_opensvc.o: CFLAGS += $(opensvc_cflags)
filters/dec_opensvc.o: filters/dec_opensvc.c
.deps/filters/dec_opensvc.dep: CFLAGS += $(opensvc_cflags)

filters/dec_j2k.o: CFLAGS += $(openjpeg_cflags)
filters/dec_j2k.o: filters/dec_j2k.c
.deps/filters/dec_j2k.dep: CFLAGS += $(openjpeg_cflags)

filters/dec_mad.o: CFLAGS += $(mad_cflags)
filters/dec_mad.o: filters/dec_mad.c

filters/dec_ac52.o: CFLAGS += $(a52_cflags)
filters/dec_ac52.o: filters/dec_ac52.c

filters/dec_xvid.o: CFLAGS += $(xvid_cflags)
filters/dec_xvid.o: filters/dec_xvid.c

filters/dec_faad.o: CFLAGS += $(faad_cflags)
filters/dec_faad.o: filters/dec_faad.c

filters/dec_faad.o: CFLAGS += $(faad_cflags)
filters/dec_faad.o: filters/dec_faad.c

filters/dec_theora.o: CFLAGS += $(ogg_cflags) $(theora_cflags)
filters/dec_theora.o: filters/dec_theora.c

filters/dec_vorbis.o: CFLAGS += $(ogg_cflags) $(vorbis_cflags)
filters/dec_vorbis.o: filters/dec_vorbis.c

filters/dec_vtb.o: CFLAGS += $(vtb_cflags)
filters/dec_vtb.o: filters/dec_vtb.c

filters/enc_jpg.o: CFLAGS += $(jpeg_cflags)
filters/enc_jpg.o: filters/enc_jpg.c

filters/enc_png.o: CFLAGS += $(zlib_cflags) $(png_cflags)
filters/enc_png.o: filters/enc_png.c

media_tools/img.o: CFLAGS += $(jpeg_cflags) $(zlib_cflags) $(png_cflags)
media_tools/img.o: media_tools/img.c

utils/downloader.o: CFLAGS += $(nghttp2_cflags) $(ssl_cflags)
utils/downloader.o: utils/downloader.c
.deps/utils/downloader.dep: CFLAGS += $(nghttp2_cflags) $(ssl_cflags)

crypto/g_crypt_openssl.o: CFLAGS += $(ssl_cflags)
crypto/g_crypt_openssl.o: crypto/g_crypt_openssl.c


../bin/gcc/$(LIB): $(LIBGPAC_UTILS) $(LIBGPAC_IETF) $(LIBGPAC_BIFS) $(LIBGPAC_ODF) $(LIBGPAC_LASER) $(LIBGPAC_ISOM) $(LIBGPAC_SCENEMANAGER) compositor scenegraph media_tools crypto filters filters_ffmpeg filters_ohevc jsmods $(OBJS)
#	@echo "OBJS $(OBJS)"
#	@echo "LIBS $(ALL_LIBS)"

ifeq ($(CONFIG_DARWIN),yes)

	$(LIBTOOL) -s -o ../bin/gcc/libgpac_static.a $(OBJS)
	$(RANLIB) ../bin/gcc/libgpac_static.a
ifneq ($(STATIC_BUILD),yes)
	$(CC) $(SHFLAGS) $(LD_SONAME) -o $@ $(OBJS) $(ALL_LIBS) $(LDFLAGS)
endif

else

	$(AR) cr ../bin/gcc/libgpac_static.a $(OBJS)
	$(RANLIB) ../bin/gcc/libgpac_static.a
ifneq ($(STATIC_BUILD),yes)
	$(CC) $(SHFLAGS) $(LD_SONAME) -o $@ $(OBJS) $(ALL_LIBS) $(LDFLAGS)
	mv $@ $@.$(VERSION_SONAME)
	ln -sf $(notdir $@).$(VERSION_SONAME) $@.$(VERSION_MAJOR)
	ln -sf $(notdir $@).$(VERSION_SONAME) $@
endif

endif

ifeq ($(shell fgrep "Libs.private:" ../gpac.pc 1>&2 2> /dev/null ; echo $$?),1)
	@echo "Libs.private: -lgpac_static $(ALL_LIBS)" >> ../gpac.pc
endif

dep:

DEPS := $(SRCS:%.c=.deps/%.dep)

-include .depend

.depend: $(DEPS)
	cat $^ > "$@"

.deps/%.dep: %.c
	@mkdir -p $(dir $@)
	$(CC) -MM $(CFLAGS) "$<" -MT "$*.o" -MF "$@"

clean:
	rm -f $(OBJS) ../bin/gcc/$(LIB)
	rm -rf ../bin/gcc/libgpac_static.a ../bin/gcc/libgpac.so \
		../bin/gcc/libgpac.so.$(VERSION_MAJOR) \
		../bin/gcc/libgpac.so.$(VERSION_SONAME)

distclean: clean
	rm -f Makefile.bak .depend $(DEPS)
