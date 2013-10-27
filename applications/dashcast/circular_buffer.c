/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Arash Shafiei
 *			Copyright (c) Telecom ParisTech 2000-2013
 *					All rights reserved
 *
 *  This file is part of GPAC / dashcast
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

#include "circular_buffer.h"


//#define DEBUG


void dc_circular_buffer_create(CircularBuffer *circular_buf, u32 size, LockMode mode, int max_num_consumers)
{
	u32 i;
	circular_buf->size = size;
	circular_buf->list = gf_malloc(size * sizeof(Node));
	circular_buf->mode = mode;
	circular_buf->max_num_consumers = max_num_consumers;

	for (i=0; i<size; i++) {
		circular_buf->list[i].num_producers = 0;
		circular_buf->list[i].num_consumers = 0;
		circular_buf->list[i].num_consumers_accessed = 0;
		circular_buf->list[i].marked = 0;
		circular_buf->list[i].num_consumers_waiting = 0;
		circular_buf->list[i].consumers_semaphore = gf_sema_new(1000, 0);
		circular_buf->list[i].producers_semaphore = gf_sema_new(1000, 0);
		circular_buf->list[i].mutex = gf_mx_new("Circular Buffer Mutex");
	}
}

void dc_circular_buffer_destroy(CircularBuffer *circular_buf)
{
	u32 i;
	for (i = 0; i < circular_buf->size; i++) {
		gf_sema_del(circular_buf->list[i].consumers_semaphore);
		gf_sema_del(circular_buf->list[i].producers_semaphore);
		gf_mx_del(circular_buf->list[i].mutex);
	}

	gf_free(circular_buf->list);
}

void dc_consumer_init(Consumer *consumer, int max_idx, char *name)
{
	consumer->idx = 0;
	consumer->max_idx = max_idx;
	strcpy(consumer->name, name);
}

void * dc_consumer_consume(Consumer *consumer, CircularBuffer *circular_buf)
{
	return circular_buf->list[consumer->idx].data;
}

int dc_consumer_lock(Consumer *consumer, CircularBuffer *circular_buf)
{
	Node *node = &circular_buf->list[consumer->idx];

	gf_mx_p(node->mutex);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("consumer %s enters lock %d\n", consumer->name, consumer->idx));
	if (node->marked == 2) {
		gf_mx_v(node->mutex);
		return -1;
	}

	node->num_consumers_waiting++;
	while (node->num_producers || !node->marked) {
		gf_mx_v(node->mutex);
		gf_sema_wait(node->consumers_semaphore);
		gf_mx_p(node->mutex);

		if (node->marked == 2) {
			gf_mx_v(node->mutex);
			return -1;
		}
	}
	node->num_consumers_waiting--;

	if (node->marked == 2) {
		gf_mx_v(node->mutex);
		return -1;
	}
	node->num_consumers++;
	node->num_consumers_accessed++;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("consumer %s exits lock %d \n", consumer->name, consumer->idx));
	gf_mx_v(node->mutex);

	return 0;
}

int dc_consumer_unlock(Consumer *consumer, CircularBuffer *circular_buf)
{
	int last_consumer = 0;
	Node *node = &circular_buf->list[consumer->idx];

	gf_mx_p(node->mutex);
	node->num_consumers--;

	if (node->num_consumers_accessed == circular_buf->max_num_consumers) {
		node->marked = 0;
		node->num_consumers_accessed = 0;
		last_consumer = 1;
	}

	gf_sema_notify(node->producers_semaphore, 1);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("consumer %s unlock %d \n", consumer->name, consumer->idx));
	gf_mx_v(node->mutex);

	return last_consumer;
}

int dc_consumer_unlock_previous(Consumer *consumer, CircularBuffer *circular_buf)
{
	int node_idx = (consumer->idx - 1 + consumer->max_idx) % consumer->max_idx;
	int last_consumer = 0;
	Node *node = &circular_buf->list[node_idx];

	gf_mx_p(node->mutex);

	node->num_consumers--;
	if (node->num_consumers < 0)
		node->num_consumers = 0;

	if (node->num_consumers_accessed == circular_buf->max_num_consumers) {
		if (node->marked != 2)
			node->marked = 0;
		node->num_consumers_accessed = 0;
		last_consumer = 1;
	}

	gf_sema_notify(node->producers_semaphore, 1);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("consumer %s unlock %d \n", consumer->name, node_idx));
	gf_mx_v(node->mutex);

	return last_consumer;
}

void dc_consumer_advance(Consumer *consumer)
{
	consumer->idx = (consumer->idx + 1) % consumer->max_idx;
}

void dc_producer_init(Producer *producer, int max_idx, char *name)
{
	producer->idx = 0;
	producer->max_idx = max_idx;
	strcpy(producer->name, name);
}

void * dc_producer_produce(Producer *producer, CircularBuffer *circular_buf)
{
	return circular_buf->list[producer->idx].data;
}

int dc_producer_lock(Producer *producer, CircularBuffer *circular_buf)
{
	Node *node = &circular_buf->list[producer->idx];

	gf_mx_p(node->mutex);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("producer %s enters lock %d \n", producer->name, producer->idx));

	if ( (circular_buf->mode == LIVE_CAMERA || circular_buf->mode == LIVE_MEDIA) && (node->num_consumers || node->marked)) {
		gf_mx_v(node->mutex);
		return -1;
	}

	while (node->num_consumers || node->marked) {
		gf_mx_v(node->mutex);
		gf_sema_wait(node->producers_semaphore);
		gf_mx_p(node->mutex);
	}

	node->num_producers++;
	node->marked = 1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("producer %s exits lock %d \n", producer->name, producer->idx));
	gf_mx_v(node->mutex);

	return 0;
}

void dc_producer_unlock(Producer *producer, CircularBuffer *circular_buf)
{
	Node *node = &circular_buf->list[producer->idx];

	gf_mx_p(node->mutex);
	node->num_producers--;
	gf_sema_notify(node->consumers_semaphore, node->num_consumers_waiting);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("producer %s unlock %d \n", producer->name, producer->idx));
	gf_mx_v(node->mutex);
}

void dc_producer_unlock_previous(Producer *producer, CircularBuffer *circular_buf)
{
	int node_idx = (producer->idx - 1 + producer->max_idx) % producer->max_idx;
	Node *node = &circular_buf->list[node_idx];

	gf_mx_p(node->mutex);
	node->num_producers = 0;
	gf_sema_notify(node->consumers_semaphore, node->num_consumers_waiting);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("producer %s unlock %d \n", producer->name, node_idx));
	gf_mx_v(node->mutex);
}

void dc_producer_advance(Producer *producer)
{
	producer->idx = (producer->idx + 1) % producer->max_idx;
}

void dc_producer_end_signal(Producer *producer, CircularBuffer *circular_buf)
{
	Node *node = &circular_buf->list[producer->idx];

	gf_mx_p(node->mutex);
	node->marked = 2;
	gf_sema_notify(node->consumers_semaphore, node->num_consumers_waiting);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("producer %s sends end signal %d \n", producer->name, producer->idx));
	gf_mx_v(node->mutex);
}

void dc_producer_end_signal_previous(Producer *producer, CircularBuffer *circular_buf)
{
	int i_node = (producer->max_idx + producer->idx - 1) % producer->max_idx;
	Node *node = &circular_buf->list[i_node];

	gf_mx_p(node->mutex);
	node->marked = 2;
	gf_sema_notify(node->consumers_semaphore, node->num_consumers_waiting);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("producer %s sends end signal %d \n", producer->name, node));
	gf_mx_v(node->mutex);
}
