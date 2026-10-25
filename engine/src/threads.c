#include "threads.h"
#include "core.h"

#if defined(_WIN32)
#include <windows.h>
typedef struct { void (*fn)(void *, int, int); void *ctx; int a, b; } Job;
static DWORD WINAPI job_thread(LPVOID p) {
    Job *j = (Job *)p;
    j->fn(j->ctx, j->a, j->b);
    return 0;
}
void parallel_for(int count, void (*fn)(void *ctx, int begin, int end), void *ctx, int chunks) {
    if (chunks < 1) chunks = 1;
    if (chunks > count) chunks = count;
    if (chunks <= 1) { fn(ctx, 0, count); return; }
    Job jobs[16];
    HANDLE handles[16];
    int nspawn = 0;
    for (int c = 0; c < chunks; c++) {
        int a = (int)((long long)count * c / chunks);
        int b = (int)((long long)count * (c + 1) / chunks);
        if (c == 0) continue;
        jobs[nspawn].fn = fn; jobs[nspawn].ctx = ctx; jobs[nspawn].a = a; jobs[nspawn].b = b;
        handles[nspawn] = CreateThread(NULL, 0, job_thread, &jobs[nspawn], 0, NULL);
        nspawn++;
    }
    fn(ctx, 0, (int)((long long)count / chunks));
    for (int i = 0; i < nspawn; i++) {
        if (handles[i]) { WaitForSingleObject(handles[i], INFINITE); CloseHandle(handles[i]); }
    }
}
int sys_cpu_count(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
}
#else
#include <pthread.h>
#include <unistd.h>
typedef struct { void (*fn)(void *, int, int); void *ctx; int a, b; } Job;
static void *job_thread(void *p) {
    Job *j = (Job *)p;
    j->fn(j->ctx, j->a, j->b);
    return NULL;
}
void parallel_for(int count, void (*fn)(void *ctx, int begin, int end), void *ctx, int chunks) {
    if (chunks < 1) chunks = 1;
    if (chunks > count) chunks = count;
    if (chunks <= 1) { fn(ctx, 0, count); return; }
    Job jobs[16];
    pthread_t th[16];
    int nspawn = 0;
    for (int c = 0; c < chunks; c++) {
        int a = (int)((long long)count * c / chunks);
        int b = (int)((long long)count * (c + 1) / chunks);
        if (c == 0) continue;
        jobs[nspawn].fn = fn; jobs[nspawn].ctx = ctx; jobs[nspawn].a = a; jobs[nspawn].b = b;
        if (pthread_create(&th[nspawn], NULL, job_thread, &jobs[nspawn]) == 0) nspawn++;
    }
    fn(ctx, 0, (int)((long long)count / chunks));
    for (int i = 0; i < nspawn; i++) pthread_join(th[i], NULL);
}
int sys_cpu_count(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}
#endif
