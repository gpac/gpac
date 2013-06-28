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

void dc_circular_buffer_create(CircularBuffer * p_cb, int i_size, LockMode mode,
		int i_max_con_nb) {

	int i;
	p_cb->i_size = i_size;
	p_cb->p_list = gf_malloc(i_size * sizeof(Node));
	p_cb->mode = mode;
	p_cb->i_max_con_nb = i_max_con_nb;

	for (i = 0; i < i_size; i++) {
		p_cb->p_list[i].i_pro_nb = 0;
		p_cb->p_list[i].i_con_nb = 0;
		p_cb->p_list[i].i_con_access = 0;
		p_cb->p_list[i].i_marked = 0;
		p_cb->p_list[i].i_con_waiting = 0;

#ifdef SEM
		p_cb->p_list[i].con_sem = gf_sema_new(1000, 0);
		p_cb->p_list[i].pro_sem = gf_sema_new(1000, 0);
#else
		p_cb->p_list[i].con_cond = gf_cond_new();
		p_cb->p_list[i].pro_cond = gf_cond_new();
#endif

		p_cb->p_list[i].mux = gf_mx_new("Circular Buffer Mutex");

	}

}

void dc_circular_buffer_destroy(CircularBuffer * p_cb) {

	int i;
	for (i = 0; i < p_cb->i_size; i++) {

#ifdef SEM
		gf_sema_del(p_cb->p_list[i].con_sem);
		gf_sema_del(p_cb->p_list[i].pro_sem);
#else
		gf_cond_del(p_cb->p_list[i].con_cond);
		gf_cond_del(p_cb->p_list[i].pro_cond);
#endif

		gf_mx_del(p_cb->p_list[i].mux);

	}

	gf_free(p_cb->p_list);
}

void dc_consumer_init(Consumer * p_con, int i_max_idx, char * name) {
	p_con->i_idx = 0;
	p_con->i_max_idx = i_max_idx;
	strcpy(p_con->psz_name, name);
}

void * dc_consumer_consume(Consumer * p_con, CircularBuffer * p_cb) {
	return p_cb->p_list[p_con->i_idx].p_data;
}

int dc_consumer_lock(Consumer * p_con, CircularBuffer * p_cb) {

	Node * p_node = &p_cb->p_list[p_con->i_idx];

	gf_mx_p(p_node->mux);
#ifdef DEBUG
	printf("consumer %s enters lock %d\n", p_con->psz_name, p_con->i_idx);
#endif
	if (p_node->i_marked == 2) {
		gf_mx_v(p_node->mux);
		return -1;
	}

	p_node->i_con_waiting++;
	while (p_node->i_pro_nb || !p_node->i_marked) {

#ifdef SEM
		gf_mx_v(p_node->mux);
		gf_sema_wait(p_node->con_sem);
		gf_mx_p(p_node->mux);
#else
		gf_cond_wait(p_node->con_cond, p_node->mux);
#endif

		if (p_node->i_marked == 2) {
			gf_mx_v(p_node->mux);
			return -1;
		}
	}
	p_node->i_con_waiting--;

	if (p_node->i_marked == 2) {
		gf_mx_v(p_node->mux);
		return -1;
	}
	p_node->i_con_nb++;
	p_node->i_con_access++;
#ifdef DEBUG
	printf("consumer %s exits lock %d \n", p_con->psz_name, p_con->i_idx);
#endif
	gf_mx_v(p_node->mux);

	return 0;
}

int dc_consumer_unlock(Consumer * p_con, CircularBuffer * p_cb) {

	int i_last_con = 0;
	Node * p_node = &p_cb->p_list[p_con->i_idx];

	gf_mx_p(p_node->mux);
	p_node->i_con_nb--;

	if (p_node->i_con_access == p_cb->i_max_con_nb) {
		p_node->i_marked = 0;
		p_node->i_con_access = 0;
		i_last_con = 1;
	}

#ifdef SEM
	gf_sema_notify(p_node->pro_sem, 1);
#else
	gf_cond_signal(p_node->pro_cond);
#endif

#ifdef DEBUG
	printf("consumer %s unlock %d \n", p_con->psz_name, p_con->i_idx);
#endif
	gf_mx_v(p_node->mux);

	return i_last_con;
}

int dc_consumer_unlock_previous(Consumer * p_con, CircularBuffer * p_cb) {

	int node_idx = (p_con->i_idx - 1 + p_con->i_max_idx) % p_con->i_max_idx;
	int i_last_con = 0;
	Node * p_node = &p_cb->p_list[node_idx];

	gf_mx_p(p_node->mux);

	p_node->i_con_nb--;
	if (p_node->i_con_nb < 0)
		p_node->i_con_nb = 0;

	if (p_node->i_con_access == p_cb->i_max_con_nb) {
		if (p_node->i_marked != 2)
			p_node->i_marked = 0;
		p_node->i_con_access = 0;
		i_last_con = 1;
	}

#ifdef SEM
	gf_sema_notify(p_node->pro_sem, 1);
#else
	gf_cond_signal(p_node->pro_cond);
#endif

#ifdef DEBUG
	printf("consumer %s unlock %d \n", p_con->psz_name, node_idx);
#endif
	gf_mx_v(p_node->mux);

	return i_last_con;
}

void dc_consumer_advance(Consumer * p_con) {
	p_con->i_idx = (p_con->i_idx + 1) % p_con->i_max_idx;
}

void dc_producer_init(Producer * p_pro, int i_max_idx, char * name) {
	p_pro->i_idx = 0;
	p_pro->i_max_idx = i_max_idx;
	strcpy(p_pro->psz_name, name);
}

void * dc_producer_produce(Producer * p_pro, CircularBuffer * p_cb) {
	return p_cb->p_list[p_pro->i_idx].p_data;

}

int dc_producer_lock(Producer * p_pro, CircularBuffer * p_cb) {

	Node * p_node = &p_cb->p_list[p_pro->i_idx];

	gf_mx_p(p_node->mux);
#ifdef DEBUG
	printf("producer %s enters lock %d \n", p_pro->psz_name, p_pro->i_idx);
#endif

	if ( (p_cb->mode == LIVE_CAMERA || p_cb->mode == LIVE_MEDIA)  && (p_node->i_con_nb || p_node->i_marked)) {
		gf_mx_v(p_node->mux);
		return -1;
	}

	while (p_node->i_con_nb || p_node->i_marked) {

#ifdef SEM
		gf_mx_v(p_node->mux);
		gf_sema_wait(p_node->pro_sem);
		gf_mx_p(p_node->mux);
#else
		gf_cond_wait(p_node->pro_cond, p_node->mux);
#endif

	}

	p_node->i_pro_nb++;
	p_node->i_marked = 1;

#ifdef DEBUG
	printf("producer %s exits lock %d \n", p_pro->psz_name, p_pro->i_idx);
#endif
	gf_mx_v(p_node->mux);

	return 0;
}

void dc_producer_unlock(Producer * p_pro, CircularBuffer * p_cb) {

	Node * p_node = &p_cb->p_list[p_pro->i_idx];

	gf_mx_p(p_node->mux);

	p_node->i_pro_nb--;

#ifdef SEM
	gf_sema_notify(p_node->con_sem, p_node->i_con_waiting);
#else
	gf_cond_broadcast(p_node->con_cond);
#endif

#ifdef DEBUG
	printf("producer %s unlock %d \n", p_pro->psz_name, p_pro->i_idx);
#endif
	gf_mx_v(p_node->mux);

}

void dc_producer_unlock_previous(Producer * p_pro, CircularBuffer * p_cb) {

	int node_idx = (p_pro->i_idx - 1 + p_pro->i_max_idx) % p_pro->i_max_idx;
	Node * p_node = &p_cb->p_list[node_idx];

	gf_mx_p(p_node->mux);

	p_node->i_pro_nb = 0;

#ifdef SEM
	gf_sema_notify(p_node->con_sem, p_node->i_con_waiting);
#else
	gf_cond_broadcast(p_node->con_cond);
#endif

#ifdef DEBUG
	printf("producer %s unlock %d \n", p_pro->psz_name, node_idx);
#endif
	gf_mx_v(p_node->mux);

}

void dc_producer_advance(Producer * p_pro) {
	p_pro->i_idx = (p_pro->i_idx + 1) % p_pro->i_max_idx;
}

void dc_producer_end_signal(Producer * p_pro, CircularBuffer * p_cb) {

	Node * p_node = &p_cb->p_list[p_pro->i_idx];
	gf_mx_p(p_node->mux);

	p_node->i_marked = 2;

#ifdef SEM
	gf_sema_notify(p_node->con_sem, p_node->i_con_waiting);
#else
	gf_cond_broadcast(p_node->con_cond);
#endif

#ifdef DEBUG
	printf("producer %s sends end signal %d \n", p_pro->psz_name, p_pro->i_idx);
#endif
	gf_mx_v(p_node->mux);
}

void dc_producer_end_signal_previous(Producer * p_pro, CircularBuffer * p_cb) {

	int i_node = (p_pro->i_max_idx + p_pro->i_idx - 1) % p_pro->i_max_idx;

	Node * p_node = &p_cb->p_list[i_node];
	gf_mx_p(p_node->mux);

	p_node->i_marked = 2;

#ifdef SEM
	gf_sema_notify(p_node->con_sem, p_node->i_con_waiting);
#else
	gf_cond_broadcast(p_node->con_cond);
#endif

#ifdef DEBUG
	printf("producer %s sends end signal %d \n", p_pro->psz_name, i_node);
#endif
	gf_mx_v(p_node->mux);
}

