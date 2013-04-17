/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Pierre Souchay
 *			Copyright (c) Telecom ParisTech 2010
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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
#include <gpac/ringbuffer.h>

GF_EXPORT
GF_Ringbuffer * gf_ringbuffer_new(u32 sz)
{
  GF_Ringbuffer *rb;

  rb = gf_malloc (sizeof (GF_Ringbuffer));
  if (sz % 2 != 0)
    sz++;
  rb->size = sz;
  rb->size_mask = rb->size;
  rb->size_mask -= 1;
  rb->write_ptr = 0;
  rb->read_ptr = 0;
  rb->buf = gf_malloc (rb->size);
  rb->mx = gf_mx_new("RingBufferMutex");
  return rb;
}

GF_EXPORT
void gf_ringbuffer_del(GF_Ringbuffer * ringbuffer){
    if (!ringbuffer)
      return;
    gf_mx_p(ringbuffer->mx);
    gf_free( ringbuffer->buf);
    ringbuffer->mx = NULL;
    gf_mx_v(ringbuffer->mx);
}

/*!
 * Return the number of bytes available for writing.  This is the
 * number of bytes in front of the write pointer and behind the read
 * pointer.
 * \param The ringbuffer
 */
GF_EXPORT
u32 gf_ringbuffer_available_for_write (GF_Ringbuffer * rb)
{
  u32 w, r;

  w = rb->write_ptr;
  r = rb->read_ptr;

  if (w > r) {
    return ((r - w + rb->size) & rb->size_mask) - 1;
  } else if (w < r) {
    return (r - w) - 1;
  } else {
    return rb->size - 1;
  }
}

GF_EXPORT
u32 gf_ringbuffer_available_for_read (GF_Ringbuffer * rb)
{
  size_t w, r;

  w = rb->write_ptr;
  r = rb->read_ptr;

  if (w > r) {
    return (u32) (w - r);
  } else {
    return (u32) (w - r + rb->size) & rb->size_mask;
  }
}

GF_EXPORT
u32 gf_ringbuffer_read(GF_Ringbuffer *rb, u8 *dest, u32 szDest)
{
 u32 free_sz, sz2, to_read, n1, n2;

  if ((free_sz = gf_ringbuffer_available_for_read(rb)) == 0) {
    return 0;
  }

  to_read = szDest > free_sz ? free_sz : szDest;

  sz2 = rb->read_ptr + to_read;

  if (sz2 > rb->size) {
    n1 = rb->size - rb->read_ptr;
    n2 = sz2 & rb->size_mask;
  } else {
    n1 = to_read;
    n2 = 0;
  }

  memcpy (dest, &(rb->buf[rb->read_ptr]), n1);
  rb->read_ptr += n1;
  rb->read_ptr &= rb->size_mask;

  if (n2) {
    memcpy (dest + n1, &(rb->buf[rb->read_ptr]), n2);
    rb->read_ptr += n2;
    rb->read_ptr &= rb->size_mask;
  }

  return to_read; 
}

GF_EXPORT
u32 gf_ringbuffer_write (GF_Ringbuffer * rb, const u8 *src, u32 sz)
{
  u32 free_sz, sz2, to_write, n1, n2;

  if ((free_sz = gf_ringbuffer_available_for_write(rb)) == 0) {
    return 0;
  }

  to_write = sz > free_sz ? free_sz : sz;

  sz2 = rb->write_ptr + to_write;

  if (sz2 > rb->size) {
    n1 = rb->size - rb->write_ptr;
    n2 = sz2 & rb->size_mask;
  } else {
    n1 = to_write;
    n2 = 0;
  }

  memcpy (&(rb->buf[rb->write_ptr]), src, n1);
  rb->write_ptr += n1;
  rb->write_ptr &= rb->size_mask;

  if (n2) {
    memcpy (&(rb->buf[rb->write_ptr]), src + n1, n2);
    rb->write_ptr += n2;
    rb->write_ptr &= rb->size_mask;
  }
  return to_write;
}

