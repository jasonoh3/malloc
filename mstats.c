#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h> 
#include <sys/types.h> 
#include <sys/mman.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "lib/mstats-alloc.h"

int child_still_running = 1;

void *timeout_timer(void *ptr) {
	int child_pid = *((int *)ptr);
	
	sleep(30);
	if (child_still_running == 1) {
		printf("Sending a SIGTERM to kill the child process (pid=%d) for running for over 30sec.\n", child_pid);
		if (kill(child_pid, SIGTERM) != 0 && errno != ESRCH) {
      perror("kill()");
    }
	}
	
	return NULL;
}


int main(int argc, char **argv, char **envp) {
	/*
	 * Check to ensure that the program is launched with at least one command
	 * line option.  Display helpful text if no options are present.
	 */
	if (argc == 1) {
		printf("You must supply a program to be invoked to use your replacement malloc() script.\n");
		printf("...you may use any program, even system programs, such as `ls`.\n");
		printf("\n");
		printf("Example: %s ls\n", argv[0]);
		return 1;
	}

	int evaluate = 0;
	if (argc == 3) {
		if(strcmp(argv[2],"evaluate") == 0)
			evaluate = 1;
	}
	/*
	 * Set up a shared memory file for later use by mmap(). 
	 */
	char file_name[] = "/tmp/cs240-XXXXXX";
	int fd = mkstemp(file_name);
	
	char *buffer = calloc(1, sizeof(alloc_stats_t));
	write(fd, buffer, sizeof(alloc_stats_t));
	close(fd);
	free(buffer);


  /*
   * Copy over existing ENV variables:
   */
  // Find length of existing `ENV`:
  unsigned int env2_len = 0;
  for (char **env = envp; *env != 0; env++) {
    env2_len++;
  }

	// Library sideload
	#ifdef __APPLE__
	const char *library_sideload_env = "DYLD_INSERT_LIBRARIES=";
	#else
	const char *library_sideload_env = "LD_PRELOAD=";
	#endif


	#ifdef USE_LIBC_ALLOC
	const char *library_sideload_lib = "lib/mstats-libc-alloc.so";
	#else
	const char *library_sideload_lib = "lib/mstats-alloc.so";
	#endif


  // Create space for existing `ENV` plus two more (and a NULL):
  env2_len += 4;
  char **env2 = malloc(env2_len * sizeof(char *));
  char *env2_LD_PRELOAD = NULL;
  unsigned int env2_ct = 0;
  for (char **env = envp; *env != 0; env++) {
    // Check for existing LD_PRELOAD:
    if (strncmp(*env, library_sideload_env, strlen(library_sideload_env)) == 0) {
      // Inject at beginning of LD_PRELOAD so it's loaded last:
      asprintf(&env2_LD_PRELOAD, "%s%s:%s", library_sideload_env, library_sideload_lib, (*env + strlen(library_sideload_env)));
      env2[env2_ct++] = env2_LD_PRELOAD;
    } else {
      env2[env2_ct++] = *env;
    }
  }

  // Add LD_PRELOAD if it does not exist:
  if (!env2_LD_PRELOAD) {
    asprintf(&env2_LD_PRELOAD, "%s%s", library_sideload_env, library_sideload_lib);
    env2[env2_ct++] = env2_LD_PRELOAD;
  }

  // Add ALLOC_STATS_MMAP for stats tracing:
  asprintf(&env2[env2_ct++], "ALLOC_STATS_MMAP=%s", file_name);

	// Add DYLD_FORCE_FLAT_NAMESPACE for Mac OSX:
	#ifdef __APPLE__
  asprintf(&env2[env2_ct++], "DYLD_FORCE_FLAT_NAMESPACE=1");
	#endif

  // Add NULL termination:
  env2[env2_ct++] = NULL;

	
	/*
	 * Replace the current running process with the process specified by the command
	 * line options.  If exec() fails, we won't even try and recover as there's likely
	 * nothing we could really do; however, we do our best to provide useful output
	 * with a call to perror().
	 */
  #ifdef STATS_MODE
	int forkid = fork();
  #else
  int forkid = 0;
  #endif

	if (forkid == 0 /* child */) {
		#ifdef __APPLE__
		execve(argv[1], argv + 1, env2); 
		#else
		execvpe(argv[1], argv + 1, env2); 
		#endif
    /* Note that exec() will not return on success. */
		perror("exec() failed");
		return 3;
	}
	free(env2[env2_ct - 2]); // ALLOC_STATS_MMAP
	free(env2_LD_PRELOAD);   // custom LD_PRELOAD
	free(env2);
	
	pthread_t tid;
	pthread_create(&tid, NULL, timeout_timer, &forkid);
	child_still_running = 1;
	
	int result;
	struct rusage resources_used;
	if (wait4(forkid, &result, 0, &resources_used) == -1) {
		perror("wait4()");
		return 4;
	}
	child_still_running = 0;
	pthread_detach(tid);
	
	FILE *file = fopen(file_name, "r");
	alloc_stats_t *stats = mmap(NULL, sizeof(alloc_stats_t), PROT_READ, MAP_SHARED, fileno(file), 0);
	if (!stats) {
		perror("mmap");
		return 5;
	}
	
	fclose(file);
	
	int total_sec = resources_used.ru_utime.tv_sec + resources_used.ru_stime.tv_sec;
	int total_usec = resources_used.ru_utime.tv_usec + resources_used.ru_stime.tv_usec;
	if (total_usec >= 1000 * 1000) {
		total_usec -= 1000 * 1000;
		total_sec++;
	}
	double total_time = total_sec + ((double)total_usec / ((double)1000 * 1000));
	
	// Save mstats result to a file.
	if(evaluate) {
		FILE *result_file = fopen("mstats_result.txt","w+");
		if(result_file) {
			if(result == 0)
				fputs("1\n", result_file);
			else
				fputs("0\n",result_file);

			// Save maximum memory used.
			char max_heap_used[16];
			sprintf(max_heap_used,"%llu\n",stats->max_heap_used);
			fputs(max_heap_used, result_file);

			// Save average memory used.
			if(stats->memory_uses == 0)
				fputs("0\n", result_file);
			else {
				char avg_heap_used[32];
				sprintf(avg_heap_used,"%.6f\n",stats->memory_heap_sum/(double)stats->memory_uses);
				fputs(avg_heap_used, result_file);
			}
			
			// Save time taken to execute
			char total_time_used[32];
			sprintf(total_time_used,"%.6f\n",total_time);
			fputs(total_time_used, result_file);
			
			fclose(result_file);
		}
		else {
			printf("[mstats]: ERROR WRITING EVALUATION RESULTS\n");
 		}
	}

	if (result == 0) { printf("[mstats]: STATUS: OK\n"); }
	else             { printf("[mstats]: STATUS: FAILED=(%d)\n", result); }
	printf("[mstats]: MAX: %llu\n", stats->max_heap_used);
	
	if (stats->memory_uses == 0) { printf("[mstats]: AVG: %f\n", 0.0f); }
	else                         { printf("[mstats]: AVG: %f\n", (stats->memory_heap_sum / (double)stats->memory_uses)); }
	
	printf("[mstats]: TIME: %f\n", total_time);

	munmap(stats, sizeof(alloc_stats_t));
	unlink(file_name);
	return 0;
}
