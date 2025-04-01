#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define QUANTUM 5
#define MAX_PROCESSES 100

struct Process {
    char name[5];  
    int arrivalTime, burstTime, remainingTime;
    int ioInterval, ioDuration;
    int waitingTime, turnaroundTime, completionTime, responseTime;
    int executed;
    int quantum;
};

struct Process processes[MAX_PROCESSES];
int processCount = 0;

void readData(char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(1);
    }

    while (fscanf(file, "%[^;];%d;%d;%d;%d\n", 
                  processes[processCount].name,
                  &processes[processCount].arrivalTime,
                  &processes[processCount].burstTime,
                  &processes[processCount].ioInterval,
                  &processes[processCount].ioDuration) != EOF) {
        processes[processCount].remainingTime = processes[processCount].burstTime;
        processes[processCount].waitingTime = 0;
        processes[processCount].turnaroundTime = 0;
        processes[processCount].completionTime = 0;
        processes[processCount].responseTime = -1; // -1 means not yet responded
        processes[processCount].executed = 0;
        processCount++;
    }

    fclose(file);
}

void printProcesses() {
    printf("\nProcess Execution Results:\n");
    printf("------------------------------------------------------------\n");
    printf("PID  Arrival  Burst  Completion  Turnaround  Waiting  Response\n");
    for (int i = 0; i < processCount; i++) {
        printf("%-4s %-8d %-6d %-11d %-11d %-7d %-7d\n", 
               processes[i].name, 
               processes[i].arrivalTime, 
               processes[i].burstTime, 
               processes[i].completionTime, 
               processes[i].turnaroundTime, 
               processes[i].waitingTime, 
               processes[i].responseTime);
    }
    printf("------------------------------------------------------------\n");
}

void sjf() {
    printf("\nExecuting SJF (Non-Preemptive)...\n");

    int completed = 0, time = 0;
    while (completed < processCount) {
        int minIdx = -1;
        for (int i = 0; i < processCount; i++) {
            if (processes[i].arrivalTime <= time && !processes[i].executed) {
                if (minIdx == -1 || processes[i].burstTime < processes[minIdx].burstTime)
                    minIdx = i;
            }
        }

        if (minIdx == -1) {
            time++;
            continue;
        }

        if (processes[minIdx].responseTime == -1)
            processes[minIdx].responseTime = time - processes[minIdx].arrivalTime;

        time += processes[minIdx].burstTime;
        processes[minIdx].completionTime = time;
        processes[minIdx].turnaroundTime = processes[minIdx].completionTime - processes[minIdx].arrivalTime;
        processes[minIdx].waitingTime = processes[minIdx].turnaroundTime - processes[minIdx].burstTime;
        processes[minIdx].executed = 1;
        completed++;
    }
    printProcesses();
}

void srtf() {
    printf("\nExecuting SRTF (Preemptive)...\n");

    int completed = 0, time = 0, lastProcess = -1;
    while (completed < processCount) {
        int minIdx = -1;
        for (int i = 0; i < processCount; i++) {
            if (processes[i].arrivalTime <= time && processes[i].remainingTime > 0) {
                if (minIdx == -1 || processes[i].remainingTime < processes[minIdx].remainingTime)
                    minIdx = i;
            }
        }

        if (minIdx == -1) {
            time++;
            continue;
        }

        if (processes[minIdx].responseTime == -1)
            processes[minIdx].responseTime = time - processes[minIdx].arrivalTime;

        processes[minIdx].remainingTime--;
        time++;

        if (processes[minIdx].remainingTime == 0) {
            processes[minIdx].completionTime = time;
            processes[minIdx].turnaroundTime = processes[minIdx].completionTime - processes[minIdx].arrivalTime;
            processes[minIdx].waitingTime = processes[minIdx].turnaroundTime - processes[minIdx].burstTime;
            completed++;
        }
    }
    printProcesses();
}

void roundRobin() {
    printf("\nExecuting Round Robin (Quantum = %d)...\n", QUANTUM);

    int time = 0, completed = 0;
    int queue[MAX_PROCESSES], front = 0, rear = 0;

    for (int i = 0; i < processCount; i++) {
        if (processes[i].arrivalTime == 0) {
            queue[rear++] = i;
        }
    }

    while (completed < processCount) {
        if (front == rear) {
            time++;
            continue;
        }

        int index = queue[front++];
        if (processes[index].responseTime == -1)
            processes[index].responseTime = time - processes[index].arrivalTime;

        int execTime = (processes[index].remainingTime > QUANTUM) ? QUANTUM : processes[index].remainingTime;
        processes[index].remainingTime -= execTime;
        time += execTime;

        for (int i = 0; i < processCount; i++) {
            if (i != index && processes[i].arrivalTime <= time && processes[i].remainingTime > 0)
                queue[rear++] = i;
        }

        if (processes[index].remainingTime == 0) {
            processes[index].completionTime = time;
            processes[index].turnaroundTime = time - processes[index].arrivalTime;
            processes[index].waitingTime = processes[index].turnaroundTime - processes[index].burstTime;
            completed++;
        } else {
            queue[rear++] = index;
        }
    }
    printProcesses();
}
void ino(struct Process* p1, int auxQueue[], int* auxRear) {
    if (p1->ioDuration > 0) {
        p1->ioDuration--;  // Decrease I/O time
        if (p1->ioDuration == 0) {
            auxQueue[(*auxRear)++] = p1 - processes;  // Move to auxiliary queue
        }
    }
}

void execution(struct Process* p1, int* time, int queue[], int* rear, int auxQueue[], int* auxRear) {
    p1->quantum = QUANTUM;  
    while (p1->quantum > 0 && p1->remainingTime > 0) {
        p1->remainingTime -= 1;
        p1->quantum -= 1;
        (*time) += 1;

        // If process requires I/O
        if (p1->ioInterval > 0 && (p1->burstTime - p1->remainingTime) % p1->ioInterval == 0) {
            ino(p1, auxQueue, auxRear);  // Move to auxiliary queue
            return;
        }
    }

    // If process still has remaining time, re-add to queue
    if (p1->remainingTime > 0) {
        queue[(*rear)++] = p1 - processes;
    } else {
        p1->executed = 1;
        p1->completionTime = *time;
    }
}

void virtualRoundRobin() {
    printf("\nExecuting Virtual Round Robin (Quantum = %d)...\n", QUANTUM);

    int completed = 0, vrrtime = 0;
    int queue[MAX_PROCESSES], front = 0, rear = 0;
    int auxQueue[MAX_PROCESSES], auxFront = 0, auxRear = 0;  // Auxiliary Queue

    // Enqueue initially arriving processes
    for (int i = 0; i < processCount; i++) {
        if (processes[i].arrivalTime <= vrrtime) {
            queue[rear++] = i;
        }
    }

    while (completed < processCount) {
        // Process Auxiliary Queue first (higher priority)
        if (auxFront < auxRear) {
            int index = auxQueue[auxFront++];
            struct Process* p = &processes[index];
            execution(p, &vrrtime, queue, &rear, auxQueue, &auxRear);
            continue;  // Give priority to auxQueue processes
        }

        if (front == rear) {  
            vrrtime++;  // Move time forward if no processes are ready
            continue;
        }

        int index = queue[front++];
        struct Process* p = &processes[index];

        if (p->responseTime == -1) 
            p->responseTime = vrrtime - p->arrivalTime;

        execution(p, &vrrtime, queue, &rear, auxQueue, &auxRear);

        if (p->remainingTime == 0) {
            p->turnaroundTime = vrrtime - p->arrivalTime;
            p->waitingTime = p->turnaroundTime - p->burstTime;
            completed++;
        }

        // Add newly arriving processes
        for (int i = 0; i < processCount; i++) {
            if (!processes[i].executed && processes[i].arrivalTime <= vrrtime && processes[i].remainingTime > 0) {
                bool alreadyQueued = false;
                for (int j = front; j < rear; j++) {
                    if (queue[j] == i) {
                        alreadyQueued = true;
                        break;
                    }
                }
                if (!alreadyQueued) {
                    queue[rear++] = i;
                }
            }
        }
    }

    printProcesses();
}


int main(int argc , char *argv[]) {
    readData(argv[1]);

    sjf();
    srtf();
    // roundRobin();
    virtualRoundRobin();

    return 0;
}
