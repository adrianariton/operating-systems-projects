// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <stdatomic.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4
static atomic_int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;

// #define SLEEP_CHECK

static void graph_destroy_arg(void *arg)
{
	(void)arg;
}

static void graph_action(void *arg)
{
	pthread_mutex_lock(&tp->lock);
	long long idx = (long long)arg;

	graph->visited[idx] = DONE;
	pthread_mutex_unlock(&tp->lock);

#ifdef SLEEP_CHECK
	sleep(5);
	printf("Thread %d ][ Node #%d\n", gettid(), idx);
#endif

	for (int i = 0; i < graph->nodes[idx]->num_neighbours; ++i) {
		int node_id = graph->nodes[idx]->neighbours[i];

		pthread_mutex_lock(&tp->lock);
		if (graph->visited[node_id] == NOT_VISITED) {
			graph->visited[node_id] = PROCESSING;
			os_task_t *task = create_task(graph_action, (void *)node_id, graph_destroy_arg);

			pthread_mutex_unlock(&tp->lock);
			enqueue_task(tp, task);
		} else {
			pthread_mutex_unlock(&tp->lock);
		}
	}
	atomic_fetch_add(&sum, graph->nodes[idx]->info);
	DEBUG_PRINT("%d : adding : %d => %d\n", gettid(), graph->nodes[idx]->info, sum);
}

static void process_node(unsigned int idx)
{
	os_node_t *node;

	node = graph->nodes[idx];
	graph->visited[idx] = PROCESSING;
	DEBUG_PRINT("\n\n> Creating task...\n");

	os_task_t *task = create_task(&graph_action, (void *)idx, &graph_destroy_arg);

	DEBUG_PRINT("> Created task, enqueueing.... %p\n", task->action);
	enqueue_task(tp, task);
}

int abs(int x)
{
	if (x < 0)
		return -x;
	return x;
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

#ifdef TIME_IT
	clock_t begin = clock();
#endif

	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);
	fflush(stdout);

	printf("%d", (int)sum);
#ifdef TIME_IT
	clock_t end = clock();
	double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

	printf("Time: %lf", time_spent);
#endif
	return 0;
}
