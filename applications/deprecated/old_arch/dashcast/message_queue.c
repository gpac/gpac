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

#include "message_queue.h"


void dc_message_queue_init(MessageQueue *mq)
{
	memset(mq, 0, sizeof(MessageQueue));
	mq->first_node = NULL;
	mq->last_node = NULL;
	mq->nb_nodes = 0;
	mq->mutex = gf_mx_new("MessageQueue Mutex");
	mq->sem = gf_sema_new(1000, 0); //TODO: why 1000 (at other places too)
}

void dc_message_queue_put(MessageQueue *mq, void *data, u32 size)
{
	MessageQueueNode *mqn = (MessageQueueNode*)gf_malloc(sizeof(MessageQueueNode));
	mqn->data = gf_malloc(size);
	memcpy(mqn->data, data, size);
	mqn->size = size;
	mqn->next = NULL;

	gf_mx_p(mq->mutex);

	if (!mq->last_node)
		mq->first_node = mqn;
	else
		mq->last_node->next = mqn;

	mq->last_node = mqn;
	mq->nb_nodes++;

	gf_sema_notify(mq->sem, 1);
	gf_mx_v(mq->mutex);
}

int dc_message_queue_get(MessageQueue *mq, void * data)
{
	int ret = 0;
	MessageQueueNode *mqn;

	gf_mx_p(mq->mutex);

	mqn = mq->first_node;
	if (!mqn) {
		gf_mx_v(mq->mutex);
		ret = gf_sema_wait_for(mq->sem, 10000);
		gf_mx_p(mq->mutex);

		mqn = mq->first_node;

		if (!ret || !mqn) {
			gf_mx_v(mq->mutex);
			return -1;
		}
	}
	if (mqn) {
		mq->first_node = mqn->next;
		if (!mq->first_node)
			mq->last_node = NULL;
		mq->nb_nodes--;
		memcpy(data, mqn->data, mqn->size);
		ret = (int)mqn->size;
		gf_free(mqn->data);
		gf_free(mqn);
	}

	gf_mx_v(mq->mutex);

	return ret;
}

void dc_message_queue_flush(MessageQueue *mq)
{
	MessageQueueNode *mqn, *mqn1;

	gf_mx_p(mq->mutex);

	for (mqn = mq->first_node; mqn != NULL; mqn = mqn1) {
		mqn1 = mqn->next;
		gf_free(mqn);
	}
	mq->last_node = NULL;
	mq->first_node = NULL;
	mq->nb_nodes = 0;

	gf_sema_notify(mq->sem, 1);
	gf_mx_v(mq->mutex);
}

void dc_message_queue_free(MessageQueue *mq)
{
	dc_message_queue_flush(mq);
	gf_mx_del(mq->mutex);
	gf_sema_del(mq->sem);
}
