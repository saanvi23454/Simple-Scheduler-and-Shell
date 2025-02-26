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
#include <semaphore.h>
#define PIPE_SEMAPHORE_NAME "/pipe_semaphore"

pid_t scheduler_pid;
int pipefd[2];
sem_t* pipe_semaphore;


int launch(char* command){
    if (strcmp(command,"\n")==0 ||command==NULL || strlen(command)==0){
        return 1;
    }
    char* comm = strdup(command);
    if (comm==NULL){
        perror("strdup failed");
        return 1;
    }
    char* arguments[500];
    char* token = strtok(command, " \t\n");
    int i = 0;
    while (token != NULL) {
        arguments[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    arguments[i] = NULL;
    if (strcmp(arguments[0],"submit")==0){
        sem_wait(pipe_semaphore);
        write(pipefd[1], comm, strlen(comm) + 1); //writing to pipe if command starts with submit.
        sem_post(pipe_semaphore);
        free(comm);
        return 1;
    }
    free(comm);

    //if command not submit it will execute as usual.
    int pid=fork();
    if (pid<0){
        perror("Fork Failed");
    }
    else if (pid==0){
        if (execvp(arguments[0], arguments)==-1){
            //execvp searches for executable replaces it with the child process if succesfull it will never return else return -1;
            perror("Command could not be executed");
            return 1;
        }
    }
    else{
        waitpid(pid,NULL,0);
    }
    return 1;
}

char* read_user_input(){
    char* command=NULL;                             //Where the command would be stored.
    size_t commandSize=0;                           //Size allocated for command, getline can change it.
    getline(&command, &commandSize, stdin);         //reads from stdin.
    return command;
}

void shell_loop(){
  int status;
  do{
    printf("shell: ");
    char* command = read_user_input();
    status = launch(command);
    free(command);
  } while (status);
}

static void shell_signal_handler(int signum) {
    if (signum == SIGINT){
        //printf("Recieved SIGINT Shell");
        waitpid(scheduler_pid,NULL,0);
        sem_close(pipe_semaphore);
        sem_unlink(PIPE_SEMAPHORE_NAME);
        exit(0);
    }
}

int main(int argc, char* argv[]){
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(1);
    }
    //error handling if ncpu or tslice not provided
    if (argc != 3){
        perror("insufficient command line arguments provided");
        exit(1);
    }

    //error checking if ncpu or tslice is not number
    // if (!is_numeric(argv[1] || !is_numeric(argv[2]){
    //     perror("ncpu and tslice must be positive numbers");
    //     exit(1);
    // }

    // ncpu and tslice cannot have leading + as well (can have whitespace leading)
    int NCPU = atoi(argv[1]);
    float TSLICE = strtof(argv[2],NULL);
    sem_unlink(PIPE_SEMAPHORE_NAME);
    pipe_semaphore = sem_open(PIPE_SEMAPHORE_NAME, O_CREAT | O_EXCL, 0644, 1);
    if (pipe_semaphore == SEM_FAILED) {
        perror("Failed to create semaphore");
        exit(1);
    }

    if (NCPU<=0 || TSLICE<=0.0){
        perror("invalid ncpu or tslice values");
        exit(1);
    }

    scheduler_pid = fork();
    if (scheduler_pid < 0){
        perror("fork failed!");
        exit(1);
    }
    else if (scheduler_pid == 0){
        char ncpu_str[10], tslice_str[10];
        snprintf(ncpu_str, sizeof(ncpu_str), "%d", NCPU);
        snprintf(tslice_str, sizeof(tslice_str), "%f", TSLICE);
        char read_fd_str[10], write_fd_str[10];
        sprintf(read_fd_str, "%d", pipefd[0]);  //writes second argument in buffer of first argument similar to printf just writing in read_fd_str;
        sprintf(write_fd_str, "%d", pipefd[1]);
        char* args[] = {"./scheduler", ncpu_str, tslice_str,read_fd_str, write_fd_str, NULL};
        // starting the scheduler with ncpu, tslice, read and write end of the pipe as arguments.
        if (execvp(args[0], args)==-1){
            perror("Scheduler could not be invoked");
        }
        exit(1);
    }
    close(pipefd[0]);       //closing read end not used.
    struct sigaction sig;   //setting signal handler.
    memset(&sig, 0, sizeof(sig));
    sig.sa_handler = shell_signal_handler;
    sigaction(SIGINT, &sig, NULL);
    sigemptyset(&sig.sa_mask); 
    sig.sa_flags = 0;
    shell_loop();
    return 0;
}
