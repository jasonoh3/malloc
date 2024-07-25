#pragma once

typedef struct _alloc_stats_t {
    unsigned long long max_heap_used;
    unsigned long memory_uses;
    unsigned long long memory_heap_sum;
} alloc_stats_t;
