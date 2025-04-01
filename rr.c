#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define Max 4

struct Process {
    int arrivalTime;
    int burstTimeCPU;
    int burstTimeIO;
    int burstTimeRate;
    int waitingTime;
    int completionTime;
    int quantumtime;
    bool completed;
    int turnaroundTime;  
    int remainingBurst;  
};

void initProcess(struct Process * p, int arrivalTime, int burstTimeCPU, int burstTimeIO, int burstTimeRate) {
    p->arrivalTime = arrivalTime;
    p->burstTimeCPU = burstTimeCPU;
    p->burstTimeIO = burstTimeIO;
    p->burstTimeRate = burstTimeRate;
    p->waitingTime = 0;
    p->completionTime = 0;
    p->quantumtime = 0;
    p->completed = false;
    p->turnaroundTime = 0;
    p->remainingBurst = burstTimeCPU;  
}

void ino(struct Process * p) {
    p->quantumtime = 0;
    p->burstTimeRate = p->burstTimeIO;
}

void round_robin(struct Process * p) {  
    int time = 0;
    int quantum = 5;
    int queue[100];  
    int front = 0;
    int rear = 0;
    int completedProcess = 0;
    
    
    for(int i = 0; i < Max; i++) {
        if(p[i].arrivalTime == 0) {
            queue[rear] = i;
            rear++;
        }
    }
    
    while(completedProcess < Max) {
        if(front == rear) {
            
            time++;
            for(int i = 0; i < Max; i++) {
                if(p[i].arrivalTime == time && !p[i].completed) {
                    queue[rear] = i;
                    rear++;
                }
            }
            continue;
        }
        
        int i = queue[front];
        front = (front + 1) % 100;  
        
    
        int timeSlice = (quantum < p[i].remainingBurst) ? quantum : p[i].remainingBurst;
        
        
        time += timeSlice;
        p[i].remainingBurst -= timeSlice;
        p[i].quantumtime += timeSlice;  
        
        
        for(int j = 0; j < Max; j++) {
            if(!p[j].completed && p[j].arrivalTime > time - timeSlice && p[j].arrivalTime <= time) {
                queue[rear] = j;
                rear = (rear + 1) % 100;  
            }
        }
        
        
        if(p[i].remainingBurst == 0) {
            p[i].completionTime = time;
            p[i].completed = true;
            p[i].turnaroundTime = p[i].completionTime - p[i].arrivalTime;
            p[i].waitingTime = p[i].turnaroundTime - p[i].burstTimeCPU;
            completedProcess++;
        } else {
            
            if(p[i].burstTimeRate <= timeSlice) {
                ino(&p[i]);  
           
            }
        
            queue[rear] = i;
            rear = (rear + 1) % 100;  
        }
    }
}

void print(struct Process * p) {
    float avgWaitingTime = 0;
    float avgTurnaroundTime = 0;
    
    printf("Process\tArrival\tBurst\tCompletion\tWaiting\tTurnaround\n");
    for(int i = 0; i < Max; i++) {
        printf("%d\t%d\t%d\t%d\t\t%d\t%d\n", 
               i, p[i].arrivalTime, p[i].burstTimeCPU, 
               p[i].completionTime, p[i].waitingTime, p[i].turnaroundTime);
        
        avgWaitingTime += p[i].waitingTime;
        avgTurnaroundTime += p[i].turnaroundTime;
    }
    
    avgWaitingTime /= Max;
    avgTurnaroundTime /= Max;
    
    printf("\nAverage Waiting Time: %.2f\n", avgWaitingTime);
    printf("Average Turnaround Time: %.2f\n", avgTurnaroundTime);
}

int main() {
    struct Process p[Max];
    initProcess(&p[0],0, 24, 2, 5);
    initProcess(&p[1],3, 17, 3, 6);
    initProcess(&p[2],8, 50, 2, 5);
    initProcess(&p[3],15, 10, 3, 6);
    
    round_robin(p);  
    print(p);
    return 0;
}