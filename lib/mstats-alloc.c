/*
 * CS 240
 * The University of Illinois
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <execinfo.h>
#include <signal.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#include "mstats-alloc.h"

void *alloc_handle = NULL;

void *(*alloc_calloc)(size_t nmemb, size_t size) = NULL;
void *(*alloc_malloc)(size_t size) = NULL;
void  (*alloc_free)(void *ptr) = NULL;
void *(*alloc_realloc)(void *ptr, size_t size) = NULL;

void *(*libc_calloc)(size_t nmemb, size_t size) = NULL;
void *(*libc_malloc)(size_t size) = NULL;
void (*libc_free)(void *ptr) = NULL;
void *(*libc_realloc)(void *ptr, size_t size) = NULL;

#ifdef __APPLE__
void *(*mmap_sbrk)(intptr_t increment) = NULL;
void *(*libc_sbrk)(intptr_t increment) = NULL;
#endif

void *sbrk_start = 0;
void *sbrk_largest = 0;
void *sbrk_init_done = 0;

char _buffer[4096];
const void *buffer_start = (void *)_buffer;
void *buffer = (void *)_buffer;

alloc_stats_t *stats = NULL;

int alloc_init_stage = 0;

void stats_alloc_init() {
  /*
   * Phase 1: Store references to the system's (libc) alloc library.
   * - Assuming that malloc() and other calls are unavailable.
   */
	alloc_init_stage = 1;

	#ifdef USE_LIBC_ALLOC
  printf("[mstats-alloc]: Injecting stat tracking into libc's malloc.\n");	
	#else
  printf("[mstats-alloc]: Injecting your alloc library into the running process.\n");
	#endif
	
	// Tell malloc() not to use mmap() (for contact tracing):
	#ifndef __APPLE__
	mallopt(M_MMAP_MAX, 0);	
	#endif
	libc_calloc  = dlsym(RTLD_NEXT, "calloc");
	libc_malloc  = dlsym(RTLD_NEXT, "malloc");
	libc_free    = dlsym(RTLD_NEXT, "free");
	libc_realloc = dlsym(RTLD_NEXT, "realloc");

  #ifdef __APPLE__
  libc_sbrk    = dlsym(RTLD_NEXT, "sbrk");

	void *sbrk_handle = dlopen("./lib/osx-sbrk-mmap-wrapper.so", RTLD_NOW | RTLD_GLOBAL);
	if (!sbrk_handle) {
		char *err =  dlerror();

		if (!err) { fprintf(stderr, "A dynamic linking error occurred: (%s)\n", err); }
		else      { fprintf(stderr, "An unknown dynamic linking error occurred.\n"); }
		exit(90);
	}

  mmap_sbrk = dlsym(sbrk_handle, "sbrk");
	if (!mmap_sbrk) {
		fprintf(stderr, "Unable to dynamicly load a required memory allocation call.\n");
		exit(91);
	}
  #endif


  /*
   * Phase 2: Replace calloc(), malloc(), free(), and realloc() with the versions
   *          defined in alloc.so (from alloc.c).
   * - Calls to alloc-related functions during this phase get directed to libc allocators.
   */
	alloc_init_stage = 2;

	#ifdef USE_LIBC_ALLOC
	alloc_calloc  = libc_calloc;
	alloc_malloc  = libc_malloc;
	alloc_free    = libc_free;
	alloc_realloc = libc_realloc;	
	#else		
	alloc_handle = dlopen("./alloc.so", RTLD_NOW | RTLD_GLOBAL);
	if (!alloc_handle) {
		char *err =  dlerror();

		if (!err) { fprintf(stderr, "A dynamic linking error occurred: (%s)\n", err); }
		else      { fprintf(stderr, "An unknown dynamic linking error occurred.\n"); }
		exit(65);
	}

	alloc_calloc  = dlsym(alloc_handle, "calloc");
	alloc_malloc  = dlsym(alloc_handle, "malloc");
	alloc_free    = dlsym(alloc_handle, "free");
	alloc_realloc = dlsym(alloc_handle, "realloc");

	if (!alloc_calloc || !alloc_malloc || !alloc_free || !alloc_realloc) {
		fprintf(stderr, "Unable to dynamicly load a required memory allocation call.\n");
		exit(66);
	}
	#endif

	
	char *file_name = getenv("ALLOC_STATS_MMAP");
	int fd = open(file_name, O_RDWR);
	stats = mmap(NULL, sizeof(alloc_stats_t), PROT_WRITE, MAP_SHARED, fd, 0);

	if (fd <= 0 || stats == (void *)-1) {
		fprintf(stderr, "fd/mmap");
		exit(67);
	}
	
	stats->max_heap_used = 0;
	stats->memory_heap_sum = 0;
	stats->memory_uses = 0;
	
	sbrk_init_done = sbrk(0);
	sbrk_start = sbrk_largest = sbrk(0);
	alloc_init_stage = 3;
}


void stats_tracking() {
	void *sbrk_current = sbrk(0);
	unsigned long current_mem_usage = ((long)sbrk_current - (long)sbrk_start);
	
	if (stats->max_heap_used < current_mem_usage) {
		sbrk_largest = sbrk_current;
		stats->max_heap_used = current_mem_usage;

		if (current_mem_usage > ((double)1024) * ((double)1024) * ((double)1024) * 2)
		{
			fprintf(stderr, "Exceeded 2 GB\n");
			exit(68);
		}
	}
	
	stats->memory_heap_sum += current_mem_usage;
	stats->memory_uses++;
}


#ifdef __APPLE__
void *sbrk(int increment) {
	if (alloc_init_stage == 0) { stats_alloc_init(); }

  if (mmap_sbrk) {
    return mmap_sbrk(increment);
  } else if (libc_sbrk) {
    return libc_sbrk(increment);
  } else {
    return NULL;
  }
};
#endif


void *calloc(size_t nmemb, size_t size) {
	if (alloc_init_stage == 0) {
		stats_alloc_init();
	} else if (alloc_init_stage == 1 || alloc_init_stage == 2) {
		memset(buffer, 0x00, nmemb * size);
		buffer += nmemb * size;
		return buffer - (nmemb * size);
	}

	void *addr = alloc_calloc(nmemb, size);
	stats_tracking();

	return addr;
}


void *malloc(size_t size) {
	if (alloc_init_stage == 0) {
		stats_alloc_init();
	} else if (alloc_init_stage < 3) {
		buffer += size;
		return buffer - size;
  }

	void *addr = alloc_malloc(size);
	stats_tracking();

	return addr;
}


void free(void *ptr) {
	if (alloc_init_stage == 0) {
		stats_alloc_init();
	} else if (alloc_init_stage < 3 || (ptr >= buffer_start && ptr < buffer)) {
		return;
	}
	
	if (ptr) {
		alloc_free(ptr);
		stats_tracking();
	}
}

void *realloc(void *ptr, size_t size) {
	if (alloc_init_stage == 0) {
		stats_alloc_init();
	} else if (alloc_init_stage < 3) {
		void *newPtr = buffer;
		buffer += size;
		if (ptr && size) { memcpy(newPtr, ptr, size); }
		return newPtr;
  }

	void *addr = alloc_realloc(ptr, size);
	stats_tracking();

	return addr;
}
