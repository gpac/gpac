/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Rendering sub-project
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



#ifndef COMMON_STACKS_H
#define COMMON_STACKS_H

#include <gpac/internal/renderer_dev.h>

void InitAudioSource(GF_Renderer *sr, GF_Node *node);
void AudioSourceModified(GF_Node *node);
void InitAudioClip(GF_Renderer *sr, GF_Node *node);
void AudioClipModified(GF_Node *node);
void InitAudioBuffer(GF_Renderer *sr, GF_Node *node);
void AudioBufferModified(GF_Node *node);
void InitAnimationStream(GF_Renderer *sr, GF_Node *node);
void AnimationStreamModified(GF_Node *node);
void InitTimeSensor(GF_Renderer *sr, GF_Node *node);
void TimeSensorModified(GF_Node *node);
void InitImageTexture(GF_Renderer *sr, GF_Node *node);
GF_TextureHandler *it_get_texture(GF_Node *node);
void ImageTextureModified(GF_Node *node);
void InitMovieTexture(GF_Renderer *sr, GF_Node *node);
GF_TextureHandler *mt_get_texture(GF_Node *node);
void MovieTextureModified(GF_Node *node);
void InitPixelTexture(GF_Renderer *sr, GF_Node *node);
GF_TextureHandler *pt_get_texture(GF_Node *node);

#endif


