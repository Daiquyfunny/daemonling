#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

typedef struct {
    pid_t pid;
    char name[256];
    long ram_usage;
    double cpu_usage; // delta
    unsigned long long ticks; // utime + stime
    unsigned long long ticksNext; // another sample over a period of time
} ProcessSample;

typedef struct {
    unsigned long long total;
    unsigned long long idle;
} SystemSample;

#define MAX_VALUE 32768

const char *filePathG(const char *path, const char *file_target) {
    size_t size = 256;

    char *filePath = malloc(size);
    snprintf(filePath, size, "%s%s", path, file_target);

    return filePath;
}

static unsigned long long get_process_ticks(const char *filePath) {
    FILE *f = fopen(filePath, "r");
    if (!f) return 0;
    
    char buffer[512];
    unsigned long long utime = 0, stime = 0;
    if (fgets(buffer, sizeof(buffer), f)) {
        char *after_paren = strrchr(buffer, ')');
        if (after_paren) {
            sscanf(after_paren + 1, " %*c %*d %*d %*d %*d %*d %*lu %*lu %*lu %*lu %*lu %llu %llu", &utime, &stime);
        }
    }

    fclose(f);
    return utime + stime;
}

void look_at_pid(pid_t r, ProcessSample *k) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/", r);

    DIR *pid_f = opendir(path);
    struct dirent *entry;

    int f_comm = 0; // 0: not found 1: found
    int f_status = 0;

    while ((entry = readdir(pid_f)) != NULL) {
        if (f_comm != 1 && strcmp(entry->d_name, "comm") == 0) {
            const char *filePath = filePathG(path, entry->d_name);
            FILE *f = fopen(filePath, "r");

            if (f) {
                fgets(k->name, 256, f);
                fclose(f);
            }

            free((void*)filePath);
            f_comm = 1;
        }
        
        // FETCH RAM USAGE
        if (f_status != 1 && strcmp(entry->d_name, "status") == 0) {
            const char *filePath = filePathG(path, entry->d_name);
            FILE *f = fopen(filePath, "r");

            if (f) {
                char buffer[256];

                while (fgets(buffer, sizeof(buffer), f) != NULL) {
                    if (strncmp(buffer, "VmRSS:", 6) == 0) {
                        sscanf(buffer, "VmRSS: %ld", &k->ram_usage);
                        break;
                    }
                }
                fclose(f);
            }
            free((void*)filePath);
            f_status = 1;

        }
        
        if (strcmp(entry->d_name, "stat") == 0) {
            // const char *filePath = filePathG(path, entry->d_name);
            
        }
    }
    
    closedir(pid_f);
}

void readSystemCPU(SystemSample *s) {
    unsigned long long user, nice, sys, idle, iowait, irq, sirq, steal;

    FILE *f = fopen("/proc/stat", "r");
    fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
            &user, &nice, &sys, &idle, &iowait, &irq, &sirq, &steal);
    printf("cpu %llu %llu %llu %llu %llu %llu %llu %llu\n",
            &user, &nice, &sys, &idle, &iowait, &irq, &sirq, &steal);

    s->total = user+nice+sys+idle+iowait+irq+sirq+steal;
    s->idle = idle+iowait;
}

// gptfp: get_process_ticks_for_pid
unsigned long long gptfp(ProcessSample *process) {
    char pidStr[32];
    snprintf(pidStr, sizeof(pidStr), "%d/stat", process->pid);
    const char *filePath = filePathG("/proc/", pidStr);
    return get_process_ticks(filePath);
}

void sampleSystemStart(SystemSample *a, ProcessSample *process, size_t count) {
    readSystemCPU(a);
    for (size_t i = 0; i < count; i++) {
        process[i].ticks = gptfp(&process[i]);
    }
}

void sampleSystemEnd(SystemSample *b, ProcessSample *process, size_t count) {
    readSystemCPU(b);
    for (size_t i = 0; i < count; i++) {
        process[i].ticksNext = gptfp(&process[i]);
    }
}

double compute_overall_cpu(const SystemSample *a, const SystemSample *b, unsigned long long *delta_total_out) {
    unsigned long long delta_total = b->total - a->total;
    unsigned long long delta_idle = b->idle - a->idle;
    *delta_total_out = delta_total;

    if (delta_total > 0) {
        return (1.0 - ((double)delta_idle) / delta_total) * 100.0;
    }
    return 0.0;
}

void compute_process_usages(ProcessSample *process, size_t count, double overall_cpu_util) {
    for (size_t i = 0; i < count; i++) {
        double usage = 0.0;
        if (overall_cpu_util > 0.0) {
            usage = ((double)(process[i].ticksNext - process[i].ticks) / overall_cpu_util) * 100.0;
        }
        process[i].cpu_usage = usage;
    }
}

// loP: list of process
void readSystemUsage(ProcessSample *loP, size_t count) {
    SystemSample a;
    SystemSample b;

    sampleSystemStart(&a, loP, count);
    usleep(1000000);
    sampleSystemEnd(&b, loP, count);

    unsigned long long delta_total = 0;

    double overall_CPU_util = compute_overall_cpu(&a, &b, &delta_total);
    printf("overall CPU utilization: %.2f%%\n", overall_CPU_util);

    compute_process_usages(loP, count, overall_CPU_util);
}

void displayUsage() {
}


volatile sig_atomic_t running = 1;
void handle_sigint(int sig) {
    running = 0;
}

int main() {

    DIR *dir = opendir("/proc/");
    FILE *log = fopen("./log.txt", "w");
    
    if (dir == NULL) {
        perror("opendir");
        return 1;
    }
    
    unsigned long total_files = 0;
    size_t count = 0;
    size_t pid_amount = 256; // capacity
    ProcessSample *process = malloc( pid_amount * sizeof(ProcessSample) );
    if (process == NULL) {
        fprintf(stderr, "failed to malloc process\n");
        free(process);
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *filePath = filePathG("/proc/", entry->d_name);

        // baseline: nothing, or . or ..
        if (*entry->d_name == '\0') continue;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (entry->d_type != DT_DIR) {
            if (entry->d_type == DT_UNKNOWN) {
                struct stat fileInfo;

                if (stat(filePath, &fileInfo) != 0 && S_ISDIR(fileInfo.st_mode) == 0) {
                    continue; // file not found
                }
            }
        }

        size_t i = 0;
        size_t length = strlen(entry->d_name);
        while (entry->d_name[i] != '\0') {
            if (!isdigit((unsigned char)entry->d_name[i])) {
                break;
            }
            i = i + 1;
        }

        if (i > 0 && entry->d_name[i] == '\0') {
            pid_t pid = atoi(entry->d_name);

            if (count >= pid_amount) {
                printf("count %zu\npid_amount %zu\n", count, pid_amount);
                pid_amount = pid_amount * 2;
                ProcessSample *temp = realloc(process, pid_amount * sizeof(ProcessSample));

                if (temp == NULL) {
                    fprintf(stderr, "realloc failed on pid_reamount\n");
                    free(temp);
                    return 1;
                }

                process = temp;
            }
            count++;
        }

        total_files += 1;
    }
    printf("total_files %lu\n", total_files);
    signal(SIGINT, handle_sigint);
    while (running) {
        readSystemUsage(process, count);
    }
    printf("done reading\n");

    fclose(log);
    closedir(dir);

    free(process);

    return 0;
}
