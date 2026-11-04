#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned long long process_ticks(const char *path) { 
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char buffer[512];
    unsigned long long utime = 0, stime = 0;
    if (fgets(buffer, sizeof(buffer), f)) {
        char *after_paren = strrchr(buffer, ')');
        if (after_paren) {
            sscanf(after_paren + 1, " %*c %*d %*d %*d %*d %*d %*lu %*lu %*lu %*lu %*lu %llu %llu", &utime, &stime);
        }
    }

    return utime + stime;
}

typedef struct {
    int pid;
    unsigned long long ticks;
} ProcessEntry;

typedef struct {
    ProcessEntry *items;
    size_t count;
    size_t capacity;
} ProcessList;

void add_process_ticks(ProcessList *list) {
}

typedef struct {
    char name[256];
    int age;
    float grade;
} student;

int main() {
    Student a[3] = {
        {"daniel", 15, 0.0f},
        
    }

    return 0;
}
