/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Pierre Souchay 2008
 *  History:
 *
 *  2008/02/19 - v1.1 (Pierre Souchay)
 *    first revision
 *  2008/03/11 - v1.2 (Pierre Souchay)
 *    added volume control
 *    fixed possible bug in latency computation (did not return the max value)
 *
 *  Jack audio output module : output audio thru the jackd daemon
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


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <strings.h>
#include <jack/types.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <gpac/modules/audio_out.h>

#ifndef WIN32
#include <unistd.h>
int
getPid ()
{
  return getpid ();
}
#else
	// FIXME : get handle under WIN32 ?
int
getPid ()
{
  return 1979;
}
#endif

#define MAX_JACK_CLIENT_NAME_SZ 128
/*
 * This structure defines the handle to a Jack driver
 */
typedef struct
{
  char jackClientName[MAX_JACK_CLIENT_NAME_SZ];
  jack_client_t *jack;
  jack_port_t **jackPorts;
  jack_nframes_t currentBlockSize;
  u32 numChannels;
  char *buffer;
  u32 bufferSz;
  u32 bytesPerSample;
  char isActive;
  char autoConnect;
  char autoStartJackd;
  jack_default_audio_sample_t **channels;
  float volume;
} JackContext;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static void
Jack_cleanup (JackContext * ctx)
{
  u32 channels = 0;
  if (ctx == NULL)
    return;
  ctx->isActive = 0;
  GF_LOG (GF_LOG_DEBUG, GF_LOG_MMIO, ("[Jack] Jack_cleanup\n"));
  if (ctx->jack != NULL && ctx->isActive)
    {
      jack_deactivate (ctx->jack);
    }
  if (ctx->buffer != NULL)
    {
      gf_free(ctx->buffer);
      ctx->bufferSz = 0;
      ctx->buffer = NULL;
    }
  if (ctx->jackPorts != NULL)
    {
      for (channels = 0; channels < ctx->numChannels; channels++)
	{
	  if (ctx->jackPorts[channels] != NULL)
	    jack_port_unregister (ctx->jack, ctx->jackPorts[channels]);
	  ctx->jackPorts[channels] = NULL;
	}
      gf_free(ctx->jackPorts);
      ctx->jackPorts = NULL;
    }
  if (ctx->jack != NULL)
    {
      jack_client_close (ctx->jack);
    }
  if (ctx->channels != NULL)
    {
      gf_free(ctx->channels);
      ctx->channels = NULL;
    }
  ctx->numChannels = 0;
  ctx->currentBlockSize = 0;
  bzero (ctx->jackClientName, MAX_JACK_CLIENT_NAME_SZ);
  ctx->jack = NULL;
}

/**
 * The callback called by the jack thread
 */
static int
process_callback (jack_nframes_t nframes, void *arg)
{
  uint channel, i;
  short *tmpBuffer;
  size_t toRead;
  size_t bytesToRead;
  size_t readen;
  GF_AudioOutput *dr = (GF_AudioOutput *) arg;
  JackContext *ctx;
  if (dr == NULL)
    {
      // Should not happen
      return 1;
    }
  ctx = dr->opaque;
  toRead = nframes * ctx->numChannels;
  bytesToRead = toRead * ctx->bytesPerSample;
  readen = dr->FillBuffer (dr->audio_renderer, (void *) ctx->buffer,
			   bytesToRead);
  toRead = readen / ctx->bytesPerSample;
  if (ctx->bytesPerSample == 2)
    {
      tmpBuffer = (short *) ctx->buffer;
      for (channel = 0; channel < nframes; channel += ctx->numChannels)
	{
	  for (i = 0; i < ctx->numChannels; i++)
	    ctx->channels[i][channel] =
	      (float) (ctx->volume / 32768.0 *
		       (tmpBuffer[channel * ctx->numChannels + i]));
	}
    }
  else
    {
      for (channel = 0; channel < nframes; channel += ctx->numChannels)
	{
	  for (i = 0; i < ctx->numChannels; i++)
	    ctx->channels[i][channel] =
	      (float) (ctx->volume / 255.0 *
		       (ctx->buffer[channel * ctx->numChannels + i]));
	}
    }
  return 0;
}

/**
 * Called when jackbuffer size change
 */
static int
onBufferSizeChanged (jack_nframes_t nframes, void *arg)
{
  GF_AudioOutput *dr = (GF_AudioOutput *) arg;
  JackContext *ctx;
  size_t realBuffSize;
  u32 channel;
  if (dr == NULL)
    {
      // Should not happen
      return 1;
    }
  ctx = dr->opaque;
  realBuffSize = nframes * ctx->numChannels * sizeof (short);
  if (ctx->buffer != NULL && ctx->bufferSz == realBuffSize)
    return 0;
  if (ctx->channels != NULL)
    gf_free(ctx->channels);
  ctx->channels = NULL;
  ctx->channels = gf_calloc (ctx->numChannels, sizeof (jack_default_audio_sample_t *));
  if (ctx->channels == NULL)
    {
      Jack_cleanup (ctx);
      return 2;
    }
  for (channel = 0; channel < ctx->numChannels; channel++)
    {
      ctx->channels[channel] =
	jack_port_get_buffer (ctx->jackPorts[channel], nframes);
      if (ctx->channels[channel] == NULL)
	{
	  Jack_cleanup (ctx);
	  return 3;
	}
    }

  if (ctx->buffer != NULL)
    gf_free(ctx->buffer);
  ctx->buffer = gf_calloc (realBuffSize, sizeof (char));
  if (ctx->buffer == NULL)
    {
      Jack_cleanup (ctx);
      return 4;
    }
  ctx->bufferSz = realBuffSize;
  GF_LOG (GF_LOG_DEBUG, GF_LOG_MMIO,
	  ("[Jack] onBufferSizeChanged : resized to %d.\n", realBuffSize));
  if (ctx->buffer == NULL)
    {
      ctx->bufferSz = 0;
      Jack_cleanup (ctx);
      return 5;
    }
  return 0;
}

static const char *MODULE_NAME = "Jack";

static const char *AUTO_CONNECT_OPTION = "AutoConnect";

static const char *AUTO_START_JACKD_OPTION = "AutoStartJackd";

static const char *TRUE_OPTION = "true";

static const char *YES_OPTION = "yes";

static char
optionIsTrue (const char *optionValue)
{
  return (0 == strcasecmp (TRUE_OPTION, optionValue) ||
	  0 == strcasecmp (YES_OPTION, optionValue)
	  || 0 == strcmp ("1", optionValue));
}


static GF_Err
Jack_Setup (GF_AudioOutput * dr, void *os_handle, u32 num_buffers,
	    u32 total_duration)
{
  const char *opt;
  JackContext *ctx = (JackContext *) dr->opaque;
  jack_status_t status;
  jack_options_t options = JackNullOption;

  memset (ctx->jackClientName, 0, MAX_JACK_CLIENT_NAME_SZ);
  snprintf (ctx->jackClientName, MAX_JACK_CLIENT_NAME_SZ, "gpac-%d",
	    getPid ());

  opt =
    gf_modules_get_option ((GF_BaseInterface *) dr, MODULE_NAME,
			   AUTO_CONNECT_OPTION);
  if (opt != NULL)
    {
      if (optionIsTrue (opt))
	ctx->autoConnect = TRUE;
      else
	ctx->autoConnect = FALSE;
    }
  else
    {
      ctx->autoConnect = TRUE;
      gf_modules_set_option ((GF_BaseInterface *) dr, MODULE_NAME,
			     AUTO_CONNECT_OPTION, YES_OPTION);
    }
  opt = gf_modules_get_option ((GF_BaseInterface *) dr, MODULE_NAME,
			       AUTO_START_JACKD_OPTION);
  if (opt != NULL)
    {
      if (optionIsTrue (opt))
	ctx->autoStartJackd = TRUE;
      else
	ctx->autoStartJackd = FALSE;
    }
  else
    {
      ctx->autoStartJackd = TRUE;
      gf_modules_set_option ((GF_BaseInterface *) dr, MODULE_NAME,
			     AUTO_START_JACKD_OPTION, YES_OPTION);
    }
  if (!ctx->autoStartJackd)
    {
      options |= JackNoStartServer;
    }
  ctx->jack = jack_client_open (ctx->jackClientName, options, &status, NULL);
  if (status & JackNameNotUnique)
    {
      GF_LOG (GF_LOG_ERROR, GF_LOG_MMIO,
	      ("[Jack] Cannot open connection to jackd as %s since name was not unique.\n",
	       ctx->jackClientName));
      Jack_cleanup (ctx);
      return GF_IO_ERR;
    }

  if (ctx->jack == NULL)
    {
      GF_LOG (GF_LOG_ERROR, GF_LOG_MMIO,
	      ("[Jack] Cannot open connection to jackd as %s.\n",
	       ctx->jackClientName));
      return GF_IO_ERR;
    }
  return GF_OK;
}

static void
Jack_Shutdown (GF_AudioOutput * dr)
{
  JackContext *ctx = (JackContext *) dr->opaque;
  Jack_cleanup (ctx);
}

#define JACK_PORT_NAME_MAX_SZ 128

static GF_Err
Jack_ConfigureOutput (GF_AudioOutput * dr, u32 * SampleRate, u32 * NbChannels,
		      u32 * nbBitsPerSample, u32 channel_cfg)
{
  u32 channels;
  u32 i;
  char port_name[JACK_PORT_NAME_MAX_SZ];
  JackContext *ctx = (JackContext *) dr->opaque;
  if (!ctx)
    return GF_BAD_PARAM;
  ctx->bytesPerSample = *nbBitsPerSample / 8;
  if (ctx->bytesPerSample > 2 || ctx->bytesPerSample < 1)
    {
      GF_LOG (GF_LOG_ERROR, GF_LOG_MMIO,
	      ("[Jack] Jack-ConfigureOutput : unable to use %d bits/sample.\n"));
      return GF_BAD_PARAM;
    }
  ctx->numChannels = *NbChannels;
  *SampleRate = jack_get_sample_rate (ctx->jack);
  GF_LOG (GF_LOG_DEBUG, GF_LOG_MMIO,
	  ("[Jack] Jack_ConfigureOutput channels=%d, srate=%d bits/sample=%d\n",
	   *NbChannels, *SampleRate, *nbBitsPerSample));
  if (ctx->jackPorts == NULL)
    ctx->jackPorts = gf_calloc (ctx->numChannels, sizeof (jack_port_t *));
  if (ctx->jackPorts == NULL)
    {
      goto exit_cleanup;
    }
  if (!ctx->isActive)
    {
      for (channels = 0; channels < ctx->numChannels; channels++)
	{
	  snprintf (port_name, JACK_PORT_NAME_MAX_SZ, "playback_%d",
		    channels + 1);
	  ctx->jackPorts[channels] =
	    jack_port_register (ctx->jack, port_name, JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsOutput, 0);
	  if (ctx->jackPorts[channels] == NULL)
	    goto exit_cleanup;
	}
      onBufferSizeChanged (jack_get_buffer_size (ctx->jack), dr);
      jack_set_buffer_size_callback (ctx->jack, onBufferSizeChanged, dr);
      jack_set_process_callback (ctx->jack, process_callback, dr);
    }
  ctx->currentBlockSize = jack_get_buffer_size (ctx->jack);
  if (!ctx->isActive)
    {
      jack_activate (ctx->jack);
      if (ctx->autoConnect)
	{
	  const char **matching_outputs =
	    jack_get_ports (ctx->jack, NULL, NULL,
			    JackPortIsInput | JackPortIsPhysical |
			    JackPortIsTerminal);
	  if (matching_outputs != NULL)
	    {
	      channels = 0;
	      i = 0;
	      while (matching_outputs[i] != NULL
		     && channels < ctx->numChannels)
		{
		  if (!jack_connect (ctx->jack,
				     jack_port_name (ctx->
						     jackPorts[channels++]),
				     matching_outputs[i]))
		    {
		      GF_LOG (GF_LOG_INFO, GF_LOG_MMIO,
			      ("[Jack] Jack_ConfigureOutput: Failed to connect port[%d] to %s.\n",
			       channels - 1, matching_outputs[i]));
		    }
		  i++;
		}
	    }
	}
      ctx->isActive = TRUE;
    }
  return GF_OK;
exit_cleanup:
  Jack_cleanup (ctx);
  return GF_IO_ERR;
}

static void
Jack_SetVolume (GF_AudioOutput * dr, u32 Volume)
{
  JackContext *ctx = (JackContext *) dr->opaque;
  if (ctx == NULL)
    {
      return;
    }
  /* We support ajust the volume to more than 100, up to +6dB even
   * if frontends don't support it, it may be useful */
  if (Volume > 400)
    Volume = 400;
  ctx->volume = (float) Volume / 100.0;
  GF_LOG (GF_LOG_DEBUG, GF_LOG_MMIO,
	  ("[Jack] Jack_SetVolume: Volume set to %d%%.\n", Volume));
}

static void
Jack_SetPan (GF_AudioOutput * dr, u32 Pan)
{
  GF_LOG (GF_LOG_INFO, GF_LOG_MMIO, ("[Jack] Jack_SetPan: Not supported.\n"));

}

static void
Jack_SetPriority (GF_AudioOutput * dr, u32 Priority)
{
  /**
   * Jack manages the priority itself, we don't need
   * to interfere here...
   */
}

static u32
Jack_GetAudioDelay (GF_AudioOutput * dr)
{
  jack_nframes_t max = 0;
  jack_nframes_t latency;
  u32 channel;
  JackContext *ctx = (JackContext *) dr->opaque;
  if (ctx == NULL)
    {
      return 0;
    }
  jack_recompute_total_latencies (ctx->jack);
  for (channel = 0; channel < ctx->numChannels; channel++)
    {
      latency =
	jack_port_get_total_latency (ctx->jack, ctx->jackPorts[channel]);
      if (latency > max)
	max = latency;
    }
  channel = max * 1000 / jack_get_sample_rate (ctx->jack);
  GF_LOG (GF_LOG_DEBUG, GF_LOG_MMIO,
	  ("[Jack] Jack_GetAudioDelay latency = %d ms.\n", channel));
  return channel;
}

static GF_Err
Jack_QueryOutputSampleRate (GF_AudioOutput * dr, u32 * desired_sr,
			    u32 * NbChannels, u32 * nbBitsPerSample)
{
  JackContext *ctx = (JackContext *) dr->opaque;
  if (!ctx)
    return GF_IO_ERR;
  *desired_sr = jack_get_sample_rate (ctx->jack);
  *NbChannels = 2;
  GF_LOG (GF_LOG_DEBUG, GF_LOG_MMIO,
	  ("[Jack] Jack output sample rate %d\n", *desired_sr));
  return GF_OK;
}

void *
NewJackOutput ()
{
  JackContext *ctx;
  GF_AudioOutput *driv;
  GF_SAFEALLOC (ctx, JackContext);
  if (!ctx)
    return NULL;
  GF_SAFEALLOC (driv, GF_AudioOutput);
  if (!driv)
    {
      gf_free(ctx);
      return NULL;
    }
  driv->opaque = ctx;
  driv->SelfThreaded = 1;
  driv->Setup = Jack_Setup;
  driv->Shutdown = Jack_Shutdown;
  driv->ConfigureOutput = Jack_ConfigureOutput;
  driv->GetAudioDelay = Jack_GetAudioDelay;
  driv->SetVolume = Jack_SetVolume;
  driv->SetPan = Jack_SetPan;
  driv->SetPriority = Jack_SetPriority;
  driv->QueryOutputSampleRate = Jack_QueryOutputSampleRate;

  ctx->jack = NULL;
  ctx->numChannels = 0;
  ctx->jackPorts = NULL;
  ctx->currentBlockSize = 0;
  ctx->numChannels = 0;
  ctx->buffer = NULL;
  ctx->bufferSz = 0;
  ctx->bytesPerSample = 0;
  ctx->isActive = FALSE;
  ctx->autoConnect = FALSE;
  ctx->autoStartJackd = FALSE;
  ctx->volume = 1.0;

  GF_REGISTER_MODULE_INTERFACE (driv, GF_AUDIO_OUTPUT_INTERFACE,
				"Jack Audio Output", "gpac distribution");
  return driv;
}

void
DeleteJackOutput (void *ifce)
{
  GF_AudioOutput *dr = (GF_AudioOutput *) ifce;
  JackContext *ctx = (JackContext *) dr->opaque;
  Jack_cleanup (ctx);
  gf_free(ctx);
  dr->opaque = NULL;
  gf_free(dr);
}

/*
 * ********************************************************************
 * interface
 */

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_AUDIO_OUTPUT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface (u32 InterfaceType)
{
  if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE)
    {
      return NewJackOutput ();
    }
  return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface (GF_BaseInterface * ifce)
{
  if (ifce->InterfaceType == GF_AUDIO_OUTPUT_INTERFACE)
    DeleteJackOutput ((GF_AudioOutput *) ifce);
}

GPAC_MODULE_STATIC_DELARATION( jack )
