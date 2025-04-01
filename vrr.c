#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>
#include <math.h>

#define LOG_TICK(ticks) printf("%zu", ticks)
#define LOG(tick, device, procData) printf("%zu\t%s\t\t%s\n", tick, device, procData)
#define LOG_DEBUG(name, label, info) printf("%s\n\t\t%s\t%zu\n", name, label, info)
#define MAX_PROCS 100
#define MAX_NAME_LEN 20
#define MAX_QUEUE_SIZE 100
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} State;

typedef struct {
    char procName[MAX_NAME_LEN];
    size_t arrivalTime;
    size_t burstTimeCPU;
    size_t burstTimeIO;
    size_t burstTimeRate;  // IO burst after every n CPU bursts
    size_t startTime;
    size_t completionTime;
    size_t burstRemainCPU;
    size_t lastIOBurst;
    int saveContextOfq;
    State state;
} Process;

// Queue implementation for Process
typedef struct {
    Process data[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
} ProcessQueue;

void initQueue(ProcessQueue* q) {
    q->front = 0;
    q->rear = -1;
    q->size = 0;
}

int isEmpty(ProcessQueue* q) {
    return q->size == 0;
}

void enqueue(ProcessQueue* q, Process p) {
    if (q->size >= MAX_QUEUE_SIZE) {
        printf("Queue overflow\n");
        exit(1);
    }
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->data[q->rear] = p;
    q->size++;
}

Process dequeue(ProcessQueue* q) {
    if (isEmpty(q)) {
        printf("Queue underflow\n");
        exit(1);
    }
    Process p = q->data[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    return p;
}

Process front(ProcessQueue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty\n");
        exit(1);
    }
    return q->data[q->front];
}

// Process functions
void initProcess(Process* proc, const char* name, size_t at, size_t btCPU, size_t btIO, size_t btr) {
    strncpy(proc->procName, name, MAX_NAME_LEN-1);
    proc->procName[MAX_NAME_LEN-1] = '\0';
    proc->arrivalTime = at;
    proc->burstTimeCPU = btCPU;
    proc->burstRemainCPU = btCPU;
    proc->burstTimeIO = btIO;
    proc->burstTimeRate = btr;
    proc->startTime = 0;
    proc->lastIOBurst = 0;
    proc->saveContextOfq = 0;
}

State execProcess(Process* proc) {
    proc->state = RUNNING;
    if (--proc->burstRemainCPU <= 0) {
        proc->state = TERMINATED;
    } else if (++proc->lastIOBurst >= proc->burstTimeRate) {
        refreshIOBurst(proc);
        proc->state = BLOCKED;
    }
    return proc->state;
}

void refreshIOBurst(Process* proc) {
    proc->lastIOBurst = 0;
}

size_t turnAroundTime(Process* proc) {
    return proc->completionTime - proc->arrivalTime;
}

size_t waitingTime(Process* proc) {
    return turnAroundTime(proc) - proc->burstTimeCPU;
}

size_t responseTime(Process* proc) {
    return proc->startTime - proc->arrivalTime;
}

// Device structure
typedef struct {
    Process procs[MAX_PROCS];
    Process completedProcs[MAX_PROCS];
    size_t numProcs;
    size_t numCompletedProcs;
    size_t totalProc;
    size_t ticksCPU;
    size_t timeQuantum;
    int isCPUIdle;

    size_t countIOBurst;
    int isIOIdle;
    Process execProcIO;

    ProcessQueue readyQ;
    ProcessQueue auxQ;
    ProcessQueue ioQ;
} Device;

void initDevice(Device* d, Process procs[], size_t numProcs) {
    memcpy(d->procs, procs, numProcs * sizeof(Process));
    d->numProcs = numProcs;
    d->totalProc = numProcs;
    d->ticksCPU = 0;
    d->timeQuantum = 5;
    d->isCPUIdle = 1;
    d->countIOBurst = 0;
    d->isIOIdle = 1;
    d->numCompletedProcs = 0;
    
    // Initialize queues
    initQueue(&d->readyQ);
    initQueue(&d->auxQ);
    initQueue(&d->ioQ);
}

void checkFreshArrivals(Device* d) {
    int i = 0;
    char buffer[100];
    while (i < d->numProcs) {
        if (d->procs[i].arrivalTime == d->ticksCPU) {
            snprintf(buffer, sizeof(buffer), "%s[Arrive]", d->procs[i].procName);
            LOG(d->ticksCPU, "CPU", buffer);
            d->procs[i].state = READY;
            enqueue(&d->readyQ, d->procs[i]);
            
            // Remove the process from the array by shifting remaining elements
            for (int j = i; j < d->numProcs - 1; j++) {
                d->procs[j] = d->procs[j + 1];
            }
            d->numProcs--;
            continue;
        }
        i++;
    }
}

void ioDevice(Device* d) {
    char buffer[100];
    if (!d->isIOIdle) {
        if (++d->countIOBurst >= d->execProcIO.burstTimeIO) {
            snprintf(buffer, sizeof(buffer), "%s[Comp]:%zu", d->execProcIO.procName, d->countIOBurst);
            LOG(d->ticksCPU, "IO", buffer);
            enqueue(&d->auxQ, d->execProcIO);
            memset(&d->execProcIO, 0, sizeof(Process));
            d->isIOIdle = 1;
        } else {
            snprintf(buffer, sizeof(buffer), "%s:%zu", d->execProcIO.procName, d->countIOBurst);
            LOG(d->ticksCPU, "IO", buffer);
        }
    }

    if (d->isIOIdle && !isEmpty(&d->ioQ)) {
        d->execProcIO = dequeue(&d->ioQ);
        d->countIOBurst = 0;
        d->isIOIdle = 0;
        snprintf(buffer, sizeof(buffer), "%s[Sched]:%zu", d->execProcIO.procName, d->countIOBurst);
        LOG(d->ticksCPU, "IO", buffer);
    }
}

void processor(Device* d) {
    Process execProc;
    memset(&execProc, 0, sizeof(Process));
    int q = 0;
    char buffer[100];
    
    printf("Time (tick)\tDevice\t\tProcess Served\n");
    
    while (d->totalProc) {
        LOG_TICK(d->ticksCPU);
        if (d->isCPUIdle) {
            LOG(d->ticksCPU, "CPU", "-");
        }
        
        checkFreshArrivals(d);

        if (!d->isCPUIdle) {
            execProcess(&execProc);
            if (execProc.state == TERMINATED) {
                snprintf(buffer, sizeof(buffer), "%s[Comp]", execProc.procName);
                LOG(d->ticksCPU, "CPU", buffer);
                d->isCPUIdle = 1;
                d->totalProc--;
                execProc.completionTime = d->ticksCPU;
                d->completedProcs[d->numCompletedProcs++] = execProc;
                memset(&execProc, 0, sizeof(Process));
            } else if (execProc.state == BLOCKED) {
                snprintf(buffer, sizeof(buffer), "%s[Q IO]:%zu", execProc.procName, execProc.burstRemainCPU);
                LOG(d->ticksCPU, "CPU", buffer);
                execProc.saveContextOfq = (q + 1) % d->timeQuantum;
                enqueue(&d->ioQ, execProc);
                d->isCPUIdle = 1;
                memset(&execProc, 0, sizeof(Process));
            } else {
                snprintf(buffer, sizeof(buffer), "%s:%zu", execProc.procName, execProc.burstRemainCPU);
                LOG(d->ticksCPU, "CPU", buffer);
            }
        }

        int toSchedule = (!isEmpty(&d->readyQ) || !isEmpty(&d->auxQ)) && 
                         (d->isCPUIdle || q + 1 >= d->timeQuantum);
        
        if (toSchedule) {
            Process proc;
            if (!isEmpty(&d->auxQ)) {
                proc = dequeue(&d->auxQ);
                q = proc.saveContextOfq - 1;
            } else {
                proc = dequeue(&d->readyQ);
                q = -1;
            }
            if (!d->isCPUIdle) {
                enqueue(&d->readyQ, execProc);
            }
            snprintf(buffer, sizeof(buffer), "%s[Sched]#q=%d", proc.procName, q + 1);
            LOG(d->ticksCPU, "CPU", buffer);
            execProc = proc;
            execProc.startTime = MIN(execProc.startTime, d->ticksCPU);
            d->isCPUIdle = 0;
        }

        ioDevice(d);
        d->ticksCPU++;
        q++;
        printf("\n");
    }
}

double avgWaitingTime(Device* d) {
    double sum = 0;
    for (size_t i = 0; i < d->numCompletedProcs; i++) {
        sum += waitingTime(&d->completedProcs[i]);
    }
    return sum / d->numCompletedProcs;
}

void debugDevice(Device* d) {
    for (size_t i = 0; i < d->numCompletedProcs; i++) {
        Process* proc = &d->completedProcs[i];
        LOG_DEBUG(proc->procName, "Arrival Time:", proc->arrivalTime);
        LOG_DEBUG("", "Start Time:", proc->startTime);
        LOG_DEBUG("", "Response Time:", responseTime(proc));
        LOG_DEBUG("", "Completion Time:", proc->completionTime);
        LOG_DEBUG("", "Turnaround Time:", turnAroundTime(proc));
        LOG_DEBUG("", "Waiting Time:", waitingTime(proc));
        printf("\n");
    }
    printf("Avg Waiting Time: %f\n", avgWaitingTime(d));
}

int main() {
    Process procs[4];
    initProcess(&procs[0], "P0", 0, 24, 2, 5);
    initProcess(&procs[1], "P1", 3, 17, 3, 6);
    initProcess(&procs[2], "P2", 8, 50, 2, 5);
    initProcess(&procs[3], "P3", 15, 10, 3, 6);

    Device d;
    initDevice(&d, procs, 4);
    processor(&d);
    debugDevice(&d);

    return 0;
}