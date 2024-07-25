#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct _mstats_result {
	int status;
	unsigned long long int max_heap_used;
	float avg_heap_used;
	float time_taken;
};
typedef struct _mstats_result mstats_result;

mstats_result * read_mstats_result(char const *filename);
#ifdef __cplusplus
}
#endif


