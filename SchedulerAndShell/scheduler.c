#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<time.h>
#include<sys/mman.h>    //mmap and munmap
#include<stdatomic.h>
#include<fcntl.h>       //shm_open and ftruncate
#include<signal.h>
#include<errno.h>
#include <semaphore.h>
#define PIPE_SEMAPHORE_NAME "/pipe_semaphore"

typedef struct timespec timespec;


/// QUEUE


typedef struct Job_PCB{
    pid_t pid;
    char* job_name;
    timespec wait_time;
    timespec start_time;
    timespec end_time;
    timespec prev_time;
    int priority;
    int completed;
    struct Job_PCB* next;
    //completion time = end_time - start_time
} Job_PCB;


typedef struct Queue{
    Job_PCB *front;
    Job_PCB *back;
    int size;
} Queue;

void queue(Queue *q) {
    q->front = NULL;
    q->back = NULL;
    q->size = 0;
}

int empty(Queue *q) {
    return q->front == NULL;
}

void enqueue(Queue* q, Job_PCB* p) {
    //clock_gettime(CLOCK_REALTIME, &p->eq_time);
    p->next = NULL;
    if (empty(q)) {
        q->front = p;
        q->back = p;
    } else {
        q->back->next = p;
        q->back = p;
    }
    q->size++;
}

Job_PCB* dequeue(Queue *q) {
    if (empty(q)) {
        printf("QUEUE IS EMPTY\n");
        return NULL;
    }
    Job_PCB* temp = q->front;
    q->front = q->front->next;
    if (q->front == NULL) {
        q->back = NULL;
    }
    temp->next = NULL;
    q->size--;
    return temp;
}

void display(Queue *q) {
    if (empty(q)) {
        printf("EMPTY\n");
        return;
    }
    Job_PCB* temp = q->front;
    printf("QUEUE --> ");
    while (temp != NULL) {
        printf(" %d ->", temp->pid);
        temp = temp->next;
    }
    printf("\n");
}

int size(Queue *q){
    return q->size;
}

int terminated_count=0;
int NCPU;
float TSLICE;

Queue ready_queue_1;
Queue ready_queue_2;
Queue ready_queue_3;
Queue ready_queue_4;

int ready_1_count=0;
int ready_2_count=0;
int ready_3_count=0;
int ready_4_count=0;

Queue running_queue;
int running_count=0;
Queue terminated_queue;

int pipefd[2];
volatile int sigint_received=0;
sem_t* pipe_semaphore;  

void context_switch(){
    //code for the running and ready queue thing.
    int ready_count = ready_1_count + ready_2_count + ready_3_count + ready_4_count;
    if (ready_count==0 && running_count==0){        //if no processes have arrived or running.
        return;
    }
    int status;
    int rc = running_count;
    for (int i = 0; i < rc; i++){
        Job_PCB* j = dequeue(&running_queue);
        running_count--;
        int result=waitpid(j->pid, &status, WNOHANG);
        if (result == 0){
            kill(j->pid, SIGSTOP);
            clock_gettime(CLOCK_MONOTONIC,&j->prev_time);
            switch(j->priority){
                case 1:
                    enqueue(&ready_queue_1, j);
                    ready_1_count++;
                    break;
                case 2:
                    enqueue(&ready_queue_2, j);
                    ready_2_count++;
                    break;
                case 3:
                    enqueue(&ready_queue_3, j);
                    ready_3_count++;
                    break;
                case 4:
                    enqueue(&ready_queue_4, j);
                    ready_4_count++;
                    break;
            }
        }
        else if (result == -1){
            if (j->completed != 1){
                j->completed = 1;
                switch(j->priority){
                    case 1:
                        enqueue(&ready_queue_1, j);
                        ready_1_count++;
                        break;
                    case 2:
                        enqueue(&ready_queue_2, j);
                        ready_2_count++;
                        break;
                    case 3:
                        enqueue(&ready_queue_3, j);
                        ready_3_count++;
                        break;
                    case 4:
                        enqueue(&ready_queue_4, j);
                        ready_4_count++;
                        break;
                }
            }
        }
        else{
            if (WIFEXITED(status) && WEXITSTATUS(status)==10) {
                    j->completed = 10;
                    enqueue(&terminated_queue, j);
                    terminated_count++;
                    printf("Execution failed for command : ");
                    printf(j->job_name);
                    printf("\n");
            }
            else{
                clock_gettime(CLOCK_MONOTONIC,&j->end_time);
                printf("Command: %s\n",j->job_name);
                printf("PID: %d\n",j->pid);
                printf("Wait time: %ld seconds, %ld nanoseconds\n", j->wait_time.tv_sec, j->wait_time.tv_nsec);
                timespec completion={0,0};
                completion.tv_sec+=(j->end_time.tv_sec-j->start_time.tv_sec);
                completion.tv_nsec+=(j->end_time.tv_nsec-j->start_time.tv_nsec);
                if (completion.tv_nsec<0){
                    completion.tv_sec--;
                    completion.tv_nsec+=1000000000;
                }
                printf("Completion time: %ld seconds, %ld nanoseconds\n", completion.tv_sec,completion.tv_nsec);
                printf("\n");
                enqueue(&terminated_queue,j);
                terminated_count++;
            }    
        }
    }

    int rdc = ready_1_count + ready_2_count + ready_3_count + ready_4_count;
    for (int i=0; (i < NCPU && i < rdc); i++){
        Job_PCB* j;
        if (ready_1_count > 0){
            j = dequeue(&ready_queue_1);
            ready_1_count--;
        }
        else if (ready_2_count > 0){
            j = dequeue(&ready_queue_2);
            ready_2_count--;
        }
        else if (ready_3_count > 0){
            j = dequeue(&ready_queue_3);
            ready_3_count--;
        }
        else if (ready_4_count > 0){
            j = dequeue(&ready_queue_4);
            ready_4_count--;
        }
        else{
            break;
        }

        if (j->completed==-1){
            pid_t pid=fork();
            if (pid<0){
                perror("fork in context switch failed");
                exit(1);
            }
            else if(pid==0){
                char* buffer=strdup(j->job_name);
                if (buffer==NULL){
                    perror("strdup in context switch failed");
                }
                char* args[]={buffer,NULL};
                execvp(args[0], args);
                perror("Execvp in context switch");
                exit(10);
            }
            else{
                j->completed=0;
                j->pid=pid;
                j->wait_time.tv_sec=0;
                j->wait_time.tv_nsec=0;
            }
        }
        timespec cur_time;
        clock_gettime(CLOCK_MONOTONIC, &cur_time);
        j->wait_time.tv_sec+=(cur_time.tv_sec-j->prev_time.tv_sec);
        j->wait_time.tv_nsec+=(cur_time.tv_nsec-j->prev_time.tv_nsec);
        if (j->wait_time.tv_nsec<0){
            j->wait_time.tv_sec--;
            j->wait_time.tv_nsec+=1000000000;
        }
        enqueue(&running_queue, j);
        running_count++;
        kill(j->pid, SIGCONT); 

    }
    // printf("Ready ");
    // display(&ready_queue);
    // printf("Running ");
    // display(&running_queue);
    // printf("Terminated ");
    // display(&terminated_queue);
}

void scheduler_signal_handler(int signum){
    if (signum==SIGINT){
        sigint_received=1;
    }
}

void add_to_ready(char* whole_command){
    char* command = strtok(whole_command, "\n");
    char* commands[500];
    int num_commands = 0;

    while (command != NULL) {
        commands[num_commands++] = command;
        command = strtok(NULL, "\n");
    }

    for (int j=0; j<num_commands; j++){
        char* arguments[500];
        char* token = strtok(commands[j], " \t");
        int i = 0;
        while (token != NULL) {
            arguments[i++] = token;
            token = strtok(NULL, " \t");
        }
        arguments[i] = NULL;
        Job_PCB* new_job = malloc(sizeof(Job_PCB));
        if (new_job == NULL) {
            perror("Failed to allocate memory for new Job_PCB");
            exit(1);
        }
        new_job->job_name=strdup(arguments[1]);
        if (i==2){
            new_job->priority = 1;
        }
        else{
            int p = atoi(arguments[2]);
            if (p <= 0 || p >= 5){
                printf("Invalid priority. Default priority = 1 taken");
                new_job->priority = 1;
            }
            else{
                new_job->priority = p;
            }
        }
        if (new_job->job_name==NULL){
            perror("strdup failed");
        }
        clock_gettime(CLOCK_MONOTONIC,&new_job->start_time);
        clock_gettime(CLOCK_MONOTONIC,&new_job->prev_time);
        new_job->completed=-1;
        switch(new_job->priority){
            case 1:
                enqueue(&ready_queue_1, new_job);
                ready_1_count++;
                break;
            case 2:
                enqueue(&ready_queue_2, new_job);
                ready_2_count++;
                break;
            case 3:
                enqueue(&ready_queue_3, new_job);
                ready_3_count++;
                break;
            case 4:
                enqueue(&ready_queue_4, new_job);
                ready_4_count++;
                break;
        }

    }
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);  // Get current flags
    if (flags == -1) {
        perror("fcntl failed");
        exit(1);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("failed to set non-blocking mode for pipe");
        exit(1);
    }
}

int main(int argc, char* argv[]){

    struct sigaction ssh;
    memset(&ssh, 0, sizeof(ssh));
    ssh.sa_handler = scheduler_signal_handler;
    sigaction(SIGINT, &ssh, NULL);
    sigemptyset(&ssh.sa_mask); 
    ssh.sa_flags = 0;

    NCPU = atoi(argv[1]);
    TSLICE = strtof(argv[2],NULL);
    pipefd[0]=atoi(argv[3]);
    pipefd[1]=atoi(argv[4]);
    set_nonblocking(pipefd[0]);
    pipe_semaphore = sem_open(PIPE_SEMAPHORE_NAME, 0);
    if (pipe_semaphore == SEM_FAILED) {
        perror("sem_open failed in child");
        exit(1);
    }

    queue(&running_queue);
    queue(&ready_queue_1);
    queue(&ready_queue_2);
    queue(&ready_queue_3);
    queue(&ready_queue_4);
    queue(&terminated_queue);

    while ((ready_1_count>0 + ready_2_count + ready_3_count + ready_4_count) || (running_count>0) || !sigint_received){
        char buffer[512];
        sem_wait(pipe_semaphore);
        ssize_t b_read = read(pipefd[0], buffer, sizeof(buffer));
        sem_post(pipe_semaphore);
        //printf("buffer: %s\n",buffer);
        if (b_read ==-1 && (errno == EAGAIN || errno==EWOULDBLOCK)){}
        else if (b_read==-1){
            perror("Error in read");
            exit(1);
        }
        else{
            buffer[sizeof(buffer) - 1] = '\0';
            add_to_ready(buffer);
        }
        context_switch();
        float sleep_time=TSLICE*1000;
        usleep(sleep_time);
    }
    for (int i=0; i<terminated_count; i++){
        
        Job_PCB* j=dequeue(&terminated_queue);
        if (j->completed == 10){
            printf("Execution failed for command : ");
            printf(j->job_name);
            printf("\n");
            free(j->job_name);
            continue;
        }
        printf("Command: %s\n",j->job_name);
        printf("PID: %d\n",j->pid);
        printf("Wait time: %ld seconds, %ld nanoseconds\n", j->wait_time.tv_sec, j->wait_time.tv_nsec);
        timespec completion={0,0};
        completion.tv_sec+=(j->end_time.tv_sec-j->start_time.tv_sec);
        completion.tv_nsec+=(j->end_time.tv_nsec-j->start_time.tv_nsec);
        if (completion.tv_nsec<0){
            completion.tv_sec--;
            completion.tv_nsec+=1000000000;
        }
        printf("Completion time: %ld seconds, %ld nanoseconds\n", completion.tv_sec,completion.tv_nsec);
        printf("\n");
        free(j->job_name);
        free(j);
    }
    sem_close(pipe_semaphore);
    exit(0);
}
