//The if checks if DUMMY_MAIN is defined, if it is defined it does not execute the following code.
//Basically, the if is for not including the dummy_main.h more than once.

#ifndef DUMMY_MAIN          
#define DUMMY_MAIN

#include<stdio.h>
#include<string.h>
#include<signal.h>         //For signal handling.
#include<unistd.h>

int dummy_main(int argc, char **argv);
int main(int argc, char **argv) {
    /* You can add any code here you want to support your SimpleScheduler implementation*/

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
        perror("Failed to block SIGINT");
        return 1;
    }

    raise(SIGSTOP);        //Wait for starting the execution.
    
    int ret = dummy_main(argc, argv);
    return ret;
}


#define main dummy_main
#endif
