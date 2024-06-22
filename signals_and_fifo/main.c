#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>


#define FIFO_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH )
#define FIFO_NAME1 "fifo_1"
#define FIFO_NAME2 "fifo_2"  

int* array_of_nums(int main_parameter); //for creating random number array
void summation_child(); // for summation child process
void multiplication_child(); // for multiplication child process
void set_signals();
volatile int child_exit_count = 0; // Counter for exited children

void handle_sigchld(int sig) { //signal handler for exited childrens
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) { //gets the exited status of childrens
        printf("Child with PID %d exited with status %d\n", pid, WEXITSTATUS(status));
        child_exit_count++;
    }
}

void handle_synchronization(int sig) {
    //just for synchronization
}


int main(int argc, char* argv[]) {

    set_signals();
    sigset_t mask, oldmask, sus_mask;
    // Block signals before fork happened
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);


    srand(time(NULL));
    if (argc < 2) {
        printf("Usage: %s <integer>\n", argv[0]);
        return 1;
    }
    int main_parameter = atoi(argv[1]);

    //creating fifo
    if (mkfifo(FIFO_NAME1, FIFO_PERMS) == -1 || mkfifo(FIFO_NAME2, FIFO_PERMS) == -1) {
        perror("mkfifo");
        return 1;
    }

    //creating random number array
    int* array = array_of_nums(main_parameter);

    pid_t child1 = fork();
    if (child1 < 0) {
        perror("fork");
        return 1;
    }
    if (child1 == 0) {
        summation_child();//child process for summation
        exit(EXIT_SUCCESS);//exiting child process
    }
    else if (child1 > 0) {
        pid_t child2 = fork();
        if (child2 < 0) {
            perror("fork");
            return 1;
        }
        if (child2 == 0) {
            multiplication_child();//child process for multiplication
            exit(EXIT_SUCCESS);//exiting child process
        }
    }

    //Parent process
    //writes to first child fifo for array
    int fd = open(FIFO_NAME1, O_WRONLY);
    if (fd == -1) {
        perror("Failed to open FIFO");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < main_parameter; i++) {
        write(fd, &array[i], sizeof(array[i]));
    }
    close(fd);


    //fifo2 to writing for array
    int fd2 = open(FIFO_NAME2, O_WRONLY);

    // Set up the mask for sigsuspend
    sigemptyset(&sus_mask);  // Unblock all signals for sigsuspend
    sigsuspend(&sus_mask);   // Wait for synchronization from the first child


    if (fd2 == -1) {
        perror("Failed to open FIFO");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < main_parameter; i++) {
        write(fd2, &array[i], sizeof(array[i]));
    }
    close(fd2);

    
    // set up the mask for sigsuspend
    sigemptyset(&sus_mask);  // unblock all signals for sigsuspend
    sigsuspend(&sus_mask);   // wait for sychronization second the first child


    fd2 = open(FIFO_NAME2, O_WRONLY); // opening fd2 for sending multiplication data
    char* buffer = "multiplication";
    write(fd2, buffer, strlen(buffer) + 1);
    close(fd2);


    while (child_exit_count < 2) { //waiting for all children to exit
        printf("Parent process proceeding\n");
        sigsuspend(&oldmask);
        sleep(2);
    }
    
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    
    unlink(FIFO_NAME1);//deleting fifo1
    unlink(FIFO_NAME2);//deleting fifo2
}


int* array_of_nums(int main_parameter) {

    int* array = (int*)malloc(main_parameter * sizeof(int));
    printf("Random Numbers: ");
    for (int i = 0; i < main_parameter; i++) {
        array[i] = rand() % 10;
        printf("%d ", array[i]);
    }
    printf("\n");
    return array;
}


void summation_child() {
    //opening file descriptors accporting to the fifo names
    int fd = open(FIFO_NAME1, O_RDONLY);
    int fd2 = open(FIFO_NAME2, O_WRONLY);
    if (fd == -1 || fd2 == -1) {
        perror("Failed to open FIFO");
        exit(EXIT_FAILURE);
    }

    printf("Summation Child process: Sleeping for 10 seconds %d...\n", getpid());
    sleep(10);


    int num, sum = 0;
    //reads fifo1 and calculates the sum
    while (read(fd, &num, sizeof(num)) > 0) {
        sum += num;
    }

    //writes the sum above to fifo2
    if(write(fd2, &sum, sizeof(sum)) == -1){
        perror("Failed to write to FIFO");
        exit(EXIT_FAILURE);
    }

    


    //sends signal to parent "i am done"
    kill(getppid(), SIGUSR1);
    
    
    close(fd2);
    close(fd);
}

void multiplication_child() {
    //opens the fifo2 for reading
    int fd = open(FIFO_NAME2, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open FIFO");
        exit(EXIT_FAILURE);
    }
    printf("Multiplication Child process: Sleeping for 10 seconds %d...\n", getpid());
    sleep(10);


    int summation_from_first_child = 0;
    //reads the summation from the first child
    if(read(fd, &summation_from_first_child, sizeof(summation_from_first_child)) == -1){
        perror("Failed to read from FIFO");
        exit(EXIT_FAILURE);
    }

    //reads the numbers from fifo2 and calculates the multiplication
    int num, mul = 1;
    while (read(fd, &num, sizeof(num)) > 0) {
        mul *= num;
    }
    close(fd);
    //sends signal to parent "i am done"
    //with this signal parent will send multpilication data to fifo2
    kill(getppid(), SIGUSR1);

    fd = open(FIFO_NAME2, O_RDONLY);

    if(fd == -1){
        perror("Failed to open FIFO");
        exit(EXIT_FAILURE);
    }

    char command[20];

    //read "multiplication" data from pipe2 if not do failed exit
    if(read(fd, command, sizeof(command))== -1){
        perror("Failed to read from FIFO");
        exit(EXIT_FAILURE);
    }; // Read the command first

    if (strcmp(command, "multiplication") != 0) {
        exit(EXIT_FAILURE);
    }

    close(fd);

    //print the final result
    printf("Final result: %d\n", mul + summation_from_first_child);

    //send signal again to parent "i am done"
    kill(getppid(), SIGUSR1);
}

void set_signals(){
    struct sigaction sa;
    // Set up the SIGUSR1 handler
    sa.sa_handler = handle_synchronization;
    sigemptyset(&sa.sa_mask); 
    sa.sa_flags = 0; 
    sigaction(SIGUSR1, &sa, NULL);

    // Set up the SIGCHLD handler
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask); 
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}