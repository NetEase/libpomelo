

#if defined(__ANDROID__)

#include <uv-unix.h>

int pthread_barrier_init(pthread_barrier_t* barrier,
		const void* barrier_attr,
		unsigned count)
{
	barrier->count = count;
	pthread_mutex_init(&barrier->mutex, NULL);
	pthread_cond_init(&barrier->cond, NULL);

	return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier)
{
	int res = 0;
	pthread_mutex_lock(&barrier->mutex);

	barrier->count -= 1;
	if (barrier->count > 0) {
		while (barrier->count > 0) {
			pthread_cond_wait(&barrier->cond, &barrier->mutex);
		}
	}
	else {
		pthread_cond_broadcast(&barrier->cond);
		res = PTHREAD_BARRIER_SERIAL_THREAD;
	}

	pthread_mutex_unlock(&barrier->mutex);

	return res;
}

int pthread_barrier_destroy(pthread_barrier_t* barrier)
{
	barrier->count = 0;
	pthread_cond_destroy(&barrier->cond);
	pthread_mutex_destroy(&barrier->mutex);

	return 0;
}

#endif

