#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>


#define FILENAME 400
#define MAX_FILES 100000
// Structure to hold file copy tasks
// simple struct for file name holder for Buffer struct
typedef struct {
    char source_destination[FILENAME];
    char target_destination[FILENAME];
} FileNames;

// Shared buffer for tasks
//includes file destination and source destination
// in and out are the indexes of the buffer
// count is the number of elements in the buffer
// buffSize is the size of the buffer which is predefined
// buffer_sync is the mutex for the buffer
// not_empty is the condition variable for the buffer 
// not_full is the condition variable for the buffer
// stat is the mutex for the statistic of file such as file
// statistic_of_files is the struct that holds the statistics of the file transfers
typedef struct {
    FileNames *tasks;
    int in, out;
    int count;
    int buffSize;
    pthread_mutex_t buffer_sync;
    pthread_mutex_t stat;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} Buffer;

// Structure to hold statistics of file transfers
// files_copied is the number of files copied
// directories_copied is the number of directories copied
// bytes_copied is the total number of bytes copied
// file_counter is the number of regular files
// fifo_file_counter is the number of fifo files
typedef struct {
    int files_copied;
    int directories_copied;
    int bytes_copied;
    int file_counter;
    int fifo_file_counter;
} statistics;



// Structure to hold parameters for the manager thread
// source_path is the source directory
// destination_path is the destination directory
// start is the start time of the manager thread
// end is the end time of the manager thread
typedef struct {
    char* source_path;
    char* destination_path;
    struct timespec *start;
    struct timespec *end;
} managerParams;

Buffer buffer;
statistics statistic_of_files;
int active = 1; // Flag to indicate if the all files send or not
int number_active_threads = 0;

pthread_barrier_t worker_barrier;

void* manager(void* params);
void* worker(void* arg);
void fileCopy(const char* source_file, const char* target_file);

void handle_sigint(int sig) {
    active = 0;
    printf("Signal received quitting..\n");
    pthread_mutex_destroy(&buffer.buffer_sync);
    pthread_mutex_destroy(&buffer.stat);
    pthread_cond_destroy(&buffer.not_empty);
    pthread_cond_destroy(&buffer.not_full);
    exit(EXIT_SUCCESS);
}

void* manager(void* params) {
    managerParams *parameter = (managerParams*) params;
    struct timespec start_temp, end_temp;
    DIR* dir;
    struct dirent* entry;

    clock_gettime(CLOCK_MONOTONIC, &start_temp);
    
    // Create the head of the directory list
    typedef struct dir_node {
        char src[FILENAME];
        char dest[FILENAME];
        struct dir_node* next;
    } DirNode;

    // Initialize the head of the list
    DirNode* head = malloc(sizeof(DirNode));
    if (!head) {
        perror("Memory allocation failed");
        return NULL;
    }
    // Copy the source and destination paths to the head
    strcpy(head->src, parameter->source_path);
    strcpy(head->dest, parameter->destination_path);
    head->next = NULL;

    // Start processing the directories
    DirNode* current = head;
    while (current != NULL) {
        //if returns 0 then this means directory is created and started the copy itself
        if(mkdir(current->dest, 0777) == 0 ){
            statistic_of_files.directories_copied++;      
        }
        // Open the source directory
        DIR* dir = opendir(current->src);
        if (!dir) {
            perror("Failed to open directory");
            current = current->next;
            continue;
        }

        struct dirent* entry;
        // Process each entry in the directory
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            char src_path[FILENAME], dest_path[FILENAME];
            snprintf(src_path, 2048, "%s/%s", current->src, entry->d_name);
            snprintf(dest_path, 2048, "%s/%s", current->dest, entry->d_name);

            struct stat statbuf;
            if (stat(src_path, &statbuf) == 0) {
                // If the entry is a regular file, add it to the buffer
                if (S_ISREG(statbuf.st_mode)) {
                    //lock mutex for buffer sync    
                    pthread_mutex_lock(&buffer.buffer_sync);
                    //wait until buffer is not full
                    while(buffer.count == buffer.buffSize && active) {
                        pthread_cond_wait(&buffer.not_full, &buffer.buffer_sync);
                    }
                    //copy the source and destination path to the buffer
                    //increment the in index and count
                    //signal the worker threads
                    strcpy(buffer.tasks[buffer.in].source_destination, src_path);
                    strcpy(buffer.tasks[buffer.in].target_destination, dest_path);
                    buffer.in = (buffer.in + 1) % buffer.buffSize;
                    buffer.count++;
                    pthread_cond_signal(&buffer.not_empty);
                    //release the buffer lock
                    pthread_mutex_unlock(&buffer.buffer_sync);
                } 
                // If the entry is a directory, add it to the list
                else if (S_ISDIR(statbuf.st_mode)) {
                    DirNode* new_node = malloc(sizeof(DirNode));
                    if (!new_node) {
                        perror("Memory allocation failed");
                        closedir(dir);
                        return NULL;
                    }
                    strcpy(new_node->src, src_path);
                    strcpy(new_node->dest, dest_path);
                    new_node->next = current->next;
                    current->next = new_node;
                }
            }
        }
        closedir(dir);
        clock_gettime(CLOCK_MONOTONIC, &end_temp);
        // Move to the next directory
        parameter->start = &start_temp;
        parameter->end = &end_temp;
        DirNode* temp = current;
        current = current->next;
        free(temp);
    }

    //all data processed
    active = 0;
    //signal the worker threads 
    pthread_cond_broadcast(&buffer.not_empty); 
    return NULL;
}


void* worker(void* arg) {
    //printf("Worker starts\n");
    char src[FILENAME], dest[FILENAME];
    pthread_barrier_wait(&worker_barrier);//starting phase barrier , waits for all threads to start

    //if there is data processed and the buffer is not empty
    while (active || buffer.count > 0) {
        //try to acquire the buffer lock
        
        pthread_mutex_lock(&buffer.buffer_sync);
        //wait until buffer is not empty
        while(buffer.count == 0 && active) {
            pthread_cond_wait(&buffer.not_empty, &buffer.buffer_sync);
        }
        if (buffer.count > 0) {
            //printf("Worker in buffer\n");
            //consume the buffer
            strcpy(src, buffer.tasks[buffer.out].source_destination);
            strcpy(dest, buffer.tasks[buffer.out].target_destination);
            buffer.out = (buffer.out + 1) % buffer.buffSize;
            buffer.count--;
            //signal the manager thread to add more data
            pthread_cond_signal(&buffer.not_full);
        } else {
            //if buffer is empty then set src to empty
            src[0] = '\0';
            //signal the manager thread to add more data
            pthread_mutex_unlock(&buffer.buffer_sync);

        }
        //release the buffer lock
        pthread_mutex_unlock(&buffer.buffer_sync);
        if (src[0] != '\0') {
            //start copy the files in buffer
            //pthread_barrier_wait(&worker_barrier);
            fileCopy(src, dest);
            //printf("Copied: %s to %s\n", src, dest);
            //lock the statistic mutex to not clash with other threads
            pthread_mutex_lock(&buffer.stat);
            struct stat filestat;
            //get the file statistics
            //increment the file counter and bytes copied
            if (stat(src, &filestat) == 0) {
                statistic_of_files.bytes_copied += filestat.st_size;
            }
            //check the fifo file
            if(S_ISFIFO(filestat.st_mode) == 1){
                //statistic_of_files.files[statistic_of_files.file_counter] = buffer.tasks[statistic_of_files.file_counter];
                statistic_of_files.fifo_file_counter++;
            }
            //check the regular file
            else if(S_ISREG(filestat.st_mode) == 1){
                //statistic_of_files.files[statistic_of_files.file_counter] = buffer.tasks[statistic_of_files.file_counter];
                statistic_of_files.file_counter++;
            }
            //increment the copied files (total)
            statistic_of_files.files_copied++;
            //increment the active threads to further usage
            number_active_threads++;
            //unlock the statistic mutex
            pthread_mutex_unlock(&buffer.stat);
        }
        
    }
    pthread_barrier_wait(&worker_barrier);//end barrier , waits for all threads to finish
    return NULL;
}

//simple file copy with input fd and output fd
//starting reading with fd1 and writing with the fd2
void fileCopy(const char* source_file, const char* target_file) {
    int fd1, fd2;
    ssize_t read_file, write_file;
    char buffer[4096];

    fd1 = open(source_file, O_RDONLY);
    if (fd1 == -1) {
        perror("Error opening source file");
        return;
    }

    fd2 = open(target_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd2 == -1) {
        perror("Error opening destination file");
        close(fd1);
        return;
    }

    while ((read_file = read(fd1, buffer, sizeof(buffer))) > 0) {
        write_file = write(fd2, buffer, read_file);
        if (write_file != read_file) {
            perror("Error writing to destination file");
            close(fd1);
            close(fd2);
            return;
        }
    }

    close(fd1);
    close(fd2);
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <buffer size> <number of workers> <source directory> <destination directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    //signal handler setup for sigint
    struct sigaction action;
    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);

    int buffer_size = atoi(argv[1]);
    int num_workers = atoi(argv[2]);
    pthread_t managerThread; 
    pthread_t workersThread[num_workers];

    managerParams *man_params = malloc(sizeof(managerParams));

    //set manager thread parameters
    man_params->destination_path = argv[4];
    man_params->source_path = argv[3];

    //initialize the statistic of files struct
    statistic_of_files.files_copied = 0;
    statistic_of_files.directories_copied = 0;
    statistic_of_files.bytes_copied = 0;
    statistic_of_files.file_counter = 0;    
    statistic_of_files.bytes_copied = 0;
    statistic_of_files.fifo_file_counter = 0;
    
    //initialize the buffer struct
    buffer.buffSize = buffer_size;
    buffer.tasks = malloc(buffer_size * sizeof(FileNames));
    
    //initialize the buffer and stat mutex and condition variables
    pthread_mutex_init(&buffer.buffer_sync, NULL);
    pthread_mutex_init(&buffer.stat,NULL);
    pthread_cond_init(&buffer.not_empty, NULL);
    pthread_cond_init(&buffer.not_full, NULL);
    pthread_barrier_init(&worker_barrier, NULL, num_workers);
    
    struct timespec start_of_manager; 
    struct timespec end_of_worker;
    clock_gettime(CLOCK_MONOTONIC, &start_of_manager);

    //wait for manager and workers terminate
    pthread_create(&managerThread, NULL, manager, man_params);
    for (int i = 0; i < num_workers; i++) {
        pthread_create(&workersThread[i], NULL, worker, NULL);
    }
    pthread_join(managerThread, NULL);
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workersThread[i], NULL);
    }
    //printf("3\n");
    clock_gettime(CLOCK_MONOTONIC, &end_of_worker);

    //print desired output
    printf("\n---------------STATISTICS--------------------\n");
    printf("Consumer: %d - Buffer Size: %d\n", num_workers, buffer_size);
    printf("Number of regular file %d\n",statistic_of_files.file_counter);
    printf("Number of fifo file: %d\n",statistic_of_files.fifo_file_counter);
    printf("Number of directory: %d\n",statistic_of_files.directories_copied);
    printf("TOTAL TIME: %f\n",end_of_worker.tv_sec - start_of_manager.tv_sec + (end_of_worker.tv_nsec - start_of_manager.tv_nsec) / 1e9);
    printf("TOTAL BYTES COPIED: %d\n", statistic_of_files.bytes_copied);
    free(buffer.tasks);
    free(man_params);
    pthread_mutex_destroy(&buffer.buffer_sync);
    pthread_mutex_destroy(&buffer.stat);
    pthread_cond_destroy(&buffer.not_empty);
    pthread_cond_destroy(&buffer.not_full);
    pthread_barrier_destroy(&worker_barrier);

    return EXIT_SUCCESS;
}