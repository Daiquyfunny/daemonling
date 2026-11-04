
void readSystemUsage() {
    SystemSample a;
    SystemSample b;

    readSystemCPU(&a);
    for (size_t i = 0; i < count; i++) {
        char pidStr[32];
        snprintf(pidStr, sizeof(pidStr), "%d/stat", process[i].pid);
        const char *filePath = filePathG("/proc/", pidStr);
        process[i].ticks = get_process_ticks(filePath);
    }
    
    usleep(1000000);
    
    readSystemCPU(&b);
    for (size_t i = 0; i < count; i++) {
        char pidStr[32];
        snprintf(pidStr, sizeof(pidStr), "%d/stat", process[i].pid);
        const char *filePath = filePathG("/proc/", pidStr);
        process[i].ticksNext = get_process_ticks(filePath);
    }
    

    unsigned long long delta_total = b.total - a.total;
    unsigned long long delta_idle = b.idle - a.idle;

    double overall_CPU_util = 0.0;
    if (delta_total > 0) {
        overall_CPU_util = (1.0 - ((double)delta_idle)/(delta_total))*100.0;
    }
    printf("overall CPU utilization: %.2f%%\n", overall_CPU_util);

    for (size_t i = 0; i < count; i+=) {
        snprintf(pidStr, sizeof(pidStr), "%d/stat", process[i].pid);
        const char *filePath = filePathG("/proc/", pidStr);
        double usage = 0.0;

        usage = ((double)(process[i].ticksNext - process[i].ticks) / overall_CPU_util) * 100.0;
        process[i].cpu_usage = usage;
    }
}

#include "system_usage.h"
#include <stdio.h>
#include <unistd.h>

// External functions assumed to be defined elsewhere in your codebase
extern void readSystemCPU(SystemSample *sample);
extern unsigned long long get_process_ticks(const char *filePath);
extern const char *filePathG(const char *base, const char *path);

// Helper: Fetches ticks for a single process
static unsigned long long get_process_ticks_for_pid(int pid) {
    char pidStr[32];
    snprintf(pidStr, sizeof(pidStr), "%d/stat", pid);
    const char *filePath = filePathG("/proc/", pidStr);
    return get_process_ticks(filePath);
}

// Helper: Samples initial state
static void sample_system_start(SystemSample *a, ProcessInfo *process, size_t count) {
    readSystemCPU(a);
    for (size_t i = 0; i < count; i++) {
        process[i].ticks = get_process_ticks_for_pid(process[i].pid);
    }
}

// Helper: Samples ending state
static void sample_system_end(SystemSample *b, ProcessInfo *process, size_t count) {
    readSystemCPU(b);
    for (size_t i = 0; i < count; i++) {
        process[i].ticksNext = get_process_ticks_for_pid(process[i].pid);
    }
}

// Helper: Computes overall CPU utilization percentage
static double compute_overall_cpu(const SystemSample *a, const SystemSample *b, unsigned long long *delta_total_out) {
    unsigned long long delta_total = b->total - a->total;
    unsigned long long delta_idle = b->idle - a->idle;
    *delta_total_out = delta_total;

    if (delta_total > 0) {
        return (1.0 - ((double)delta_idle) / delta_total) * 100.0;
    }
    return 0.0;
}

// Helper: Computes individual process CPU percentages
static void compute_process_usages(ProcessInfo *process, size_t count, double overall_cpu_util) {
    for (size_t i = 0; i < count; i++) {
        double usage = 0.0;
        if (overall_cpu_util > 0.0) {
            usage = ((double)(process[i].ticksNext - process[i].ticks) / overall_cpu_util) * 100.0;
        }
        process[i].cpu_usage = usage;
    }
}

// Main Orchestrator Function
void readSystemUsage(ProcessInfo *process, size_t count) {
    SystemSample a;
    SystemSample b;
    unsigned long long delta_total = 0;

    sample_system_start(&a, process, count);
    usleep(1000000);
    sample_system_end(&b, process, count);

    double overall_CPU_util = compute_overall_cpu(&a, &b, &delta_total);
    printf("overall CPU utilization: %.2f%%\n", overall_CPU_util);

    compute_process_usages(process, count, overall_CPU_util);
}
