/*
 * (c) 2005, 2008 Nico Pranke <Nico.Pranke@googlemail.com>, Robin Luedtke <RobinLu@gmx.de> 
 *
 * This file is part of avcap.
 *
 * avcap is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * avcap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with avcap.  If not, see <http://www.gnu.org/licenses/>.
 */

/* avcap is free for non-commercial use.
 * To use it in commercial endeavors, please contact Nico Pranke <Nico.Pranke@googlemail.com>.
 */


#ifndef HELPFUNC_H_
#define HELPFUNC_H_

#include <list>
#include <math.h>
#include <string>

#include <DShow.h> // DirectShow
#include <mtype.h> // DeleteMediaType() function
#include <qedit.h> // CLSID_NullRenderer/CLSID_ISampleGrabber definitions
#include <winnls.h>

#ifndef __STREAMS__
# include <streams.h>
#endif

// Some definitions
#define VIDEO_SAMPLEGRABBER_FILTER_NAME L"SampleGrabber Video"
#define AUDIO_SAMPLEGRABBER_FILTER_NAME L"SampleGrabber Audio"

#define CAP_VIDEO_CAPTURE  0x00000001  /* Is a video capture device */
#define CAP_VIDEO_OUTPUT   0x00000002  /* Is a video output device */
#define CAP_VIDEO_OVERLAY  0x00000004  /* Can do video overlay */
#define CAP_VBI_CAPTURE    0x00000010  /* Is a VBI capture device */

#define CAP_TUNERDEVICE	   0x00010000  /* Has radio tuner or tv tuner */
#define CAP_TUNER          0x00020000  /* Has a tuner */
#define CAP_AUDIO_CAPTURE  0x00040000  /* Has audio support */
#define CAP_RADIO		   0x00080000  /* Is a radio device */

// Helper Functions -> see end of file
static inline void DeleteList(std::list<IPin*> &PinList);

static inline void DeleteList(std::list<IBaseFilter*> &AudioCaptureFilterList);

static inline void DeleteList(std::list<AM_MEDIA_TYPE*> &MediaTypeList);

static inline void EnumMediaTypesOnPin(IPin *Pin,
		std::list<AM_MEDIA_TYPE*> &MediaTypeList);

static inline void EnumPinsOnFilter(IBaseFilter *Filter,
		std::list<IPin*> &PinList);

static inline bool GetFilterGraphFromFilter(IBaseFilter *Filter,
		IGraphBuilder **FilterGraph,
		ICaptureGraphBuilder2 **CaptureGraphBuilder=NULL);

static inline char* WChar2Char(const wchar_t* szWChar);

static inline bool FindTunerRadioSupport(IBaseFilter *CaptureFilter,
		int *Capabilities);

static inline bool RenderStream(IUnknown *FilterOrPinToRender,
		bool RenderVideo, bool RenderAudio);

static inline bool GetInstalledDeviceIDs(
		std::list<std::string> &UniqueDeviceIDList);

static inline std::string bstr2string(BSTR bstr);

static inline std::string bstr2string(BSTR bstr)
{

	int length = ((DWORD*) bstr)[0];
	wchar_t* wchar_data = (wchar_t*) (((DWORD*) bstr) );

	int converted_length = WideCharToMultiByte(CP_ACP, 0, wchar_data, -1, 0, 0,
			0, 0);
	char* char_data = new char[converted_length];
	WideCharToMultiByte(CP_ACP, 0, wchar_data, -1, char_data, converted_length,
			0, 0);
	std::string res = char_data;
	delete[] char_data;
	return res;
}

//######################### DEBUG FUNCTIONS - MAKE THE FILTERGRAPH VISIBLE IN GRPAHEDIT ###################

static inline void EnumMediaTypesOnPin(IPin *Pin,
		std::list<AM_MEDIA_TYPE*> &MediaTypeList)
{
	QzCComPtr<IEnumMediaTypes> EnumMediaTypes;
	if (Pin->EnumMediaTypes(&EnumMediaTypes)==S_OK) {
		AM_MEDIA_TYPE *MediaType=NULL;
		while (EnumMediaTypes->Next(1, &MediaType, NULL) == S_OK) {
			MediaTypeList.push_back(MediaType);
		}
	}
}

static inline void EnumPinsOnFilter(IBaseFilter *Filter,
		std::list<IPin*> &PinList)
{
	QzCComPtr<IEnumPins> EnumPins;
	IPin *Pin=NULL;
	if (Filter->EnumPins(&EnumPins)==S_OK) {
		while (EnumPins->Next(1, &Pin, 0) == S_OK) {
			PinList.push_back(Pin);
		}
	}
}

static inline bool GetFilterGraphFromFilter(IBaseFilter *Filter,
		IGraphBuilder **FilterGraph,
		ICaptureGraphBuilder2 **CaptureGraphBuilder)
{
	if (Filter==NULL) {
		return false;
	}
	// Get the FilterGraph from the Filter
	FILTER_INFO FilterInfo;
	if (FAILED(Filter->QueryFilterInfo(&FilterInfo))) {
		return false;
	}
	if (FilterInfo.pGraph==NULL) {
		return false;
	}
	if (FAILED(FilterInfo.pGraph->QueryInterface(IID_IGraphBuilder,
			(void**) &*FilterGraph))) {
		return false;
	}
	// Get the CaptureGraphBuilder2
	// CaptureGraphBuilder is useful for building many kinds of custom filter graphs, not only capture graphs
	if (CaptureGraphBuilder!=NULL) {
		if (FAILED(CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL,
				CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2,
				(void**)&*CaptureGraphBuilder))) {
			return false;
		}
		if (FAILED((*CaptureGraphBuilder)->SetFiltergraph(*FilterGraph))) {
			return false;
		}
	}

	return true;
}

static inline char* WChar2Char(const wchar_t* szWChar)
{
	if (szWChar == NULL) {
		return NULL;
	}
	char* szChar = NULL;
	size_t size = 0;
	if ((size = wcstombs(0, szWChar, 0)) == -1) {
		return NULL;
	}
	szChar = new char[size + 1];
	szChar[size] = 0;
	wcstombs(szChar, szWChar, size);
	return szChar;
}

static inline bool FindTunerRadioSupport(IBaseFilter *CaptureFilter,
		int *Capabilities)
{
	if (CaptureFilter==NULL) {
		return false;
	}

	// Get the CaptureBuilderGraph COM-Interface from CaptureFilter
	QzCComPtr<ICaptureGraphBuilder2> CaptureGraphBuilder;
	QzCComPtr<IGraphBuilder> FilterGraph;
	if (!GetFilterGraphFromFilter(CaptureFilter, &FilterGraph,
			&CaptureGraphBuilder)) {
		return false;
	}

	// Look for the tuner, radio
	QzCComPtr<IAMTVTuner> Tuner;
	if (SUCCEEDED(CaptureGraphBuilder->FindInterface(&LOOK_UPSTREAM_ONLY, NULL,
			CaptureFilter, IID_IAMTVTuner, (void**)&Tuner))) {
		long lModes = 0;
		HRESULT hr = Tuner->GetAvailableModes(&lModes);
		if (SUCCEEDED(hr) && (lModes & AMTUNER_MODE_FM_RADIO)) {
			*Capabilities |= CAP_RADIO;
		}
		if (SUCCEEDED(hr) && (lModes & AMTUNER_MODE_TV)) {
			*Capabilities |= CAP_TUNER;
		}
	}

	return true;
}

static inline void GetInstalledAudioDevices(
		std::list<IBaseFilter*> &AudioCaptureFilterList)
{
	// Enumerate the audio category
	std::list<CLSID> EnumCategoriesList;
	EnumCategoriesList.push_back(CLSID_AudioInputDeviceCategory);

	for (std::list<CLSID>::iterator EnumCategoriesListIter=
			EnumCategoriesList.begin(); EnumCategoriesListIter
			!=EnumCategoriesList.end(); EnumCategoriesListIter++) {
		// Create the System Device Enumerator.
		HRESULT hr;
		ICreateDevEnum *SysDevEnum = NULL;
		if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
				CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)&SysDevEnum))) {
			return;
		}

		// Obtain a class enumerator for the device capture category.
		IEnumMoniker *EnumCat = NULL;
		hr = SysDevEnum->CreateClassEnumerator(*EnumCategoriesListIter,
				&EnumCat, 0);

		if (hr == S_OK) {
			// Enumerate the monikers.
			IMoniker *Moniker = NULL;
			ULONG Fetched;
			while (EnumCat->Next(1, &Moniker, &Fetched) == S_OK) {
				IBaseFilter *AudioCaptureFilter=NULL;
				if (SUCCEEDED(Moniker->BindToObject(0, 0, IID_IBaseFilter,
						(void**)&AudioCaptureFilter))) {
					AudioCaptureFilterList.push_back(AudioCaptureFilter);
				}
				else {
					if (AudioCaptureFilter!=NULL) {
						AudioCaptureFilter->Release();
					}
				}
				Moniker->Release();
			}
			EnumCat->Release();
		}
		SysDevEnum->Release();
	}
}

static inline bool RenderStream(IUnknown *FilterOrPinToRender,
		bool RenderVideo, bool RenderAudio)
{
	QzCComPtr<IGraphBuilder> FilterGraph;
	QzCComPtr<ICaptureGraphBuilder2> CaptureGraphBuilder;

	// Get the IGraphBuilder and CaptureGraphBuilder2 interfaces from pin or filter
	QzCComPtr<IPin> Pin;
	bool IsPin=false;
	if (SUCCEEDED(FilterOrPinToRender->QueryInterface(IID_IPin, (void**)&Pin))) // It's a pin
	{
		PIN_INFO PinInfo;
		Pin->QueryPinInfo(&PinInfo);
		IsPin=true;

		if (!GetFilterGraphFromFilter(PinInfo.pFilter, &FilterGraph,
				&CaptureGraphBuilder)) {
			return false;
		}
	}
	else // It's a filter
	{
		if (!GetFilterGraphFromFilter((IBaseFilter*)FilterOrPinToRender,
				&FilterGraph, &CaptureGraphBuilder)) {
			return false;
		}
	}

	//if(SUCCEEDED(CaptureGraphBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, CaptureFilter, NULL, NullRendererVideo))){return true;}

	// Build complete filter graph
	if (RenderVideo) {
		// Add NullRenderer filters because we want to set formats with IAMStreamConfig COM-Interface while the complete
		// filtergraph is connected. NullRenderer filters and the samplegrabber filter accept all formats.
		QzCComPtr<IBaseFilter> RendererVideo;
		if (FAILED(CoCreateInstance(CLSID_NullRenderer, 0,
				CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&RendererVideo))) {
			return false;
		}
		if (FAILED(FilterGraph->AddFilter(RendererVideo,L"Video Null Renderer" ))) {return false;}
		QzCComPtr<IBaseFilter> SampleGrabberVideo;
		if(FAILED(CoCreateInstance(CLSID_SampleGrabber, 0, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&SampleGrabberVideo))) {return false;}
		if(FAILED(FilterGraph->AddFilter(SampleGrabberVideo, VIDEO_SAMPLEGRABBER_FILTER_NAME))) {return false;}
		// Set the video samplegrabber
		QzCComPtr<ISampleGrabber> SampleGrabber;
		if(FAILED(SampleGrabberVideo->QueryInterface(IID_ISampleGrabber, (void**)&SampleGrabber))) {return false;}
		AM_MEDIA_TYPE mt;
		mt.majortype=MEDIATYPE_Video;
		mt.subtype=GUID_NULL;
		mt.pUnk=NULL;
		mt.cbFormat=0;
		if(FAILED(SampleGrabber->SetMediaType(&mt))) {return false;}
		//render the pin or filter
		if(FAILED(CaptureGraphBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, FilterOrPinToRender, SampleGrabberVideo, RendererVideo))) {return false;}
	}

	if(RenderAudio)
	{
		// Add NullRenderer filters because we want to set formats with IAMStreamConfig COM-Interface while the complete
		// filtergraph is connected. NullRenderer filters and the SampleGrabber filter accept all formats.
		QzCComPtr<IBaseFilter> RendererAudio;
		CoCreateInstance(CLSID_NullRenderer, 0, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&RendererAudio); //CLSID_DSoundRender for DirectSound Filter
		FilterGraph->AddFilter(RendererAudio, L"Audio Null Renderer");
		QzCComPtr<IBaseFilter> SampleGrabberAudio;
		CoCreateInstance(CLSID_SampleGrabber, 0, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&SampleGrabberAudio);
		FilterGraph->AddFilter(SampleGrabberAudio, AUDIO_SAMPLEGRABBER_FILTER_NAME);
		// Set the audio samplegrabber filter
		QzCComPtr<ISampleGrabber> SampleGrabber;
		SampleGrabberAudio->QueryInterface(IID_ISampleGrabber, (void**)&SampleGrabber);
		AM_MEDIA_TYPE mt;
		mt.majortype=MEDIATYPE_Audio;
		mt.subtype=GUID_NULL;
		mt.pUnk=NULL;
		mt.cbFormat=0;
		SampleGrabber->SetMediaType(&mt);

		//Render Audio
		if(IsPin) //if we should render a pin
		{
			HRESULT hr=CaptureGraphBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, FilterOrPinToRender, SampleGrabberAudio, RendererAudio);
			if(hr!=S_OK) {return false;}
		}
		// If we should render a filter

		else if(FAILED(CaptureGraphBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, FilterOrPinToRender, SampleGrabberAudio, RendererAudio)))
		{
			/* The avcap library doesn't provide full audio support at this time. There are only some basic audio
			 functions implemented (not implemented: audio capture functions, ...)*/

			/* Some notes about DirectShow and audio capture
			 - Old VFW driver based capture devices don't support audio.
			 - New WDM driver based capture devices can support audio:
			 1. The capture filter has audio output pins which have to be connected to a sound rendering DirectShow filter
			 2. The capture filter has crossbar support and the audio output pins are on the crossbar and have to
			 be connected to a sound rendering DirectShow filter.
			 3. Many capture devices (e.g. tv tuner cards) don't provide direct audio support. They have audio
			 connectors which have to be connected to a soundcard by cable. The soundcard takes up the
			 capture process of the audio data. In this case, a soundcard capture DirectShow filter is needed.
			 What we do if the function above fails?? We insert a audio capture DirectShow filter in the filtergraph
			 and connect it to the sound rendering DirectShow filter. What can we do if multiple soundcards
			 are installed - which audio capture DirectShow filter should we use?? Normally the user has to choose the
			 audio capture DirectShow filter. But because of the uncomplete audio implementation of the avcap library 
			 we use the first audio capture DirectShow Filter we'll find in the system. */

			/* TODO: Complete the audio functionality */

			std::list<IBaseFilter*> AudioCaptureFilterList;
			GetInstalledAudioDevices(AudioCaptureFilterList);
			for(std::list<IBaseFilter*>::iterator Iter=AudioCaptureFilterList.begin(); Iter!=AudioCaptureFilterList.end(); Iter++)
			{
				FilterGraph->AddFilter((*Iter), L"AudioCaptureFilter");
				if(SUCCEEDED(CaptureGraphBuilder->RenderStream(NULL, &MEDIATYPE_Audio, (*Iter), SampleGrabberAudio, RendererAudio)))
				{
					break;
				}
				FilterGraph->RemoveFilter((*Iter));
			}
			DeleteList(AudioCaptureFilterList);
		}
	}

	return true;
}

static inline void DeleteList(std::list<IPin*> &PinList)
{
	for (std::list<IPin*>::iterator Iter=PinList.begin(); Iter!=PinList.end(); Iter++) {
		if ((*Iter)!=NULL) {
			(*Iter)->Release();
		}
	}
	PinList.clear();
}

static inline void DeleteList(std::list<IBaseFilter*> &AudioCaptureFilterList)
{
	for (std::list<IBaseFilter*>::iterator Iter=AudioCaptureFilterList.begin(); Iter
			!=AudioCaptureFilterList.end(); Iter++) {
		if ((*Iter)!=NULL) {
			(*Iter)->Release();
		}
	}
	AudioCaptureFilterList.clear();
}

static inline void DeleteList(std::list<AM_MEDIA_TYPE*> &MediaTypeList)
{
	for (std::list<AM_MEDIA_TYPE*>::iterator Iter=MediaTypeList.begin(); Iter
			!=MediaTypeList.end(); Iter++) {
		DeleteMediaType(*Iter);
	}
	MediaTypeList.clear();
}

#endif
