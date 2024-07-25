#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mstats-utils.h"

mstats_result * read_mstats_result(const char *filename) {
	mstats_result *result = (mstats_result *) malloc(sizeof(mstats_result));
	FILE *f;
	f = fopen(filename,"r");
	fscanf(f,
		"%d\n%llu\n%f\n%f\n",
			&result->status,
			&result->max_heap_used,
			&result->avg_heap_used,
			&result->time_taken);
	fclose(f);
	return result;
}

