include ../config.mak

vpath %.c $(SRC_PATH)/src

CFLAGS= $(OPTFLAGS) -I"$(SRC_PATH)/include"

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

## libgpac objects gathering: src/utils
LIBGPAC_UTILS=utils/os_divers.o utils/list.o utils/bitstream.o utils/error.o utils/alloc.o utils/url.o utils/configfile.o 
ifeq ($(DISABLE_CORE_TOOLS), no)
LIBGPAC_UTILS+=utils/sha1.o  utils/base_encoding.o utils/os_net.o utils/os_thread.o utils/os_config_init.o utils/cache.o utils/downloader.o utils/xml_parser.o utils/utf.o utils/token.o 
endif

ifeq ($(DISABLE_PLAYER), no)
LIBGPAC_UTILS+=utils/color.o utils/os_module.o utils/math.o utils/path2d.o utils/path2d_stroker.o utils/module.o utils/uni_bidi.o utils/ringbuffer.o utils/unicode.o utils/map.o 
endif

## libgpac objects gathering: src/ietf
LIBGPAC_IETF=
ifeq ($(DISABLE_STREAMING), no)
LIBGPAC_IETF=ietf/rtcp.o ietf/rtp.o ietf/rtp_packetizer.o ietf/rtp_pck_3gpp.o ietf/rtp_pck_mpeg12.o ietf/rtp_pck_mpeg4.o ietf/rtsp_command.o ietf/rtsp_common.o ietf/rtsp_response.o ietf/rtsp_session.o ietf/sdp.o ietf/rtp_depacketizer.o ietf/rtp_streamer.o
endif

## libgpac objects gathering: src/bifs
LIBGPAC_BIFS=
ifeq ($(DISABLE_BIFS), no)
LIBGPAC_BIFS=bifs/arith_decoder.o bifs/bifs_codec.o bifs/bifs_node_tables.o bifs/com_dec.o bifs/com_enc.o bifs/conditional.o bifs/field_decode.o bifs/field_encode.o bifs/memory_decoder.o bifs/predictive_mffield.o bifs/quantize.o bifs/script_dec.o bifs/script_enc.o bifs/unquantize.o
endif

## libgpac objects gathering: src/isomedia
LIBGPAC_ISOM=isomedia/avc_ext.o isomedia/box_code_3gpp.o isomedia/box_code_apple.o isomedia/box_code_base.o isomedia/box_code_drm.o isomedia/box_code_meta.o isomedia/box_dump.o isomedia/box_funcs.o isomedia/data_map.o isomedia/drm_sample.o isomedia/isom_intern.o isomedia/isom_read.o isomedia/isom_store.o isomedia/isom_write.o isomedia/media.o isomedia/media_odf.o isomedia/meta.o isomedia/movie_fragments.o isomedia/sample_descs.o isomedia/stbl_read.o isomedia/stbl_write.o isomedia/track.o isomedia/tx3g.o isomedia/ttml.o 
ifeq ($(DISABLE_ISOFF_HINT), no)
LIBGPAC_ISOM+=isomedia/hint_track.o isomedia/hinting.o
endif
ifeq ($(DISABLE_ISOM_ADOBE), no)
LIBGPAC_ISOM+=isomedia/box_code_adobe.o
endif

## libgpac objects gathering: src/odf
LIBGPAC_ODF=odf/desc_private.o odf/descriptors.o odf/odf_code.o odf/odf_codec.o odf/odf_command.o odf/qos.o odf/slc.o 
ifeq ($(MINIMAL_OD), no)
LIBGPAC_ODF+=odf/ipmpx_code.o odf/oci_codec.o 

ifeq ($(DISABLE_OD_DUMP), no)
LIBGPAC_ODF+=odf/ipmpx_dump.o
endif

ifeq ($(DISABLE_OD_PARSE), no)
LIBGPAC_ODF+=odf/ipmpx_parse.o
endif

endif

ifeq ($(DISABLE_OD_DUMP), no)
LIBGPAC_ODF+=odf/odf_dump.o
endif
ifeq ($(DISABLE_OD_PARSE), no)
LIBGPAC_ODF+=odf/odf_parse.o
endif

## libgpac objects gathering: src/scenegraph
LIBGPAC_SCENE=scenegraph/base_scenegraph.o scenegraph/mpeg4_animators.o scenegraph/commands.o scenegraph/mpeg4_nodes.o scenegraph/mpeg4_valuator.o scenegraph/vrml_interpolators.o scenegraph/vrml_proto.o scenegraph/vrml_route.o scenegraph/vrml_script.o scenegraph/vrml_smjs.o scenegraph/vrml_tools.o scenegraph/x3d_nodes.o scenegraph/svg_attributes.o scenegraph/svg_types.o scenegraph/svg_smjs.o scenegraph/smil_anim.o scenegraph/smil_timing.o scenegraph/svg_properties.o scenegraph/dom_events.o  scenegraph/dom_smjs.o scenegraph/xbl_process.o scenegraph/xml_ns.o scenegraph/html5_media_smjs.o scenegraph/html5_mse_smjs.o scenegraph/webvtt_smjs.o

## libgpac objects gathering: src/mcrypt
LIBGPAC_MCRYPT=
ifeq ($(DISABLE_MCRYPT), no)
LIBGPAC_MCRYPT+=mcrypt/cbc.o mcrypt/cfb.o mcrypt/ctr.o mcrypt/des.o mcrypt/ecb.o mcrypt/g_crypt.o mcrypt/ncfb.o mcrypt/nofb.o mcrypt/ofb.o mcrypt/rijndael-128.o mcrypt/rijndael-192.o mcrypt/rijndael-256.o mcrypt/stream.o mcrypt/tripledes.o 
endif

## libgpac objects gathering: src/media tools
LIBGPAC_MEDIATOOLS=media_tools/isom_tools.o media_tools/dash_segmenter.o media_tools/av_parsers.o
ifeq ($(DISABLE_AV_PARSERS), no)
LIBGPAC_MEDIATOOLS+=media_tools/img.o
endif
ifeq ($(DISABLE_MEDIA_IMPORT), no)
LIBGPAC_MEDIATOOLS+=media_tools/media_import.o
endif
ifeq ($(DISABLE_M2TS), no)
LIBGPAC_MEDIATOOLS+=media_tools/mpegts.o
endif
ifeq ($(DISABLE_MPD), no)
LIBGPAC_MEDIATOOLS+=media_tools/m3u8.o media_tools/mpd.o
endif
ifeq ($(DISABLE_DASH_CLIENT), no)
LIBGPAC_MEDIATOOLS+=media_tools/dash_client.o 
endif
ifeq ($(DISABLE_MEDIA_EXPORT), no)
LIBGPAC_MEDIATOOLS+=media_tools/media_export.o
endif
ifeq ($(DISABLE_M2TS_MUX), no)
LIBGPAC_MEDIATOOLS+=media_tools/m2ts_mux.o
endif
ifeq ($(DISABLE_STREAMING), no)
LIBGPAC_MEDIATOOLS+=media_tools/filestreamer.o
endif
ifeq ($(DISABLE_DVBX), no)
LIBGPAC_MEDIATOOLS+=media_tools/ait.o media_tools/dsmcc.o media_tools/dvb_mpe.o media_tools/reedsolomon.o
endif
ifeq ($(DISABLE_AVILIB), no)
LIBGPAC_MEDIATOOLS+=media_tools/avilib.o
endif
ifeq ($(DISABLE_M2PS), no)
LIBGPAC_MEDIATOOLS+=media_tools/mpeg2_ps.o
endif
ifeq ($(DISABLE_OGG), no)
LIBGPAC_MEDIATOOLS+=media_tools/gpac_ogg.o
endif
ifeq ($(DISABLE_MCRYPT), no)
LIBGPAC_MEDIATOOLS+=media_tools/ismacryp.o
endif
ifeq ($(DISABLE_ISOFF_HINT), no)
LIBGPAC_MEDIATOOLS+=media_tools/isom_hinter.o
else
ifeq ($(DISABLE_STREAMING), no)
LIBGPAC_MEDIATOOLS+=media_tools/isom_hinter.o
endif
endif
ifeq ($(DISABLE_SAF), no)
LIBGPAC_MEDIATOOLS+=media_tools/saf.o
endif
ifeq ($(DISABLE_VOBSUB), no)
LIBGPAC_MEDIATOOLS+=media_tools/vobsub.o
endif
ifeq ($(DISABLE_TTXT), no)
LIBGPAC_MEDIATOOLS+=media_tools/text_import.o
endif

ifeq ($(DISABLE_PLAYER), no)
LIBGPAC_MEDIATOOLS+=media_tools/html5_media.o media_tools/html5_mse.o
ifeq ($(DISABLE_TTXT), no)
LIBGPAC_MEDIATOOLS+=media_tools/webvtt.o
endif
endif


## libgpac objects gathering: src/scene_manager
LIBGPAC_SCENEMANAGER=
ifeq ($(DISABLE_SMGR), no)
LIBGPAC_SCENEMANAGER+=scene_manager/scene_manager.o scene_manager/text_to_bifs.o
endif
ifeq ($(DISABLE_LOADER_BT), no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_bt.o
endif
ifeq ($(DISABLE_LOADER_XMT), no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_xmt.o
endif
ifeq ($(DISABLE_LOADER_ISOFF), no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_isom.o
endif
ifeq ($(DISABLE_LOADER_QTVR), no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_qt.o
endif
ifeq ($(DISABLE_SVG), no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_svg.o 
endif
ifeq ($(DISABLE_LOADER_SWF), no)
LIBGPAC_SCENEMANAGER+=scene_manager/swf_parse.o scene_manager/swf_bifs.o scene_manager/swf_svg.o 
endif
ifeq ($(DISABLE_SCENE_DUMP), no)
LIBGPAC_SCENEMANAGER+=scene_manager/scene_dump.o
endif
ifeq ($(DISABLE_SCENE_STATS), no)
LIBGPAC_SCENEMANAGER+=scene_manager/scene_stats.o
endif
ifeq ($(DISABLE_SENG), no)
LIBGPAC_SCENEMANAGER+=scene_manager/scene_engine.o 
endif
ifeq ($(DISABLE_SCENE_ENCODE), no)
LIBGPAC_SCENEMANAGER+=scene_manager/encode_isom.o
endif

## libgpac objects gathering: src/terminal
LIBGPAC_TERMINAL=terminal/channel.o terminal/clock.o terminal/decoder.o terminal/term_node_init.o terminal/input_sensor.o terminal/media_control.o terminal/media_manager.o terminal/media_memory.o terminal/media_object.o terminal/media_sensor.o terminal/mpeg4_inline.o terminal/network_service.o terminal/object_browser.o terminal/object_manager.o terminal/scene.o terminal/terminal.o terminal/svg_external.o

## libgpac objects gathering: src/compositor
LIBGPAC_COMPOSITOR=compositor/audio_input.o compositor/audio_mixer.o compositor/audio_render.o compositor/bindable.o compositor/camera.o compositor/compositor.o compositor/compositor_2d.o compositor/compositor_3d.o compositor/compositor_node_init.o compositor/drawable.o compositor/events.o compositor/font_engine.o compositor/hc_flash_shape.o compositor/hardcoded_protos.o compositor/mesh.o compositor/mesh_collide.o compositor/mesh_tesselate.o compositor/mpeg4_animstream.o compositor/mpeg4_audio.o compositor/mpeg4_background.o compositor/mpeg4_background2d.o compositor/mpeg4_bitmap.o compositor/mpeg4_composite.o compositor/mpeg4_form.o compositor/mpeg4_geometry_2d.o compositor/mpeg4_geometry_3d.o compositor/mpeg4_geometry_ifs2d.o compositor/mpeg4_geometry_ils2d.o compositor/mpeg4_gradients.o compositor/mpeg4_grouping.o compositor/mpeg4_grouping_2d.o compositor/mpeg4_grouping_3d.o compositor/mpeg4_layer_2d.o compositor/mpeg4_layer_3d.o compositor/mpeg4_layout.o compositor/mpeg4_lighting.o compositor/mpeg4_path_layout.o compositor/mpeg4_sensors.o compositor/mpeg4_sound.o compositor/mpeg4_text.o compositor/mpeg4_textures.o compositor/mpeg4_timesensor.o compositor/mpeg4_viewport.o compositor/navigate.o compositor/offscreen_cache.o compositor/svg_base.o compositor/svg_filters.o compositor/svg_font.o compositor/svg_geometry.o compositor/svg_grouping.o compositor/svg_media.o compositor/svg_paint_servers.o compositor/svg_text.o compositor/texturing.o compositor/texturing_gl.o compositor/visual_manager.o compositor/visual_manager_2d.o compositor/visual_manager_2d_draw.o compositor/visual_manager_3d.o compositor/visual_manager_3d_gl.o compositor/x3d_geometry.o

ifeq ($(DISABLE_PLAYER), yes)
LIBGPAC_COMPOSITOR=
LIBGPAC_TERMINAL=
endif
ifeq ($(DISABLE_SCENEGRAPH), yes)
LIBGPAC_SCENE=
endif



## libgpac objects gathering: src/laser
LIBGPAC_LASER=
ifeq ($(DISABLE_LASER), no)
LIBGPAC_LASER=laser/lsr_enc.o laser/lsr_dec.o laser/lsr_tables.o
endif

## libgpac objects gathering: 
OBJS=$(LIBGPAC_UTILS) $(LIBGPAC_MCRYPT) $(LIBGPAC_SCENE) $(LIBGPAC_IETF) $(LIBGPAC_BIFS) $(LIBGPAC_ISOM) $(LIBGPAC_ODF) $(LIBGPAC_MEDIATOOLS) $(LIBGPAC_SCENEMANAGER) $(LIBGPAC_TERMINAL) $(LIBGPAC_COMPOSITOR) $(LIBGPAC_LASER)


# configure static modules
ifeq ($(STATIC_MODULES), yes)

CFLAGS+= -DGPAC_STATIC_MODULES

OBJS+=../modules/aac_in/aac_in.o
ifneq ($(CONFIG_FAAD), no)
OBJS+=../modules/aac_in/faad_dec.o
CFLAGS+=-DGPAC_HAS_FAAD
EXTRALIBS+= -lfaad
endif

OBJS+=../modules/ac3_in/ac3_in.o
ifneq ($(CONFIG_A52), no)
OBJS+=../modules/ac3_in/liba52_dec.o
CFLAGS+=-DGPAC_HAS_LIBA52
EXTRALIBS+= -la52
endif

ifeq ($(CONFIG_ALSA), yes)
OBJS+=../modules/alsa/alsa.o
CFLAGS+=-DGPAC_HAS_ALSA
EXTRALIBS+= -lasound
endif

OBJS+=../modules/audio_filter/audio_filter.o
OBJS+=../modules/bifs_dec/bifs_dec.o

ifeq ($(DISABLE_SMGR), no)
OBJS+=../modules/ctx_load/ctx_load.o
OBJS+=../modules/svg_in/svg_in.o
endif

OBJS+=../modules/dummy_in/dummy_in.o
OBJS+=../modules/ismacryp/isma_ea.o
ifneq ($(CONFIG_JS), no)
CFLAGS+=$(JS_FLAGS)
OBJS+=../modules/gpac_js/gpac_js.o
endif
ifeq ($(DISABLE_SVG), no)
OBJS+=../modules/laser_dec/laser_dec.o
endif
OBJS+=../modules/soft_raster/ftgrays.o ../modules/soft_raster/raster_load.o ../modules/soft_raster/raster_565.o ../modules/soft_raster/raster_argb.o ../modules/soft_raster/raster_rgb.o ../modules/soft_raster/stencil.o ../modules/soft_raster/surface.o

OBJS+=../modules/mp3_in/mp3_in.o
ifneq ($(CONFIG_MAD), no)
OBJS+=../modules/mp3_in/mad_dec.o
CFLAGS+=-DGPAC_HAS_MAD
EXTRALIBS+= -lmad
endif
OBJS+=../modules/isom_in/load.o ../modules/isom_in/read.o ../modules/isom_in/read_ch.o ../modules/isom_in/isom_cache.o
OBJS+=../modules/odf_dec/odf_dec.o
OBJS+=../modules/rtp_in/rtp_in.o ../modules/rtp_in/rtp_session.o ../modules/rtp_in/rtp_signaling.o ../modules/rtp_in/rtp_stream.o ../modules/rtp_in/sdp_fetch.o ../modules/rtp_in/sdp_load.o
OBJS+=../modules/saf_in/saf_in.o
ifeq ($(DISABLE_DASH_CLIENT), no)
OBJS+=../modules/mpd_in/mpd_in.o
ifneq ($(CONFIG_JS),no)
OBJS+=../modules/mse_in/mse_in.o
endif
endif
OBJS+=../modules/timedtext/timedtext_dec.o ../modules/timedtext/timedtext_in.o
OBJS+=../modules/validator/validator.o
OBJS+=../modules/raw_out/raw_video.o

OBJS+=../modules/img_in/img_dec.o ../modules/img_in/img_in.o ../modules/img_in/bmp_dec.o ../modules/img_in/png_dec.o ../modules/img_in/jpeg_dec.o
ifneq ($(CONFIG_JP2), no)
OBJS+=../modules/img_in/jp2_dec.o
EXTRALIBS+= -lopenjpeg
CFLAGS+=-DGPAC_HAS_JP2
endif

ifneq ($(CONFIG_FT), no)
OBJS+=../modules/ft_font/ft_font.o 
EXTRALIBS+= $(FT_LIBS)
CFLAGS+=-DGPAC_HAS_FREETYPE $(FT_CFLAGS)
endif

ifeq ($(DISABLE_MEDIA_IMPORT), no)
OBJS+=../modules/mpegts_in/mpegts_in.o 
endif

ifeq ($(DISABLE_LOADER_BT), no)
OBJS+=../modules/osd/osd.o
endif

#cf comment for openHEVC - we cannot build ffmpeg and openhevc as static modules for the current time :(
ifneq ($(CONFIG_FFMPEG), no)
ifneq ($(CONFIG_OPENHEVC),yes)
OBJS+=../modules/ffmpeg_in/ffmpeg_decode.o ../modules/ffmpeg_in/ffmpeg_demux.o ../modules/ffmpeg_in/ffmpeg_load.o
CFLAGS+=-DGPAC_HAS_FFMPEG -Wno-deprecated-declarations -I"$(SRC_PATH)/include" $(ffmpeg_cflags)
EXTRALIBS+= $(ffmpeg_lflags)
#fix it first
#OBJS+=../modules/redirect_av/redirect_av.o  
endif
endif

ifneq ($(CONFIG_XVID), no)
OBJS+=../modules/xvid_dec/xvid_dec.o
CFLAGS+=-DGPAC_HAS_XVID
EXTRALIBS+= -lxvidcore 
endif

ifneq ($(CONFIG_OGG), no)
OBJS+=../modules/ogg/ogg_load.o ../modules/ogg/ogg_in.o ../modules/ogg/theora_dec.o ../modules/ogg/vorbis_dec.o
CFLAGS+=-DGPAC_HAS_OGG
EXTRALIBS+= -logg
ifneq ($(CONFIG_VORBIS), no)
CFLAGS+=-DGPAC_HAS_VORBIS
EXTRALIBS+= -lvorbis
endif
ifneq ($(CONFIG_THEORA), no)
CFLAGS+=-DGPAC_HAS_THEORA
EXTRALIBS+= -ltheora
endif

endif


ifeq ($(CONFIG_SDL), yes)
CFLAGS+=-DGPAC_HAS_SDL $(SDL_CFLAGS)
EXTRALIBS+= $(SDL_LIBS)
OBJS+=../modules/sdl_out/sdl_out.o ../modules/sdl_out/audio.o ../modules/sdl_out/video.o
endif


ifeq ($(DISABLE_SVG), no)
OBJS+=../modules/widgetman/widgetman.o ../modules/widgetman/unzip.o ../modules/widgetman/widget.o ../modules/widgetman/wgt_load.o
endif

ifeq ($(CONFIG_OPENSVC),yes)
OBJS+= ../modules/opensvc_dec/opensvc_dec.o
CFLAGS+=-DGPAC_HAS_OPENSVC $(OSVC_CFLAGS)
EXTRALIBS+=$(OSVC_LDFLAGS)
endif

#cannot include openhevc and ffmpeg (yet) here since it uses avcodec_* calls which are relocated to libavcodec.so ...
ifeq ($(CONFIG_OPENHEVC),yes)
ifeq ($(CONFIG_FFMPEG), no)
OBJS+= ../modules/openhevc_dec/openhevc_dec.o
CFLAGS+=-DGPAC_HAS_OPENHEVC $(OHEVC_CFLAGS)
EXTRALIBS+=$(OHEVC_LDFLAGS)
endif
endif

ifeq ($(CONFIG_X11),yes)
OBJS+= ../modules/x11_out/x11_out.o
CFLAGS+=-DGPAC_HAS_X11
ifneq ($(X11_INC_PATH), )
CFLAGS+=-I$(X11_INC_PATH)
endif

ifneq ($(X11_LIB_PATH), )
EXTRALIBS+=-L$(X11_LIB_PATH)
endif
EXTRALIBS+=-lX11

ifeq ($(USE_X11_XV), yes)
CFLAGS+=-DGPAC_HAS_X11_XV
EXTRALIBS+=-lXv
endif

ifeq ($(USE_X11_XV), yes)
CFLAGS+=-DGPAC_HAS_X11_XV
EXTRALIBS+=-lXv
endif

ifeq ($(USE_X11_SHM), yes)
CFLAGS+=-DGPAC_HAS_X11_SHM
EXTRALIBS+=-lXext
endif

ifeq ($(HAS_OPENGL), yes)
ifeq ($(CONFIG_DARWIN),yes)
EXTRALIBS+=-lGL -lGLU
endif
endif

#end of x11
endif


ifeq ($(CONFIG_JACK), yes)
OBJS+= ../modules/jack/jack.o
CFLAGS+=-DGPAC_HAS_JACK
EXTRALIBS+=-ljack
endif

ifneq ($(CONFIG_OSS_AUDIO), no)
OBJS+= ../modules/oss_audio/oss.o
CFLAGS+=-DGPAC_HAS_OSS $(OSS_CFLAGS)
EXTRALIBS+=$(OSS_LDFLAGS)
ifneq ($(OSS_INC_TYPE), yes)
CFLAGS+=-DOSS_FIX_INC
endif
ifeq ($(TARGET_ARCH_ARMV4L), yes)
CFLAGS+=-DFORCE_SR_LIMIT
endif

endif


ifeq ($(CONFIG_PULSEAUDIO), yes)
OBJS+= ../modules/pulseaudio/pulseaudio.o
CFLAGS+=-DGPAC_HAS_PULSEAUDIO
EXTRALIBS+=-lpulse -lpulse-simple
endif


ifeq ($(CONFIG_DIRECTFB),yes)
OBJS+= ../modules/directfb_out/directfb_out.o ../modules/directfb_out/directfb_wrapper.o
CFLAGS+=-DGPAC_HAS_DIRECTFB -I$(DIRECTFB_INC_PATH)
EXTRALIBS+=$(DIRECTFB_LIB)
endif

ifeq ($(CONFIG_WIN32),yes)
OBJS+= ../modules/wav_out/wav_out.o
EXTRALIBS+=-DGPAC_HAS_WAVEOUT -DDISABLE_WAVE_EX
endif

ifeq ($(CONFIG_DIRECTX),yes)
OBJS+= ../dx_hw/dx_hw/dx_audio.o  ../dx_hw/dx_hw/dx_video.o  ../dx_hw/dx_hw/dx_window.o  ../dx_hw/dx_hw/dx_2d.o  ../dx_hw/dx_hw/copy_pixels.o
CFLAGS+=-DGPAC_HAS_DIRECTX -DDIRECTSOUND_VERSION=0x0500
ifneq ($(DX_PATH), system)
EXTRALIBS+=-L$(DX_PATH)/lib
CFLAGS+= -I$(DX_PATH)/include
endif
EXTRALIBS+=-ldsound -ldxguid -lddraw -lole32 -lgdi32 -lopengl32
endif


ifneq ($(CONFIG_FREENECT), no)
OBJS+= ../modules/freenect/freenect.o
CFLAGS+=-DGPAC_HAS_FREENECT $(FREENECT_CFLAGS)
ifeq ($(CONFIG_FREENECT), local)
CFLAGS+= -I"$(LOCAL_INC_PATH)"
EXTRALIBS+=-L../../extra_lib/lib/gcc
endif
EXTRALIBS+=-lfreenect
endif

#end of static modules

endif


NEED_LOCAL_LIB="no"
LINKLIBS=
SCENEGRAPH_CFLAGS=
MEDIATOOLS_CFLAGS=$(JS_FLAGS)

ifeq ($(DISABLE_CORE_TOOLS), no)

#1 - zlib support
ifeq ($(CONFIG_ZLIB), local)
CFLAGS+= -I"$(LOCAL_INC_PATH)/zlib"
NEED_LOCAL_LIB="yes"
endif
ifneq ($(CONFIG_ZLIB), no)
LINKLIBS+=-lz
endif

#2 - ssl support
ifeq ($(HAS_OPENSSL), yes)
LINKLIBS+=$(SSL_LIBS)
endif

#3 - spidermonkey support
ifeq ($(CONFIG_JS),no)
else
SCENEGRAPH_CFLAGS+=$(JS_FLAGS)
ifeq ($(CONFIG_JS),local)
NEED_LOCAL_LIB="yes"
endif
LINKLIBS+=$(JS_LIBS)
endif

endif

#4 - JPEG support
ifeq ($(CONFIG_JPEG), no)
else
LINKLIBS+= -ljpeg
#local lib
ifeq ($(CONFIG_JPEG), local)
NEED_LOCAL_LIB="yes"
MEDIATOOLS_CFLAGS+=-I"$(LOCAL_INC_PATH)/jpeg"
endif
endif


#5 - PNG support
ifeq ($(CONFIG_PNG), no)
else
LINKLIBS+= -lpng
ifeq ($(CONFIG_PNG), local)
NEED_LOCAL_LIB="yes"
MEDIATOOLS_CFLAGS+=-I"$(LOCAL_INC_PATH)/png"
endif
endif


## libgpac compositor compilation options
COMPOSITOR_CFLAGS=

## Add prefix before every lib
ifneq ($(prefix), /usr/local)
EXTRALIBS+=-L$(prefix)/lib
else
ifneq ($(prefix), /usr)
EXTRALIBS+=-L$(prefix)/lib
endif
endif

## OpenGL available
ifeq ($(HAS_OPENGL),yes)
EXTRALIBS+= $(OGL_LIBS)
COMPOSITOR_CFLAGS+=$(OGL_INCLS)
endif

ifeq ($(NEED_LOCAL_LIB), "yes")
EXTRALIBS+=-L../extra_lib/lib/gcc
endif
EXTRALIBS+=$(LINKLIBS)

ifeq ($(GPAC_USE_TINYGL), yes)
COMPOSITOR_CFLAGS+=-I"$(SRC_PATH)/../TinyGL/include"
else
COMPOSITOR_CFLAGS+=-DGPAC_HAS_GLU
endif

ifeq ($(ENABLE_JOYSTICK), yes)
COMPOSITOR_CFLAGS+=-DENABLE_JOYSTICK
endif


## libgpac media tools compilation options
ifeq ($(GPACREADONLY), yes)
MEDIATOOLS_CFLAGS=-DGPAC_READ_ONLY
endif


ifeq ($(GPAC_ENST), yes)
OBJS+=$(LIBGPAC_ENST)
EXTRALIBS+=-liconv
MEDIATOOLS_CFLAGS+=-DGPAC_ENST_PRIVATE
endif


##libgpac library output
LIB=libgpac.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols libgpac.def
else
ifeq ($(CONFIG_DARWIN),yes)
LDFLAGS+=-dynamiclib -install_name $(prefix)/lib/$(LIB)
endif
EXTRALIBS+=$(GPAC_SH_FLAGS)
endif

LD_SONAME="-Wl,-soname,libgpac.so.$(VERSION_MAJOR)"
ifeq ($(CONFIG_DARWIN), yes)
LD_SONAME=
endif

ifeq ($(CONFIG_SUNOS), yes)
LD_SONAME="-Wl,-h,$(LIB)"
LINKLIBS+= -lrt
endif

SRCS := $(OBJS:.o=.c) 

all: lib

lib: ../bin/gcc/$(LIB)

#there's a bunch of warnings in there, get rid of them
mcrypt: CFLAGS= $(OPTFLAGS) -w -I"$(SRC_PATH)/include"
mcrypt: $(LIBGPAC_MCRYPT)

scenegraph: CFLAGS += $(SCENEGRAPH_CFLAGS)
scenegraph: $(LIBGPAC_SCENE)
.deps/scenegraph/%: CFLAGS += $(SCENEGRAPH_CFLAGS)
.deps/scenegraph/%: CFLAGS += $(SCENEGRAPH_CFLAGS)

media_tools: CFLAGS += $(MEDIATOOLS_CFLAGS)
media_tools: $(LIBGPAC_MEDIATOOLS)
.deps/media_tools/%: CFLAGS += $(MEDIATOOLS_CFLAGS)
.deps/media_tools/%: CFLAGS += $(MEDIATOOLS_CFLAGS)

compositor: CFLAGS+=$(COMPOSITOR_CFLAGS)
compositor: $(LIBGPAC_COMPOSITOR)
.deps/compositor/%: CFLAGS += $(COMPOSITOR_CFLAGS)
.deps/compositor/%: CFLAGS += $(COMPOSITOR_CFLAGS)


../bin/gcc/$(LIB): $(LIBGPAC_UTILS) $(LIBGPAC_IETF) $(LIBGPAC_BIFS) $(LIBGPAC_ODF) $(LIBGPAC_LASER) $(LIBGPAC_ISOM) $(LIBGPAC_SCENEMANAGER) $(LIBGPAC_TERMINAL) compositor scenegraph media_tools mcrypt $(OBJS)
	@echo "OBJS $(OBJS)"
	@echo "LIBS $(EXTRALIBS)"
ifeq ($(CONFIG_DARWIN),yes)
	$(LIBTOOL) -s -o ../bin/gcc/libgpac_static.a $(OBJS)
	$(RANLIB) ../bin/gcc/libgpac_static.a
	$(CC) $(SHFLAGS) $(LD_SONAME) $(LDFLAGS) -o $@ $(OBJS) $(EXTRALIBS) 
else
	$(AR) cr ../bin/gcc/libgpac_static.a $(OBJS)
	$(RANLIB) ../bin/gcc/libgpac_static.a
	$(CC) $(SHFLAGS) $(LD_SONAME) $(LDFLAGS) -o $@ $(OBJS) $(EXTRALIBS)
ifeq (,$(findstring yes, $(CONFIG_WIN32)))
	mv $@ $@.$(VERSION_SONAME)
	ln -sf $(notdir $@).$(VERSION_SONAME) $@.$(VERSION_MAJOR)
	ln -sf $(notdir $@).$(VERSION_SONAME) $@
endif
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
