#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "queue.h"
#include "messageQueue.h"
#include "matrix.h"
#include <fcntl.h>
#include <sys/select.h>


#define MAX_ORDER 100
#define DELIVERY_BAG 3

typedef struct {
    int order_id;
    int x;
    int y;
    int status;
} Order;

typedef struct {
    int *socket;
    Order *orders;
} manager_argument;
typedef struct {
    Order *orders;
} deliveryargs;

typedef struct{
    int *socket;
    Order *orders;
} cook_argument;

typedef struct{
    int *socket;
    int deliveryPool;
    int cookPool;
    Order *orders;
}sending_argument;

enum OrderStatus {
    WAITING = 0,
    PREPARING,
    COOKING,
    COOKED,
    DELIVERING,
    DELIVERED,
    DONE
};

void send_situation_of_order(int type,int order_id,int socket);
void *manager_thread(void *arg);
void* cook_thread(void *arg);
void* delivery_thread(void *arg);
void* sender_thread(void* arg);
int check_for_others(Order *orders);
int sleep_for_double(double seconds);
double calculate_hypotenuse(int a, int b);
void* dummy_thread(void* arg);
void log_message(const char *message);
void total_cleanup2(int deliveryPoolSize, int cookPoolSize,Order *orders,int sock);
void total_cleanup(int deliveryPoolSize, int cookPoolSize,Order *orders,int sock);
void total_cleanup3(int deliveryPoolSize, int cookPoolSize,Order *orders,int sock);
volatile int no_more_orders = 0; 
int order_counter_global = 0;
int next = 0;

int order_cancelled = 0;

Queue *ready_order_queue;
MessageQueue* messageQueue;
Queue *delivery_queue;
sem_t sem_kurek;
Order _new_order = {-1, -1, -1, WAITING};
sem_t oven_cap;
sem_t oven_access_entrance;
sem_t oven_access_exit;
sem_t test_lock;
sem_t queue_lock;
sem_t delivery_queue_lock;
sem_t available_motorcycle;
sem_t cook_counter;
sem_t log_sem;
int speedglobal;

int cookPoolSize_g;
int deliveryPoolSize_g;
int new_socket;
Order* global_order;
int exitFromDummy = 0;

typedef struct{
    long thread_id;
    int p_count;
} thread_stat;
sem_t thread_stat_lock;


thread_stat *thread_stat_arr;

pthread_mutex_t _order_full_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t consume_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_for_new_order = PTHREAD_COND_INITIALIZER;
pthread_cond_t can_consume = PTHREAD_COND_INITIALIZER;
pthread_cond_t can_deliver = PTHREAD_COND_INITIALIZER;
pthread_mutex_t _delivery_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t order_status_mutex = PTHREAD_MUTEX_INITIALIZER;


void sigint_handler(int sig) {
    send(new_socket, "ALL ORDERS CANCEL\n", 18, 0);
    printf("Server quits\n");
    sem_destroy(&sem_kurek);
    sem_destroy(&oven_cap);
    sem_destroy(&oven_access_entrance);
    sem_destroy(&oven_access_exit);
    sem_destroy(&queue_lock);
    sem_destroy(&test_lock);
    sem_destroy(&delivery_queue_lock);
    sem_destroy(&available_motorcycle);
    sem_destroy(&log_sem);
    sem_destroy(&thread_stat_lock);
    
    exit(EXIT_SUCCESS);
}

void handle_sigpipe(int signum) {
    if (signum == SIGPIPE) {
        printf("Orders cancalled\n");
        char logFile[2048];
        snprintf(logFile, sizeof(logFile), "Orders cancelled by client\n");
        //order_cancelled = 1;
        sem_wait(&log_sem);
        log_message(logFile);
        sem_post(&log_sem);

        total_cleanup3(deliveryPoolSize_g, cookPoolSize_g, global_order, new_socket);
    }
}

void total_cleanup(int deliveryPoolSize, int cookPoolSize,Order *orders,int sock) {
    order_counter_global = 0;
    next = 0;
    no_more_orders = 0;
    order_cancelled = 0;
    exitFromDummy = 1;
    send(sock, "ALL ORDERS SERVED\n", 18, 0);
    close(sock);
    char logFile[2048];
    snprintf(logFile, sizeof(logFile), "ALL ORDERS SERVED\n");
    sem_wait(&log_sem);
    log_message(logFile);
    sem_post(&log_sem);

    while (!isEmptyQueue(ready_order_queue)) {
        dequeue(ready_order_queue);  
    }
    while (!isEmptyQueue(delivery_queue)) {
        dequeue(delivery_queue);  
    }

    sem_destroy(&available_motorcycle);
    sem_init(&available_motorcycle, 0, deliveryPoolSize); 

    if (orders != NULL) {
        free(orders);
        orders = (Order *)malloc(sizeof(Order) * MAX_ORDER);
    }

    if (messageQueue != NULL) {
        clearMessageQueue(messageQueue); 
    }
    
}

void total_cleanup2(int deliveryPoolSize, int cookPoolSize,Order *orders,int sock) {
    order_counter_global = 0;
    next = 0;
    no_more_orders = 1;
    exitFromDummy = 1;
    order_cancelled = 1;
    close(sock);
    while (!isEmptyQueue(ready_order_queue)) {
        dequeue(ready_order_queue);
    }
    while (!isEmptyQueue(delivery_queue)) {
        dequeue(delivery_queue);
    }


    sem_destroy(&available_motorcycle);
    sem_init(&available_motorcycle, 0, deliveryPoolSize);

    if (orders != NULL) {
        orders = (Order *)malloc(sizeof(Order) * MAX_ORDER);
    }


    if (messageQueue != NULL) {
        clearMessageQueue(messageQueue);
    }
    
}

void total_cleanup3(int deliveryPoolSize, int cookPoolSize,Order *orders,int sock) {
    order_counter_global = 0;
    next = 0;
    no_more_orders = 1;
    exitFromDummy = 1;
    order_cancelled = 1;
    send(new_socket, "ALL ORDERS CANCEL\n", 19, 0);
    close(sock);
    char logFile[2048];
    snprintf(logFile, sizeof(logFile), "ALL ORDERS CANCELLED BY SERVER\n");
    sem_wait(&log_sem);
    log_message(logFile);
    sem_post(&log_sem);
    
    while (!isEmptyQueue(ready_order_queue)) {
        dequeue(ready_order_queue); 
    }
    while (!isEmptyQueue(delivery_queue)) {
        dequeue(delivery_queue);
    }


    sem_destroy(&available_motorcycle);
    sem_init(&available_motorcycle, 0, deliveryPoolSize);
    

    if (orders != NULL) {
        orders = (Order *)malloc(sizeof(Order) * MAX_ORDER);
    }
    if (messageQueue != NULL) {
        clearMessageQueue(messageQueue); 
    }
    
}

int main(int argc, char* argv[]) {
    char log[2048];
    snprintf(log, sizeof(log), "Server started\n");
    log_message(log);
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("Failed to set signal handler");
        return EXIT_FAILURE;
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); 
    sa.sa_handler = handle_sigpipe;   

    sigfillset(&sa.sa_mask);

    struct sigaction new;

    // Initialize the sigaction structure
    new.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);  // Initialize the signal mask to empty
    new.sa_flags = 0;  // No special flags

    // Set the SIGINT (Ctrl-C) signal handler
    if (sigaction(SIGINT, &new, NULL) == -1) {
        perror("Failed to set SIGINT handler");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("Failed to set SIGPIPE handler");
        return 1;
    }
    sem_init(&sem_kurek, 0, 3);
    sem_init(&oven_cap, 0, 6);
    sem_init(&oven_access_entrance, 0, 1);
    sem_init(&oven_access_exit, 0, 1);
    sem_init(&queue_lock, 0, 1);
    sem_init(&test_lock, 0, 1);
    sem_init(&delivery_queue_lock, 0, 1);
    sem_init(&log_sem,0,1);
    sem_init(&thread_stat_lock,0,1);

    Order *orders = NULL;
    orders = (Order *)malloc(sizeof(Order) * MAX_ORDER);
    ready_order_queue = createQueue(MAX_ORDER);
    if (argc < 4) { 
        printf("Usage: %s <server_ip> <port> <cookPoolSize> <deliveryPoolSize>\n", argv[0]);
        return -1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int cookPoolSize = atoi(argv[3]);
    int deliveryPoolSize = atoi(argv[4]);
    int speed = atoi(argv[5]);
    speedglobal = speed;
    sem_init(&available_motorcycle,0, deliveryPoolSize);
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    cookPoolSize_g = cookPoolSize;
    deliveryPoolSize_g = deliveryPoolSize;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(server_ip);
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on IP: %s PORT %d\n", server_ip, port);
    while (1) {
        printf("Waiting for incoming clients...\n");
        order_cancelled = 0;
        char logFile[2048];
        snprintf(logFile, sizeof(logFile), "Waiting for incoming clients...\n");
        sem_wait(&log_sem);
        log_message(logFile);
        sem_post(&log_sem);
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        int *socket_temp = malloc(sizeof(int));
        if (socket_temp == NULL) {
            printf("Failed to allocate memory for socket descriptor\n");
            close(new_socket);
            continue;
        }
        *socket_temp = new_socket;
        int *socket_temp_2 = malloc(sizeof(int));
        if (socket_temp_2 == NULL) {
            printf("Failed to allocate memory for socket descriptor\n");
            close(new_socket);
            continue;
        }
        *socket_temp_2 = new_socket;
        pthread_t manager_thread_id;
        manager_argument *arg = malloc(sizeof(manager_argument));
        arg->socket = socket_temp;
        arg->orders = orders;
        if (pthread_create(&manager_thread_id, NULL, manager_thread, arg) != 0) {
            printf("Failed to create sender thread\n");
            free(socket_temp);
            close(new_socket);
            continue;
        }

        messageQueue = createMessageQueue();
        delivery_queue = createQueue();
        pthread_t dummy_thread_id;
        if (pthread_create(&dummy_thread_id, NULL, dummy_thread, NULL) != 0) {
            printf("Failed to create thread\n");
            return -1;
        }
        sending_argument *sender_arg = malloc(sizeof(sending_argument));
        sender_arg->socket = socket_temp_2;
        sender_arg->deliveryPool = deliveryPoolSize;
        sender_arg->cookPool = cookPoolSize;
        sender_arg->orders = orders;
        pthread_t senderThreadId;
        pthread_create(&senderThreadId, NULL, sender_thread, sender_arg);
        pthread_t cook_threads[cookPoolSize];
        cook_argument *cook_arg = malloc(sizeof(cook_argument));
        cook_arg->orders = orders;
        cook_arg->socket = socket_temp_2;
        pthread_t delivery_threads[deliveryPoolSize];
        for(int i = 0; i < cookPoolSize; i++){
            pthread_create(&cook_threads[i], NULL, cook_thread, cook_arg);
        }
        deliveryargs *delivery_arg = malloc(sizeof(deliveryargs));
        delivery_arg->orders = orders;
        thread_stat_arr = malloc(sizeof(thread_stat) * (deliveryPoolSize));
        for(int i = 0; i < deliveryPoolSize; i++){
            pthread_create(&delivery_threads[i], NULL, delivery_thread, delivery_arg);
            thread_stat_arr[i].thread_id = delivery_threads[i] % 100;
            thread_stat_arr[i].p_count = 0;
            //printf("Thread %ld created\n",delivery_threads[i] % 100);
        }
        pthread_join(manager_thread_id,NULL);
        //printf("Manager out\n");
        char logFile_NEW[2048];
        snprintf(logFile_NEW, sizeof(logFile_NEW), "Manager thread out..\n");
        sem_wait(&log_sem);
        log_message(logFile);
        sem_post(&log_sem);
        for(int i = 0; i < cookPoolSize; i++){
            pthread_join(cook_threads[i],NULL);
            //printf("Cook out %d\n",i);
        }
        snprintf(logFile_NEW, sizeof(logFile_NEW), "All cooks threads out\n");
        sem_wait(&log_sem);
        log_message(logFile);
        sem_post(&log_sem);
        memset(logFile_NEW, 0, sizeof(logFile_NEW));
        for(int i = 0; i < deliveryPoolSize; i++){
            //printf("Delivery out%d\n",i);
            pthread_join(delivery_threads[i],NULL);
        }
        memset(logFile_NEW, 0, sizeof(logFile_NEW));
        snprintf(logFile_NEW, sizeof(logFile_NEW), "All delivery threads out\n");
        sem_wait(&log_sem);
        log_message(logFile);
        sem_post(&log_sem);
        memset(logFile_NEW, 0, sizeof(logFile_NEW));
        sem_wait(&log_sem);
        char write_to_log[2048];
        for(int i = 0;(i<deliveryPoolSize && order_cancelled == 0);i++){
            printf("Thread %ld delivered %d orders\n",thread_stat_arr[i].thread_id,thread_stat_arr[i].p_count);
            snprintf(write_to_log, sizeof(write_to_log), "Thread %ld delivered %d orders\n",thread_stat_arr[i].thread_id,thread_stat_arr[i].p_count);
            log_message(write_to_log);
            memset(write_to_log, 0, sizeof(write_to_log));
        }
        sem_post(&log_sem);

        pthread_join(dummy_thread_id, NULL);
        total_cleanup(deliveryPoolSize, cookPoolSize, orders, new_socket);
    }
    close(server_fd);
    return 0;
}
void send_situation_of_order(int type,int order_id,int socket){
    printf("Socket: %d\n",socket);
    char buffer[2048] = {0};
    if(type == 0){
        sprintf(buffer, "%d ORDER-STATUS-UPDATE: WAITING",order_id);
    }else if(type == 1){
        sprintf(buffer, "%d ORDER-STATUS-UPDATE: PREPARING",order_id);
    }else if(type == 2){
        sprintf(buffer, "%d ORDER-STATUS-UPDATE: COOKING",order_id);
    }else if(type == 3){
        snprintf(buffer, 2048, "%d ORDER-STATUS-UPDATE: DELIVERING",order_id);
    }else if(type == 4){
        snprintf(buffer, 2048, "%d ORDER-STATUS-UPDATE: DONE",order_id);
    }
    send(socket, buffer, strlen(buffer), 0);
}

void *manager_thread(void *arg) {
    int order_counter = 0;
    manager_argument *manager_arg = (manager_argument *)arg;
    int sock = *(int *)manager_arg->socket;
    Order *orders = manager_arg->orders;
    global_order = orders;
    char buffer[2048] = {0};
   
    while (1) {
        int bytes_read = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            //printf("We got the all orders\n");
            break;
        }
        buffer[bytes_read] = '\0';  // Null-terminate the string
        if(strcmp(buffer,"DONE") == 0){
            //printf("We got the all orders\n");
            order_counter_global = order_counter;
            sem_init(&cook_counter,0,order_counter_global);
            break;
        }
        //printf("Order: %s\n", buffer);
        char *saveptr;
        char *token = strtok_r(buffer, ",", &saveptr);
        //pthread_mutex_lock(&_order_full_mutex);
        while(token != NULL){
            orders[order_counter].order_id = atoi(token);
            token = strtok_r(NULL, ",", &saveptr);
            if(token == NULL){
                break;
            }
            orders[order_counter].x = atoi(token);
            token = strtok_r(NULL, ",", &saveptr);
            if(token == NULL){
                break;
            }
            orders[order_counter].y = atoi(token);
            orders[order_counter].status = WAITING;
            token = strtok_r(NULL, ",", &saveptr);
            _new_order.status = WAITING;
            _new_order.order_id = orders[order_counter].order_id;
            _new_order.x = orders[order_counter].x;
            _new_order.y = orders[order_counter].y;

            order_counter_global++;
        }  
        printf("Manager -> Order: OrderID :%d Location X:%d Location Y:%d\n", orders[order_counter].order_id, orders[order_counter].x, orders[order_counter].y);
        char log[2048];
        snprintf(log, sizeof(log), "Order %d: %d %d\n", orders[order_counter].order_id, orders[order_counter].x, orders[order_counter].y);
        sem_wait(&log_sem);
        log_message(log);
        sem_post(&log_sem);
        order_counter++;
        memset(buffer, 0, sizeof(buffer));
        send(sock, "OK\n", 3, 0);
    }
    int counter = 0;
    while(1){
        if(counter == order_counter_global){
            pthread_cond_broadcast(&cond_for_new_order);
            no_more_orders = 1;
            exitFromDummy = 1;
            break;
        }
        pthread_mutex_lock(&consume_mutex);
        while(isEmptyQueue(ready_order_queue)){
            pthread_cond_wait(&can_consume, &consume_mutex);
        }

        sem_wait(&queue_lock);
        
        int data = dequeue(ready_order_queue);
        sem_wait(&cook_counter);
        sem_post(&queue_lock);
        //printf("DATA: %d\n",data);
        for(int i=0;i<order_counter_global;i++){
            if(orders[i].order_id == data){
                
                pthread_mutex_lock(&_delivery_mutex);
                sem_wait(&delivery_queue_lock);
                enqueue(delivery_queue, orders[i].order_id);
                printf("Order (%d) is ready for departure %d %d\n", orders[i].order_id, orders[i].x, orders[i].y);
                char log[2048];
                snprintf(log, sizeof(log), "Order is ready for departure %d: %d %d\n", orders[i].order_id, orders[i].x, orders[i].y);
                sem_wait(&log_sem);
                log_message(log);
                sem_post(&log_sem);
                counter++;
                sem_post(&delivery_queue_lock);
                //sem_wait(&available_motorcycle);
                pthread_cond_broadcast(&can_deliver);
                pthread_mutex_unlock(&_delivery_mutex);
                break;
            }
        }
        pthread_mutex_unlock(&consume_mutex);    
    }


    //close(sock);
    free(arg);
    return NULL;
}


void* cook_thread(void *arg){
    cook_argument *cook_arg = (cook_argument *)arg;
    Order *orders = cook_arg->orders;

    while (!no_more_orders) {
        pthread_mutex_lock(&_order_full_mutex);
        while (next >= order_counter_global && !no_more_orders) {

            if (next >= order_counter_global && no_more_orders) {
                pthread_mutex_unlock(&_order_full_mutex);
                return NULL; 
            }
        }

        if (next >= order_counter_global) {
            pthread_mutex_unlock(&_order_full_mutex);
            continue;
        }

        int current = next++;
        pthread_mutex_unlock(&_order_full_mutex); 

        // Prepare to cook
        sem_wait(&sem_kurek);
        sem_wait(&oven_cap);
        sem_wait(&oven_access_entrance);

        // Start cooking
        pthread_mutex_lock(&order_status_mutex);
        orders[current].status = COOKING;
        pthread_mutex_unlock(&order_status_mutex);

        double time_prep = return_result();
        sleep_for_double(time_prep); 

        // Cooking done
        sem_post(&oven_access_entrance);
        sem_post(&oven_cap);
        sem_post(&sem_kurek);
        sem_wait(&oven_access_exit);
        sem_post(&oven_access_exit);

        pthread_mutex_lock(&order_status_mutex);
        orders[current].status = COOKED;
       
        pthread_mutex_unlock(&order_status_mutex);

        // Notify other threads
        pthread_mutex_lock(&consume_mutex);
        enqueue(ready_order_queue, orders[current].order_id);
        pthread_cond_signal(&can_consume);
        pthread_mutex_unlock(&consume_mutex);

        // Log status
        char response[2048];
        snprintf(response, sizeof(response), "Cook:%ld ORDER ID:%d COOKED\n", pthread_self() % 100, orders[current].order_id);
        printf("%s",response);
        enqueueMessage(messageQueue, response);
        sem_wait(&log_sem);
        log_message(response);
        sem_post(&log_sem);
    }
    return NULL;
}




void* delivery_thread(void *arg){
    deliveryargs *delivery_arg = (deliveryargs *)arg;
    Order *orders = delivery_arg->orders;
    int counter = 0;
    int arr[DELIVERY_BAG];
    int check = 0;

    while (1) {
        pthread_mutex_lock(&_delivery_mutex);
        while (isEmptyQueue(delivery_queue) && !no_more_orders) {
            pthread_cond_wait(&can_deliver, &_delivery_mutex);
        }

        if (isEmptyQueue(delivery_queue) && no_more_orders) {
            pthread_mutex_unlock(&_delivery_mutex);
            break;
        }

        sem_wait(&delivery_queue_lock);
        int data = dequeue(delivery_queue);
        sem_post(&delivery_queue_lock);
        pthread_mutex_unlock(&_delivery_mutex);

        pthread_mutex_lock(&order_status_mutex);
        int found = 0;
        for (int i = 0; i < order_counter_global; i++) {
            if (orders[i].order_id == data && orders[i].status == COOKED) {
                orders[i].status = DELIVERING;
                arr[counter++] = orders[i].order_id;
                found = 1;
                break;
            }
        }
        pthread_mutex_unlock(&order_status_mutex);

        if (counter == 1) {
            sem_wait(&available_motorcycle); 
            if(order_cancelled == 0) printf("Motorcycle: %lu starts delivery\n",pthread_self() % 100);
            sem_wait(&log_sem);
            char arda[2048];
            snprintf(arda, sizeof(arda), "Thread %lu starts delivery\n",pthread_self() % 100);
            if(order_cancelled == 0)  log_message(arda);
            sem_post(&log_sem);
            sem_wait(&thread_stat_lock);
            int x = 0;
            int y = 0;
            for(int i=0;i<order_counter_global;i++){
                if(orders[i].order_id == arr[0]){
                    x = orders[i].x;
                    y = orders[i].y;
                    break;
                }
            }
            for(int i=0;i<deliveryPoolSize_g;i++){
                if(pthread_self() % 100 == thread_stat_arr[i].thread_id){
                    thread_stat_arr[i].p_count++;
                    break;
                }
            }
            sem_post(&thread_stat_lock);
            double distance = calculate_hypotenuse(x, y);
            sleep_for_double(distance / speedglobal); 
            char response[2048];
            snprintf(response, sizeof(response), "ORDER ID:%d DELIVERED BY %lu\n", arr[0],pthread_self() % 100);
            if(order_cancelled == 0) printf("%s",response);
            enqueueMessage(messageQueue, response);
            sem_post(&available_motorcycle);
            counter = 0; 
            memset(arr, -1, sizeof(arr));
        }
    }
    return NULL;
}

void* sender_thread(void* arg) {
    sending_argument *sender_arg = (sending_argument *)arg;
    int sock = *(int *)sender_arg->socket;
    Order *orders = sender_arg->orders;
    int deliveryPoolSize = sender_arg->deliveryPool;
    int cookPoolSize = sender_arg->cookPool;
    char* message;
    while (1) {
        while ((message = dequeueMessage(messageQueue)) != NULL) {
            size_t a = send(sock, message, strlen(message), 0);
            if(a == -1){
                if(errno == EPIPE){
                    printf("Client terminated\n");
                    sem_wait(&log_sem);
                    log_message("Client terminated\n");
                    sem_post(&log_sem);
                    return NULL;
                }
            }
            free(message);
        }
        usleep(10000);
    }
    return NULL;
}



int check_for_others(Order *orders) {
    for(int i=0;i<order_counter_global;i++){
        if(orders[i].status == WAITING || orders[i].status == PREPARING || orders[i].status == COOKING || orders[i].status == COOKED){
            return 1;
        }
    }
    return 0;
}

int sleep_for_double(double seconds) {
    struct timespec req, rem;
    
    req.tv_sec = (time_t)seconds; 
    req.tv_nsec = (seconds - req.tv_sec) * 1e9; 

    if (nanosleep(&req, &rem) == -1) {
        perror("nanosleep");
        return -1;
    }

    return 0; 
}

double calculate_hypotenuse(int a, int b) {
    return sqrt(a * a + b * b);
}

void* dummy_thread(void* arg) {
    printf("Enter text (press Ctrl-D to exit):\n");

    int fd = fileno(stdin);  
    char c;

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    while (exitFromDummy != 1) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set);

        struct timeval timeout;
        timeout.tv_sec = 1;  
        timeout.tv_usec = 0;

        int rv = select(fd + 1, &set, NULL, NULL, &timeout);
        if (rv == -1) {
            perror("select");  
            exit(EXIT_FAILURE);
        } else if (rv == 0) {
            if (exitFromDummy) {
                break;
            }
        } else {
            if (FD_ISSET(fd, &set)) {
                ssize_t count = read(fd, &c, 1);
                if (count == -1) {
                    perror("read");
                    exit(EXIT_FAILURE);
                } else if (count == 0) {
                    printf("\nServer cancelled orders.\n");
                    sem_wait(&log_sem);
                    log_message("Server cancelled orders.\n");
                    sem_post(&log_sem);
                    total_cleanup3(deliveryPoolSize_g, cookPoolSize_g, global_order, new_socket);
                    
                    break;
                } else {
                    write(STDOUT_FILENO, &c, 1);
                }
            }
        }
    }

    return NULL;
}

void log_message(const char *message) {
    FILE *file = fopen("server.log", "a"); 
    if (file == NULL) {
        perror("Failed to open log file");
        return;
    }
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    if (time_str) {
        time_str[strlen(time_str) - 1] = '\0'; 
    }
    fprintf(file, "[%s] %s\n", time_str, message);
    fclose(file);
}