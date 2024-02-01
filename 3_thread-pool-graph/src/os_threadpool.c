// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#ifdef DEBUG_WORKLOAD
static unsigned long long thread_work[4] = {0};
static unsigned long long thread_id[4] = {0};
static int thread_maped;

void addwork(unsigned long long task_id)
{
	unsigned long long i = 0;
	unsigned long long fnd = 0;

	for (i = 0 ; i < 4; ++i) {
		if (thread_id[i] == task_id) {
			fnd = 1;
			break;
		}
	}

	if (!fnd || i >= 4) {
		i = thread_maped;
		thread_id[i] = task_id;
		thread_maped++;
	}

	thread_work[i]++;
}

int getwork(unsigned long long task_id)
{
	int i = 0;

	for (i = 0 ; i < 4; ++i) {
		if (thread_id[i] == task_id)
			return thread_work[i];
	}
	return -1;
}

int printwork(void)
{
	for (int i = 0 ; i < 4; ++i)
		log_error("Thread: %lu --> tasks# %lu\n", thread_id[i], thread_work[i]);
}
#endif
/* Create a task that would be executed by a thread. */
os_task_t *create_task(void (*action)(void *), void *arg, void (*destroy_arg)(void *))
{
	os_task_t *t;

	t = malloc(sizeof(*t));
	DIE(t == NULL, "malloc");

	t->action = action;		// the function
	t->argument = arg;		// arguments for the function
	t->destroy_arg = destroy_arg;	// destroy argument function
	return t;
}

/* Destroy task. */
void destroy_task(os_task_t *t)
{
	if (t->destroy_arg != NULL)
		t->destroy_arg(t->argument);
	free(t);
}

/* Put a new task to threadpool task queue. */
void enqueue_task(os_threadpool_t *tp, os_task_t *t)
{
	assert(tp != NULL);
	assert(t != NULL);
	pthread_mutex_lock(&tp->lock);
	list_add_tail(&tp->head, &t->list);
	pthread_mutex_unlock(&tp->lock);
	tp->begin = 1;
	pthread_cond_signal(&tp->cond);
}

/*
 * Check if queue is empty.
 * This function should be called in a synchronized manner.
 */
static int queue_is_empty(os_threadpool_t *tp)
{
	return list_empty(&tp->head);
}

/*
 * Get a task from threadpool task queue.
 * Block if no task is available.
 * Return NULL if work is complete, i.e. no task will become available,
 * i.e. all threads are going to block.
 */

os_task_t *dequeue_task(os_threadpool_t *tp)
{
	os_task_t *t;

	pthread_mutex_lock(&tp->lock);
	while (queue_is_empty(tp)) {
		tp->idle_threads = tp->idle_threads + 1;
		if (tp->idle_threads >= tp->num_threads && tp->begin == 1) {
			tp->sig_terminate = 1;

			pthread_mutex_unlock(&tp->lock);
			pthread_cond_signal(&tp->cond_term);
			return NULL;
		}
		pthread_cond_wait(&tp->cond, &tp->lock);
		tp->begin = 1;
		tp->idle_threads--;
	}

	if (tp->sig_terminate) {
		tp->idle_threads++;
		pthread_mutex_unlock(&tp->lock);
		return NULL;
	}
	os_task_t *popped = list_entry(tp->head.next, os_task_t, list);

#ifdef DEBUG_WORKLOAD
	addwork(pthread_self());
#endif
	list_del(tp->head.next);
	pthread_mutex_unlock(&tp->lock);

	return popped;
}

/* Loop function for threads */
static void *thread_loop_function(void *arg)
{
	os_threadpool_t *tp = (os_threadpool_t *) arg;

	while (1) {
		os_task_t *t;

		t = dequeue_task(tp);
		if (t == NULL)
			break;

		t->action(t->argument);
		destroy_task(t);
	}

	return NULL;
}

/* Wait completion of all threads. This is to be called by the main thread. */
void wait_for_completion(os_threadpool_t *tp)
{
	pthread_cond_wait(&tp->cond_term, &tp->term_lock);
	pthread_cond_broadcast(&tp->cond);

	for (unsigned int i = 0; i < tp->num_threads; i++)
		pthread_join(tp->threads[i], NULL);
#ifdef DEBUG_WORKLOAD
	printwork();
#endif
}

/* Create a new threadpool. */
os_threadpool_t *create_threadpool(unsigned int num_threads)
{
	os_threadpool_t *tp = NULL;
	int rc;

	tp = malloc(sizeof(*tp));
	DIE(tp == NULL, "malloc");

	list_init(&tp->head);

	if (pthread_mutex_init(&(tp->lock), NULL) != 0)
		DIE(1, "mutex_init");
	if (pthread_cond_init(&(tp->cond), NULL) != 0)
		DIE(1, "cond_init");
	if (pthread_mutex_init(&(tp->term_lock), NULL) != 0)
		DIE(1, "mutext_init");
	if (pthread_cond_init(&(tp->cond_term), NULL) != 0)
		DIE(1, "condt_init");

	tp->sig_terminate = 0;
	tp->idle_threads = 0;
	tp->begin = 0;
	tp->num_threads = num_threads;
	tp->threads = malloc(num_threads * sizeof(*tp->threads));
	DIE(tp->threads == NULL, "malloc");
	for (unsigned int i = 0; i < num_threads; ++i) {
		rc = pthread_create(&tp->threads[i], NULL, &thread_loop_function, (void *) tp);
		DIE(rc < 0, "pthread_create");
	}
	return tp;
}

/* Destroy a threadpool. Assume all threads have been joined. */
void destroy_threadpool(os_threadpool_t *tp)
{
	os_list_node_t *n, *p;

	pthread_mutex_destroy(&tp->lock);
	pthread_cond_destroy(&tp->cond);

	list_for_each_safe(n, p, &tp->head) {
		list_del(n);
		destroy_task(list_entry(n, os_task_t, list));
	}

	free(tp->threads);
	free(tp);
}
