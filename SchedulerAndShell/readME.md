
# Simple Scheduler Implementation (with Bonus - Priority)

## SHELL

### Simple Shell
- Simple Shell implementation re-used from previous assignment with slight modifications to accommodate for the “submit” command.

### Launch
- Checks if it's an empty string, then returns and continues the shell loop.
- If the first argument is not “submit” then continues with the normal execution of that command.
- If the first argument is “submit” then writes the command to the pipe’s write end and returns.

### Read User Input
- Reads user input from `stdin` and stores it in a character array.

### Shell Loop
- Infinite `while` loop that reads user input and sends it to the launch function to execute based on the command provided.

### Shell Signal Handler
- Handles calls to `SIGINT` by waiting for the child scheduler process to finish and terminate.

### Main
- Takes `NCPU` and `TSLICE` (in milliseconds) as command-line arguments.
- Initializes pipe and closes the read end.
- Forks and executes the scheduler process.
- Sets up signal handler using `sigaction`.
- Calls the shell loop.

---

## DUMMY MAIN

- Raises `SIGSTOP` signal to wait for a signal before starting any execution.
- Included as a header file in all executables to be run through the scheduler, as it ensures that execution does not start immediately but waits for an appropriate signal from the scheduler.
- `SIGINT` is being masked so that only shell and scheduler receive the signal, but currently scheduled processes run until termination.

---

## SCHEDULER

- `Struct Job_PCB` defined for process’s PCB details.
- `Struct Queue` defined for maintaining running, ready, and terminated state process queues.

### Context Switch
- Checks if there are any processes currently either in the running or ready queue.
- If yes, dequeues all the processes from the running queue.
- If the process has terminated in the current time slice, then enqueues it to the terminated queue.
- Else enqueues it to the ready queue.
- Puts processes from the ready queue to the running queue (If the process never started, then forks it, initializes its `Job_PCB` and calls `exec`. Even if `exec` fails, CPU has still been assigned to that process for this time slice) until the ready queue is empty or all CPUs are used.

### Scheduler Signal Handler
- If `SIGINT` is received, updates the global variable `sigint_received` from 0 to 1, so that the `while` loop in the scheduler knows to terminate after all currently alive processes finish.

### Add to Ready
- Takes the already read command from the pipe, parses it to separate all the commands given in the previous time slice.
- For each command, creates a job and updates the job name as the argument after “submit” in the command.
- Enqueues the job to `ready_queue`.

### Set NonBlocking
- Sets the pipe’s read end as non-blocking to account for cases in which, in a particular time slice, no command has been written to the pipe.

### Main
- Sets up a signal handler for `SIGINT`.
- Takes `NCPU` and `TSLICE` (in milliseconds) as arguments and stores them as integer and float values.
- Sets up the pipe’s read and write end.
- Sets up the scheduler's loop, which only wakes up and performs tasks every `TSLICE` seconds.
- In the loop, checks if any command has been written to the pipe in the previous time slice.
- If yes, adds them to the ready queue.
- Performs context switch.
- Once the `SIGINT` signal is received, as well as all jobs are terminated, prints the history for every executed command using “submit”.

---

## BONUS
- **4-level priority queue** implemented with 1 being the highest priority and 4 being the lowest priority.
- A process when created is enqueued into the ready queue of the respective priority.
- While dequeuing, **round-robin fashion** is followed for priority levels from 1 to 4 in order.
- Assumed that jobs arriving in the same time slice are arriving at the same time (no first come, first serve used here).
- **User-given priority is final** until the termination of the process.

---

## GitHub Repository Link
[https://github.com/MayankK-20/Operating_System]

---

## Contributions by:
- **Mayank Kumar** - 2023317  
- **Saanvi Singh** - 2023454
```
