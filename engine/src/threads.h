// Minimal portable parallel-for (spawns short-lived threads; no pool state machine)
#ifndef TLN_THREADS_H
#define TLN_THREADS_H

// Runs fn(ctx, begin, end) over [0,count) split into `chunks` contiguous ranges.
// The calling thread executes the first range; others run in spawned threads.
void parallel_for(int count, void (*fn)(void *ctx, int begin, int end), void *ctx, int chunks);
int  sys_cpu_count(void);

#endif
