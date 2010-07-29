/*
 * StarPU
 * Copyright (C) Université Bordeaux 1, CNRS 2008-2010 (see AUTHORS file)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License in COPYING.LGPL for more details.
 */

#include <starpu.h>
#include <pthread.h>

#define NTHREADS	16
#define NITER		128

//#define DEBUG_MESSAGES	1

static pthread_cond_t cond;
static pthread_mutex_t mutex;

struct thread_data {
	unsigned index;
	unsigned val;
	starpu_data_handle handle;
	pthread_t thread;

	pthread_mutex_t recv_mutex;
	unsigned recv_flag; // set when a message is received
	unsigned recv_buf;
	struct thread_data *neighbour;
};

struct data_req {
	int (*test_func)(void *);
	void *test_arg;
	struct data_req *next;
};

static pthread_mutex_t data_req_mutex;
static pthread_cond_t data_req_cond;
struct data_req *data_req_list;
unsigned progress_thread_running;

static struct thread_data problem_data[NTHREADS];

/* We implement some ring transfer, every thread will try to receive a piece of
 * data from its neighbour and increment it before transmitting it to its
 * successor. */

#ifdef STARPU_USE_CUDA
void cuda_codelet_unsigned_inc(void *descr[], __attribute__ ((unused)) void *cl_arg);
#endif

static void increment_handle_cpu_kernel(void *descr[], void *cl_arg __attribute__((unused)))
{
	unsigned *val = (unsigned *)STARPU_VARIABLE_GET_PTR(descr[0]);
	*val += 1;

//	fprintf(stderr, "VAL %d (&val = %p)\n", *val, val);
}

static starpu_codelet increment_handle_cl = {
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = increment_handle_cpu_kernel,
#ifdef STARPU_USE_CUDA
	.cuda_func = cuda_codelet_unsigned_inc,
#endif
	.nbuffers = 1
};

static void increment_handle_async(struct thread_data *thread_data)
{
	struct starpu_task *task = starpu_task_create();
	task->cl = &increment_handle_cl;

	task->buffers[0].handle = thread_data->handle;
	task->buffers[0].mode = STARPU_RW;

	task->detach = 1;
	task->destroy = 1;

	int ret = starpu_task_submit(task);
	STARPU_ASSERT(!ret);
}

static int test_recv_handle_async(void *arg)
{
//	fprintf(stderr, "test_recv_handle_async\n");

	int ret;
	struct thread_data *thread_data = arg;
	
	pthread_mutex_lock(&thread_data->recv_mutex);

	ret = (thread_data->recv_flag == 1);

	if (ret)
	{
		thread_data->recv_flag = 0;
		thread_data->val = thread_data->recv_buf; 
	}

	pthread_mutex_unlock(&thread_data->recv_mutex);

	if (ret)
	{
#ifdef DEBUG_MESSAGES
		fprintf(stderr, "Thread %d received value %d from thread %d\n",
			thread_data->index, thread_data->val, (thread_data->index - 1)%NTHREADS);
#endif
		starpu_data_release(thread_data->handle);
	}
	
	return ret;
}

static void recv_handle_async(void *_thread_data)
{
	struct thread_data *thread_data = _thread_data;

	struct data_req *req = malloc(sizeof(struct data_req));
	req->test_func = test_recv_handle_async;
	req->test_arg = thread_data;
	req->next = NULL;

	pthread_mutex_lock(&data_req_mutex);
	req->next = data_req_list;
	data_req_list = req;
	pthread_cond_signal(&data_req_cond);
	pthread_mutex_unlock(&data_req_mutex);
}

static int test_send_handle_async(void *arg)
{
	int ret;
	struct thread_data *thread_data = arg;
	struct thread_data *neighbour_data = thread_data->neighbour;
	
	pthread_mutex_lock(&neighbour_data->recv_mutex);
	ret = (neighbour_data->recv_flag == 0);
	pthread_mutex_unlock(&neighbour_data->recv_mutex);

	if (ret)
	{
#ifdef DEBUG_MESSAGES
		fprintf(stderr, "Thread %d sends value %d to thread %d\n", thread_data->index, thread_data->val, neighbour_data->index);
#endif
		starpu_data_release(thread_data->handle);
	}

	return ret;
}

static void send_handle_async(void *_thread_data)
{
	struct thread_data *thread_data = _thread_data;
	struct thread_data *neighbour_data = thread_data->neighbour;

//	fprintf(stderr, "send_handle_async\n");

	/* send the message */
	pthread_mutex_lock(&neighbour_data->recv_mutex);
	neighbour_data->recv_buf = thread_data->val;
	neighbour_data->recv_flag = 1;
	pthread_mutex_unlock(&neighbour_data->recv_mutex);

	struct data_req *req = malloc(sizeof(struct data_req));
	req->test_func = test_send_handle_async;
	req->test_arg = thread_data;
	req->next = NULL;

	pthread_mutex_lock(&data_req_mutex);
	req->next = data_req_list;
	data_req_list = req;
	pthread_cond_signal(&data_req_cond);
	pthread_mutex_unlock(&data_req_mutex);
}

static void *progress_func(void *arg)
{
	pthread_mutex_lock(&data_req_mutex);

	progress_thread_running = 1;
	pthread_cond_signal(&data_req_cond);	

	while (progress_thread_running) {
		struct data_req *req;

		if (data_req_list == NULL)
			pthread_cond_wait(&data_req_cond, &data_req_mutex);

		req = data_req_list;

		if (req)
		{
			data_req_list = req->next;
			req->next = NULL;

			pthread_mutex_unlock(&data_req_mutex);

			int ret = req->test_func(req->test_arg);

			if (ret)
			{
				free(req);
				pthread_mutex_lock(&data_req_mutex);
			}
			else {
				/* ret = 0 : the request is not finished, we put it back at the end of the list */
				pthread_mutex_lock(&data_req_mutex);

				struct data_req *req_aux = data_req_list;
				if (!req_aux)
				{
					/* The list is empty */
					data_req_list = req;
				}
				else {
					while (req_aux)
					{
						if (req_aux->next == NULL)
						{
							req_aux->next = req;
							break;
						}
						
						req_aux = req_aux->next;
					}
				}
			}
		}
	}
	pthread_mutex_unlock(&data_req_mutex);

	return NULL;
}

static void *thread_func(void *arg)
{
	unsigned iter;
	struct thread_data *thread_data = arg;
	unsigned index = thread_data->index;

	starpu_variable_data_register(&thread_data->handle, 0, (uintptr_t)&thread_data->val, sizeof(unsigned));

	for (iter = 0; iter < NITER; iter++)
	{
		/* The first thread initiates the first transfer */
		if (!((index == 0) && (iter == 0)))
		{
			starpu_data_acquire_cb(
				thread_data->handle, STARPU_W,
				recv_handle_async, thread_data
			);
		}
		
		increment_handle_async(thread_data);

		if (!((index == (NTHREADS - 1)) && (iter == (NITER - 1))))
		{
			starpu_data_acquire_cb(
				thread_data->handle, STARPU_R,
				send_handle_async, thread_data
			);
		}
	}

	starpu_task_wait_for_all();

	return NULL;
}

int main(int argc, char **argv)
{
	int ret;
	void *retval;

	starpu_init(NULL);

	/* Create a thread to perform blocking calls */
	pthread_t progress_thread;
	pthread_mutex_init(&data_req_mutex, NULL);
	pthread_cond_init(&data_req_cond, NULL);
	data_req_list = NULL;
	progress_thread_running = 0;

	unsigned t;
	for (t = 0; t < NTHREADS; t++)
	{
		problem_data[t].index = t;
		problem_data[t].val = 0;
		pthread_mutex_init(&problem_data[t].recv_mutex, NULL);
		problem_data[t].recv_flag = 0;
		problem_data[t].neighbour = &problem_data[(t+1)%NTHREADS];
	}

	pthread_create(&progress_thread, NULL, progress_func, NULL);

	pthread_mutex_lock(&data_req_mutex);
	while (!progress_thread_running)
		pthread_cond_wait(&data_req_cond, &data_req_mutex);
	pthread_mutex_unlock(&data_req_mutex);

	for (t = 0; t < NTHREADS; t++)
	{
		ret = pthread_create(&problem_data[t].thread, NULL, thread_func, &problem_data[t]);
		STARPU_ASSERT(!ret);
	}

	for (t = 0; t < NTHREADS; t++)
	{
		ret = pthread_join(problem_data[t].thread, &retval);
		STARPU_ASSERT(retval == NULL);
	}

	pthread_mutex_lock(&data_req_mutex);
	progress_thread_running = 0;
	pthread_cond_signal(&data_req_cond);
	pthread_mutex_unlock(&data_req_mutex);

	ret = pthread_join(progress_thread, &retval);
	STARPU_ASSERT(retval == NULL);

	/* We check that the value in the "last" thread is valid */
	starpu_data_handle last_handle = problem_data[NTHREADS - 1].handle;
	starpu_data_acquire(last_handle, STARPU_R);
	if (problem_data[NTHREADS - 1].val != (NTHREADS * NITER))
	{
		fprintf(stderr, "Final value : %d should be %d\n", problem_data[NTHREADS - 1].val, (NTHREADS * NITER));
		STARPU_ABORT();
	}
	starpu_data_release(last_handle);

	starpu_shutdown();

	return 0;
}
