#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>



void *listener_thread(void *arg);
void *sender_thread(void *arg);
int generate_location_x(int p,int shop_location_x);
int generate_location_y(int q,int shop_location_y);
int getRandomNumber(int min, int max);
void send_orders(int sock, int num_client, int shop_location_x, int shop_location_y,int p,int q);
void* dummy_thread(void* arg);
void log_message(const char *message);

int sock;
int shouldExit = 1;
int *new_sock;

sem_t binary_sem;
pthread_t dummy;
pthread_t listener_thread_dummy;

void sigint_handler(int sig) {
    printf("\nSignal.. cancelling orders..editing log\n");
    sem_wait(&binary_sem);
    log_message("Client cancelled orders.");
    sem_post(&binary_sem);
    close(sock);
    free(new_sock);
    pthread_cancel(listener_thread_dummy);
    pthread_cancel(dummy);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    sem_init(&binary_sem, 0, 1);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGINT");
        return EXIT_FAILURE;
    }
    pthread_t num_threads[3];
    
    if(argc < 6) {
        printf("Usage: %s <server_ip> <port> <numClient> <p> <q>\n", argv[0]);
        return -1;
    }

    const char *server_ip = argv[1];
    int PORT = atoi(argv[2]);
    int num_client = atoi(argv[3]);
    int p = atoi(argv[4]);  
    int q = atoi(argv[5]);  

    int shop_location_x = p/2;
    int shop_location_y = q/2;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address / Address not supported \n");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        close(sock);
        return -1;
    }

    new_sock = malloc(sizeof(int));
    if (new_sock == NULL) {
        printf("Failed to allocate memory for socket descriptor\n");
        close(sock);
        return -1;
    }
    *new_sock = sock;



    
    send_orders(sock, num_client, shop_location_x, shop_location_y,p,q);

    pthread_t listener_thread_id;
    if (pthread_create(&listener_thread_id, NULL, listener_thread, new_sock) != 0) {
        printf("Failed to create thread\n");
        free(new_sock);
        close(sock);
        return -1;
    }
    listener_thread_dummy = listener_thread_id;
    pthread_t dummy_thread_id;
    
    if (pthread_create(&dummy_thread_id, NULL, dummy_thread, NULL) != 0) {
        printf("Failed to create thread\n");
        free(new_sock);
        close(sock);
        return -1;
    }
    dummy = dummy_thread_id;

    pthread_join(listener_thread_id, NULL);
    pthread_join(dummy_thread_id, NULL);
    
    close(sock);
    //free(new_sock);
    return 0;
}

void *listener_thread(void *arg) {
    int sock = *(int *)arg;
    char buffer[2048] = {0};
    int idx = 0;

    while (1) {
        int bytes_read = recv(sock, buffer + idx, sizeof(buffer) - idx - 1, 0);
        if (bytes_read > 0) {
            idx += bytes_read;
            buffer[idx] = '\0';

            char *start = buffer; 
            for (int i = 0; i < idx; i++) {
                if (buffer[i] == '\n') { 
                    buffer[i] = '\0';
                    if(strcmp(buffer,"OK") != 0){
                        printf("Response: %s\n", start);
                        sem_wait(&binary_sem);
                        log_message(start);
                        sem_post(&binary_sem);
                        if(strcmp(buffer,"ALL ORDERS SERVED") == 0){
                            shouldExit = 0;
                            sem_wait(&binary_sem);
                            log_message(start);
                            sem_post(&binary_sem);
                            free(new_sock);
                            close(sock);
                            pthread_cancel(dummy);
                            pthread_cancel(pthread_self());
                            
                            //exit(EXIT_SUCCESS);
                        }
                        else if(strcmp(buffer,"ALL ORDERS CANCEL") == 0){
                            sem_wait(&binary_sem);
                            log_message(start);
                            sem_post(&binary_sem);
                            shouldExit = 0;
                            free(new_sock);
                            close(sock);
                            pthread_cancel(dummy);
                            pthread_cancel(pthread_self());
                            
                            //exit(EXIT_SUCCESS);
                        }
                    }        
                    start = buffer + i + 1;
                }
            }

            if (start != buffer) {
                memmove(buffer, start, idx - (start - buffer));
                idx -= (start - buffer);
            }
        } else if (bytes_read == 0) {
            sem_wait(&binary_sem);
            log_message("Server closed the connection");
            sem_post(&binary_sem);
            printf("Server closed the connection\n");
            break;
        } else {
            perror("recv failed");
            break;
        }
    }
    return NULL;
}




void send_orders(int sock, int num_client, int shop_location_x, int shop_location_y,int p,int q) {
    char buffer[1024] = {0};
    char response[1024] = {0};
    for (int i = 0; i < num_client; i++) {
        memset(buffer, 0, sizeof(buffer));
        int x = generate_location_x(p,shop_location_x);
        int y = generate_location_y(q,shop_location_y);
        sprintf(buffer, "%d,%d,%d", i, x, y);
        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("send failed");
            break;
        }
        sem_wait(&binary_sem);
        log_message(buffer);
        sem_post(&binary_sem);

        memset(buffer, 0, sizeof(buffer));
        if(recv(sock, response, sizeof(response), 0) < 0){
            perror("recv failed");
            break;
        }
        
        if(strcmp(response,"OK\n") == 0){

            memset(response, 0, sizeof(response));
        }
        else{
            printf("Response: %s\n",response);
            memset(response, 0, sizeof(response));
        }
    }
    send(sock, "DONE", 4, 0);
    //send(sock, "done", 4, 0);
}

int generate_location_x(int p,int shop_location_x){
    int min = 0;
    int max = p;
    int x = 0;
    while(1){
        x = getRandomNumber(min, max);
        if(0 != shop_location_x){
            break;
        }
    }
    return x;
}

int generate_location_y(int q,int shop_location_y){
    int min = 0;
    int max = q;
    int y = 0;
    while(1){
        y = getRandomNumber(min, max);
        if(0 != shop_location_y){
            break;
        }
    }
    return y;
}

int getRandomNumber(int min, int max) {
    return min + rand() % (max - min + 1);
}


void* dummy_thread(void* arg){

    int c;

    while (shouldExit) {
        c = getchar(); 

        if (c == EOF) {
            if (feof(stdin)) {

                printf("\nClient cancelled orders.\n");
                sem_wait(&binary_sem);
                log_message("Client cancelled orders.");
                sem_post(&binary_sem);
                free(new_sock);
                close(sock);
                exit(EXIT_SUCCESS);
                break;
            } else if (ferror(stdin)) {

                perror("Error reading from stdin");
                exit(EXIT_FAILURE);
            }
        } else {

            putchar(c);
        }
    }
    return NULL;
}

void log_message(const char *message) {
    FILE *file = fopen("client.log", "a");
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
