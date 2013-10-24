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

#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_

#include <stdlib.h>
#include <stdio.h>
#include <gpac/thread.h>


/*
 * The method (mode) of multithread management.
 * It can be LIVE or OFFLINE.
 * LIVE means that the system is real time. The producer does not wait
 * for anyone and it always produce and find a place on the circular buffer.
 * OFFLINE means that the system is working offline, so the producer can
 * wait for the consumers to finish their job.
 */
typedef enum {
	LIVE_CAMERA,
	LIVE_MEDIA,
	ON_DEMAND
} LockMode;

/*
 * Every node of the circular buffer has a data, plus
 * all the variables needed for multithread management. 
 */
typedef struct {
	/* Pointer to the data on the node */
	void *data;
	/* The number of the producer currently using this node */
	int num_producers;
	/* The number of consumer currently using this node */
	int num_consumers;
	/* The number of consumer currently waiting for this node */
	int num_consumers_waiting;
	/* Mutex used for synchronizing the users of this node. */
	GF_Mutex *mutex;
	/* Semaphore for producer */
	GF_Semaphore *producers_semaphore;
	/* Semaphore for consumers */
	GF_Semaphore *consumers_semaphore;
	/* If marked is 0 it means the data on this node is not valid.
	 * If marked is 1 it means that the data on this node is valid.
	 * If marked is 2 it means this node is the last node.
	 */
	int marked;
	/*
	 * Indicates the number of consumers which already accessed this node.
	 * It is used for the case where the last consumer has to do something.
	 */
	int num_consumers_accessed;
} Node;

/*
 * The circular buffer has a size, a list of nodes and it
 * has the number of consumers using it. Also it needs to know which
 * locking mechanism it needs to use. (LIVE or OFFLINE)
 */
typedef struct {
	/* The size of circular buffer */
	int size;	
	/* A list of all the nodes */
	Node *list;	
	/* The mode for multithread management. */
	LockMode mode;	
	/* The maximum number of the consumers using the circular buffer */
	int max_con_nb;
} CircularBuffer;

/*
 * Producer has an index to the circular buffer.
 */ 
typedef struct {
	/* The index where the producer is using */
	int idx;
	/* The maximum of the index. (Which means the size of circular buffer) */
	int max_idx;

	char name[256];
} Producer;

/*
 * Consumer has an index to the circular buffer.
 */
typedef struct {
	/* The index where the consumer is using */
	int idx;
	/* The maximum of the index. (Which means the size of circular buffer) */
	int max_idx;

	char name[256];
} Consumer;

/*
 * Create a circular buffer
 * 
 * @param circular_buf [out] circular buffer to be created
 * @param size [in] size of circular buffer
 * @param mode [in] mode of multithread management (LIVE or OFFLINE)
 * @param num_consumers [in] maximum number of the consumers of the circular buffer
 */
void dc_circular_buffer_create(CircularBuffer *circular_buf, int size, LockMode mode, int num_consumers);

/*
 * Destroy the circular buffer
 *
 * @param circular_buf [in] circular buffer to be destroyed
 */
void dc_circular_buffer_destroy(CircularBuffer *circular_buf);

/* 
 * Initialize a consumer
 * 
 * @param consumer [out] the consumer to be initialize
 * @param num_consumers [in] maximum number of the consumers
 */
void dc_consumer_init(Consumer *consumer, int num_consumers, char *name);

/*
 * Return the data in the node in question. (circular_buf[consumer index])
 *
 * @param consumer [in] consumer
 * @param circular_buf [in] circular buffer
 */
void * dc_consumer_consume(Consumer *consumer, CircularBuffer *circular_buf);

/*
 * Consumer lock on circular buffer
 * 
 * @param consumer [in] consumer
 * @param circular_buf [in] circular buffer
 *
 * @return 0 on success, -1 if the node in question is the last node and not usable. 
 */ 
int dc_consumer_lock(Consumer *consumer, CircularBuffer *circular_buf);

/*
 * Consumer unlock on circular buffer
 *
 * @param consumer [in] consumer
 * @param circular_buf [in] circular buffer
 *
 * @return 0 on normal exit, 1 if the consumer unlocking this node is the last consumer.
 */
int dc_consumer_unlock(Consumer *consumer, CircularBuffer *circular_buf);

/*
 * Consumer unlock on previous node of the circular buffer
 *
 * @param consumer [in] consumer
 * @param circular_buf [in] circular buffer
 *
 * @return 0 on normal exit, 1 if the consumer unlocking this node is the last consumer.
 */
int dc_consumer_unlock_previous(Consumer *consumer, CircularBuffer *circular_buf);

/*
 * Consumer leads its index
 * 
 * @param consumer [in] consumer
 */
void dc_consumer_advance(Consumer *consumer);

/* 
 * Initialize a producer
 * 
 * @param producer [out] the producer to be initialize
 * @param maxpro [in] maximum number of the producers
 */
void dc_producer_init(Producer *producer, int maxpro, char *name);

/*
 * Return the data in the node in question. (circular_buf[consumer index])
 *
 * @param producer [in] producer
 * @param circular_buf [in] circular buffer
 */
void * dc_producer_produce(Producer *producer, CircularBuffer *circular_buf);

/*
 * Producer lock on circular buffer
 * 
 * @param producer [in] producer
 * @param circular_buf [in] circular buffer
 *
 * @return 0 on success, -1 if the mode is live and cannot wait.
 */ 
int dc_producer_lock(Producer *producer, CircularBuffer *circular_buf);

/*
 * Producer unlock on circular buffer
 *
 * @param producer [in] producer
 * @param circular_buf [in] circular buffer
 */
void dc_producer_unlock(Producer *producer, CircularBuffer *circular_buf);

/*
 * Producer unlock on the previous node of the circular buffer
 *
 * @param producer [in] producer
 * @param circular_buf [in] circular buffer
 */
void dc_producer_unlock_previous(Producer *, CircularBuffer *);

/*
 * Producer leads its index
 * 
 * @param producer [in] producer
 */
void dc_producer_advance(Producer *producer);

/*
 * Producer signal that the current node is the last node
 * 
 * @param producer [in] producer
 * @param circular_buf [in] circular buffer
 */
void dc_producer_end_signal(Producer *producer, CircularBuffer *circular_buf);

void dc_producer_end_signal_previous(Producer *producer, CircularBuffer *circular_buf);

#endif /* CIRCULAR_BUFFER_H_ */
