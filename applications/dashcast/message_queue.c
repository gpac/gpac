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

void dc_message_queue_init(MessageQueue *mq) {

	memset(mq, 0, sizeof(MessageQueue));
	mq->first_node = NULL;
	mq->last_node = NULL;
	mq->nb_nodes = 0;
#ifdef GPAC_THREAD
	mq->mux = gf_mx_new("MessageQueue Mutex");
#ifdef SEM
	mq->sem = gf_sema_new(1000, 0); //TODO: why 1000 (at other places too)
#else
	mq->cond = gf_cond_new();
#endif
#else
	pthread_mutex_init(&mq->mux, NULL);
	pthread_cond_init(&mq->cond, NULL);
#endif

}

void dc_message_queue_put(MessageQueue *mq, void *data, int size) {

	MessageQueueNode * mqn = gf_malloc(sizeof(MessageQueueNode));
	mqn->data = gf_malloc(size);
	memcpy(mqn->data, data, size);
	mqn->size = size;
	mqn->next = NULL;

#ifdef GPAC_THREAD
	gf_mx_p(mq->mux);
#else
	pthread_mutex_lock(&mq->mux);
#endif

	if (!mq->last_node)
		mq->first_node = mqn;
	else
		mq->last_node->next = mqn;

	mq->last_node = mqn;
	mq->nb_nodes++;

#ifdef GPAC_THREAD
#ifdef SEM
	gf_sema_notify(mq->sem, 1);
#else
	gf_cond_signal(mq->cond);
#endif
	gf_mx_v(mq->mux);
#else
	pthread_cond_signal(&mq->cond);
	pthread_mutex_unlock(&mq->mux);
#endif

}

int dc_message_queue_get(MessageQueue *mq, void * data) {

	int ret = 0;
	MessageQueueNode * mqn;

#ifdef GPAC_THREAD
	gf_mx_p(mq->mux);
#else
	pthread_mutex_lock(&mq->mux);
#endif

	mqn = mq->first_node;

	if(!mqn) {
#ifdef GPAC_THREAD
#ifdef SEM
		gf_mx_v(mq->mux);
		gf_sema_wait(mq->sem);
		gf_mx_p(mq->mux);
#else
		//FIXME: doesn't work: semaphore based replacement above
		gf_cond_wait(mq->cond, mq->mux);
#endif
#else
		pthread_cond_wait(&mq->cond, &mq->mux);
#endif

		mqn = mq->first_node;

		if (!mqn)
			return -1;
	}
	if (mqn) {
		mq->first_node = mqn->next;
		if (!mq->first_node)
			mq->last_node = NULL;
		mq->nb_nodes--;
		memcpy(data, mqn->data, mqn->size);
		ret = mqn->size;
		gf_free(mqn->data);
		gf_free(mqn);
	}

#ifdef GPAC_THREAD
	gf_mx_v(mq->mux);
#else
	pthread_mutex_unlock(&mq->mux);
#endif

	return ret;

}

void dc_message_queue_flush(MessageQueue *mq) {

	MessageQueueNode * mqn, *mqn1;

#ifdef GPAC_THREAD
	gf_mx_p(mq->mux);
#else
	pthread_mutex_lock(&mq->mux);
#endif

	for (mqn = mq->first_node; mqn != NULL; mqn = mqn1) {
		mqn1 = mqn->next;
		gf_free(mqn);
	}
	mq->last_node = NULL;
	mq->first_node = NULL;
	mq->nb_nodes = 0;


#ifdef GPAC_THREAD
#ifdef SEM
	gf_sema_notify(mq->sem, 1);
#else
	gf_cond_signal(mq->cond);
#endif
	gf_mx_v(mq->mux);
#else
	pthread_cond_signal(&mq->cond);
	pthread_mutex_unlock(&mq->mux);
#endif

}

void dc_message_queue_free(MessageQueue *mq) {
	dc_message_queue_flush(mq);
#ifdef GPAC_THREAD
	gf_mx_del(mq->mux);
#ifdef SEM
	gf_sema_del(mq->sem);
#else
	gf_cond_del(mq->cond);
#endif
#else
	pthread_mutex_destroy(&mq->mux);
	pthread_cond_destroy(&mq->cond);
#endif
}

