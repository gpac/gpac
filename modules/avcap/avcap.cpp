/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2011-20XX
 *					All rights reserved
 *
 *  This file is part of GPAC / AVCAP video input module
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <avcap/avcap.h>


#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/modules/service.h>
#include <gpac/modules/codec.h>
/*for GF_STREAM_PRIVATE_SCENE definition*/
#include <gpac/constants.h>
#include <gpac/download.h>

#ifdef __cplusplus
}
#endif


#if !defined(__GNUC__)&& (defined(_WIN32_WCE) || defined (WIN32))
#  pragma comment(lib, "strmiids")
#ifdef _DEBUG
#  pragma comment(lib, "avcapd")
#  pragma comment(lib, "strmbasd")
#else
#  pragma comment(lib, "avcap")
#  pragma comment(lib, "strmbase")
#endif
#  pragma comment(lib, "winmm")
#endif


using namespace avcap;

class GPACCaptureHandler : public CaptureHandler
{
public:
	GPACCaptureHandler(GF_ClientService *service, LPNETCHANNEL channel)
		: m_pService(service), m_pChannel(channel), m_data(NULL)
	{
		memset(&m_pSLHeader, 0, sizeof(GF_SLHeader));
		m_pSLHeader.compositionTimeStampFlag = 1;
	}
	virtual ~GPACCaptureHandler() {
		if (m_data != NULL) {
			gf_free(m_data);
			m_data=NULL;
		}
	}

	GF_ClientService *m_pService;
	LPNETCHANNEL m_pChannel;
	GF_SLHeader m_pSLHeader;

	u32 m_height;
	u32 m_stride;

	char* m_data;

public:
	void AllocData(u32 height, u32 stride) {
		m_height = height;
		m_stride = stride;
		m_data = (char*)gf_malloc(m_height * m_stride);
	}
	/* This method is called by the CaptureManager, when new data was captured.
	 * \param io_buf The buffer, that contains the captured data. */
	void handleCaptureEvent(IOBuffer* io_buf);
};


void GPACCaptureHandler::handleCaptureEvent(IOBuffer* io_buf)
{
	m_pSLHeader.compositionTimeStamp = io_buf->getTimestamp();

	if (m_data) {
		char* data = (char*)io_buf->getPtr();
		for (u32 i=0; i<m_height; i++) {
			memcpy(m_data + (m_height - 1 - i) * m_stride, data + i*m_stride, m_stride);
		}
		gf_term_on_sl_packet(m_pService, m_pChannel, m_data, (u32)io_buf->getValidBytes(), &m_pSLHeader, GF_OK);
	} else {
		gf_term_on_sl_packet(m_pService, m_pChannel, (char *) io_buf->getPtr(), (u32)io_buf->getValidBytes(), &m_pSLHeader, GF_OK);
	}
	io_buf->release();
}

DeviceDescriptor* get_device_descriptor(char *name)
{
	// find the descriptor of the device in the device-list by index
	const DeviceCollector::DeviceList& dl = DEVICE_COLLECTOR::instance().getDeviceList();
	DeviceDescriptor* dd = 0;
	int index = 0;

	for(DeviceCollector::DeviceList::const_iterator i = dl.begin(); i != dl.end(); i++, index++) {
		dd = *i;

		if (!name || !stricmp(name, "default") )
			return dd;
		if (strstr((char *) dd->getName().c_str(), name) != NULL)
			return dd;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[VideoCapture] Cannot find capture driver %s\n", name));
	return NULL;
}


#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	/*the service we're responsible for*/
	GF_ClientService *service;
	u32 state;

	DeviceDescriptor *device_desc;
	CaptureDevice* device;

	GPACCaptureHandler *video_handler;
	GPACCaptureHandler *audio_handler;

	u32 width, height, pixel_format, stride, out_size, fps;
	u32 default_4cc;
	Bool flip_video;
} AVCapIn;


Bool AVCap_CanHandleURL(GF_InputService *plug, const char *url)
{
	if (!strnicmp(url, "camera://", 9)) return GF_TRUE;
	if (!strnicmp(url, "video://", 8)) return GF_TRUE;
	return GF_FALSE;
}


GF_Err AVCap_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	GF_ESD *esd;
	GF_BitStream *bs;
	GF_ObjectDescriptor *od;
	const char *opt;
	AVCapIn *vcap = (AVCapIn *) plug->priv;

	if (!vcap || !serv || !url) return GF_BAD_PARAM;

	vcap->state = 0;
	vcap->service = serv;

	opt = gf_modules_get_option((GF_BaseInterface *)plug, "AVCap", "FlipVideo");
	if (opt && !strcmp(opt, "yes"))	vcap->flip_video = GF_TRUE;

	if (!vcap->device_desc) {
		Format *format;
		u32 default_4cc;
		char *name;
		char *params = (char *) strchr(url, '?');
		if (params) params[0] = 0;
		name = (char *) strstr(url, "://");
		if (name) name += 3;
		/* get device by name  */
		vcap->device_desc = get_device_descriptor(name);
		if (params) {
			params[0] = '?';
			params ++;
		}

		if (!vcap->device_desc) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[VideoCapture] Failed to instanciate AVCap\n"));
			gf_term_on_connect(serv, NULL, GF_REMOTE_SERVICE_ERROR);
			return GF_OK;
		}

		vcap->device_desc->open();
		if ( (!strnicmp(url, "camera://", 9) || !strnicmp(url, "video://", 8)) && !vcap->device_desc->isVideoCaptureDev()) {
			vcap->device_desc->close();
			gf_term_on_connect(serv, NULL, GF_URL_ERROR);
			return GF_OK;
		}
		else if (!strnicmp(url, "audio://", 8) && !vcap->device_desc->isAudioDev()) {
			vcap->device_desc->close();
			gf_term_on_connect(serv, NULL, GF_URL_ERROR);
			return GF_OK;
		}
		vcap->device = vcap->device_desc->getDevice();
		if (!vcap->device) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[VideoCapture] Failed to initialize capture device\n"));
			vcap->device_desc->close();
			gf_term_on_connect(serv, NULL, GF_SERVICE_ERROR);
			return GF_OK;
		}
		vcap->device->getFormatMgr()->setFramerate(30);

		default_4cc = 0;
		opt = gf_modules_get_option((GF_BaseInterface *)plug, "AVCap", "Default4CC");
		if (opt) {
			default_4cc = GF_4CC(opt[0], opt[1], opt[2], opt[3]);
			vcap->device->getFormatMgr()->setFormat(default_4cc);
		}


		while (params) {
			char *sep = (char *) strchr(params, '&');
			if (sep) sep[0] = 0;

			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[VideoCapture] Set camera option %s\n", params));

			if (!strnicmp(params, "resolution=", 11)) {
				u32 w, h;
				if (sscanf(params+11, "%dx%d", &w, &h)==2) {
					vcap->device->getFormatMgr()->setResolution(w, h);
					GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[VideoCapture] Set resolution to %dx%d\n", w, h));
				}
			}
			else if (!strnicmp(params, "fps=", 4)) {
				u32 fps;
				if (sscanf(params+4, "%d", &fps)==1) {
					vcap->device->getFormatMgr()->setFramerate(fps);
					GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[VideoCapture] Set framerate to %d\n", fps));
				}
			}
			else if (!strnicmp(params, "stereo=", 7)) {
			}
			else if (!strnicmp(params, "mode=", 5)) {
			}
			else if (!strnicmp(params, "fmt=", 4)) {
				if (!strnicmp(params+4, "rgb", 3)) {
					default_4cc = GF_4CC('3', 'B', 'G', 'R');
				}
				else if (!strnicmp(params+4, "yuv", 3)) {
					default_4cc = GF_4CC('V', 'Y', 'U', 'Y');
				}
				else if (strlen(params+4)>=4) {
					default_4cc = GF_4CC(params[4], params[5], params[6], params[7]);
				}
			}

			if (!sep) break;
			sep[0] = '&';
			params = sep+1;
		}

		vcap->width = vcap->device->getFormatMgr()->getWidth();
		vcap->height = vcap->device->getFormatMgr()->getHeight();
		vcap->fps = vcap->device->getFormatMgr()->getFramerate();

		if (default_4cc )
			vcap->device->getFormatMgr()->setFormat(default_4cc );

		format = vcap->device->getFormatMgr()->getFormat();
		switch (format->getFourcc()) {
		case GF_4CC('V', 'Y', 'U', 'Y'):
		case GF_4CC('2', 'Y', 'U', 'Y'):
			vcap->pixel_format = GF_PIXEL_YUY2;
			vcap->stride = 2*vcap->width;
			vcap->out_size = 2*vcap->width*vcap->height;
			break;
		case GF_4CC('2', '1', 'U', 'Y'):
			vcap->pixel_format = GF_PIXEL_I420;
			vcap->stride = (u32)vcap->device->getFormatMgr()->getBytesPerLine();//1.5*vcap->width;//
			vcap->out_size = (u32)vcap->device->getFormatMgr()->getImageSize();//1.5*vcap->width*vcap->height;//
			break;
		case GF_4CC('3', 'B', 'G', 'R'):
			vcap->pixel_format = GF_PIXEL_BGR_24;
			vcap->stride = vcap->device->getFormatMgr()->getBytesPerLine();//1.5*vcap->width;//
			vcap->out_size = (u32)vcap->device->getFormatMgr()->getImageSize();//1.5*vcap->width*vcap->height;//
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[VideoCapture] Unsupported 4CC %s (%08x) from capture device\n", gf_4cc_to_str(format->getFourcc()), format->getFourcc()));
			vcap->device_desc->close();
			gf_term_on_connect(serv, NULL, GF_NOT_SUPPORTED);
			return GF_OK;
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[VideoCapture] Device configured - resolution %dx%d - Frame Rate %d - Pixel Format %s (Device 4CC %08x) \n", vcap->width, vcap->height, vcap->fps, gf_4cc_to_str(vcap->pixel_format), format->getFourcc()));
	}

	/*ACK connection is OK*/
	gf_term_on_connect(serv, NULL, GF_OK);


	/*setup object descriptor*/
	od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);

	esd = gf_odf_desc_esd_new(0);
	esd->slConfig->timestampResolution = 1000;
	if (!strnicmp(url, "camera://", 9) || !strnicmp(url, "video://", 8)) {
		od->objectDescriptorID = 1;
		esd->ESID = 1;
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	} else {
		od->objectDescriptorID = 2;
		esd->ESID = 2;
		esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	}
	esd->decoderConfig->objectTypeIndication = GPAC_OTI_RAW_MEDIA_STREAM;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_u32(bs, vcap->pixel_format);
	gf_bs_write_u16(bs, vcap->width);
	gf_bs_write_u16(bs, vcap->height);
	gf_bs_write_u32(bs, vcap->out_size);
	gf_bs_write_u32(bs, vcap->stride);
	gf_bs_get_content(bs, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_bs_del(bs);

	gf_list_add(od->ESDescriptors, esd);
	gf_term_add_media(vcap->service, (GF_Descriptor*)od, GF_FALSE);

	return GF_OK;
}

GF_Err AVCap_CloseService(GF_InputService *plug)
{
	AVCapIn *vcap = (AVCapIn *) plug->priv;
	if (vcap->device_desc) {
		vcap->device_desc->close();
		vcap->device_desc = NULL;
	}

	vcap->state = 0;
	gf_term_on_disconnect(vcap->service, NULL, GF_OK);
	return GF_OK;
}

/*Dummy input just send a file name, no multitrack to handle so we don't need to check sub_url nor expected type*/
static GF_Descriptor *AVCap_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	return NULL;
}


GF_Err AVCap_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	AVCapIn *vcap = (AVCapIn *) plug->priv;

	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	switch (com->command_type) {
	case GF_NET_CHAN_SET_PULL:
		return GF_NOT_SUPPORTED;
	case GF_NET_CHAN_INTERACTIVE:
		return GF_OK;
	/*since data is file-based, no padding is needed (decoder plugin will handle it itself)*/
	case GF_NET_CHAN_SET_PADDING:
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = com->buffer.min = 500;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		/*this module is not made for updates, use undefined duration*/
		com->duration.duration = 0;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		if (vcap->state==0) {
			/*start capture*/
			if (vcap->video_handler)
				vcap->device->getVidCapMgr()->registerCaptureHandler(vcap->video_handler);

			if (vcap->device->getVidCapMgr()->startCapture() != -1)
				vcap->state = 1;
			else
				vcap->device->getVidCapMgr()->removeCaptureHandler();
		}
		return GF_OK;
	case GF_NET_CHAN_STOP:
		if (vcap->state==1) {
			/*stop capture*/
			vcap->device->getVidCapMgr()->removeCaptureHandler();
			vcap->device->getVidCapMgr()->stopCapture();
			vcap->state = 0;
		}
		return GF_OK;
	case GF_NET_CHAN_CONFIG:
		return GF_OK;
	case GF_NET_CHAN_GET_DSI:
		com->get_dsi.dsi = NULL;
		com->get_dsi.dsi_len = 0;
		return GF_OK;
	default:
		break;
	}
	return GF_OK;
}

GF_Err AVCap_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ESID;
	AVCapIn *vcap = (AVCapIn *) plug->priv;

	sscanf(url, "ES_ID=%u", &ESID);
	if (ESID == 1) {
		/*video connect*/
		vcap->video_handler = new GPACCaptureHandler(vcap->service, channel);

		if (vcap->flip_video)
			vcap->video_handler->AllocData(vcap->height, vcap->stride);

		gf_term_on_connect(vcap->service, channel, GF_OK);
	} else if (ESID == 2) {
		/*audio connect*/
		vcap->audio_handler = new GPACCaptureHandler(vcap->service, channel);
		gf_term_on_connect(vcap->service, channel, GF_OK);
	} else {
		gf_term_on_connect(vcap->service, channel, GF_STREAM_NOT_FOUND);
	}
	return GF_OK;
}

GF_Err AVCap_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	AVCapIn *vcap = (AVCapIn *) plug->priv;
	if (vcap->video_handler && vcap->video_handler->m_pChannel==channel) {
		delete vcap->video_handler;
		vcap->video_handler = NULL;
	}
	else if (vcap->audio_handler && vcap->audio_handler->m_pChannel==channel) {
		delete vcap->audio_handler;
		vcap->audio_handler = NULL;
	}
	gf_term_on_disconnect(vcap->service, channel, GF_OK);
	return GF_OK;
}

Bool AVCap_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	return GF_FALSE;
}


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_NET_CLIENT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) {
		AVCapIn *vcap;
		GF_InputService *plug;
		GF_SAFEALLOC(plug, GF_InputService);
		memset(plug, 0, sizeof(GF_InputService));
		GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "Video Capture using libavcap", "gpac distribution")

		plug->RegisterMimeTypes = NULL;
		plug->CanHandleURL = AVCap_CanHandleURL;
		plug->ConnectService = AVCap_ConnectService;
		plug->CloseService = AVCap_CloseService;
		plug->GetServiceDescriptor = AVCap_GetServiceDesc;
		plug->ConnectChannel = AVCap_ConnectChannel;
		plug->DisconnectChannel = AVCap_DisconnectChannel;
		plug->ServiceCommand = AVCap_ServiceCommand;
		plug->CanHandleURLInService = AVCap_CanHandleURLInService;

		GF_SAFEALLOC(vcap, AVCapIn);
		plug->priv = vcap;
		return (GF_BaseInterface *)plug;
	}
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
	if (bi->InterfaceType==GF_NET_CLIENT_INTERFACE) {
		GF_InputService *ifcn = (GF_InputService*)bi;
		AVCapIn *vcap = (AVCapIn*)ifcn->priv;
		gf_free(vcap);
		gf_free(bi);
	}
}

GPAC_MODULE_STATIC_DELARATION( avcap )

#ifdef __cplusplus
}
#endif
