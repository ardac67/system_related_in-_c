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

typedef struct MessageNode {
    char* message;
    struct MessageNode* next;
} MessageNode;

typedef struct {
    MessageNode* front;
    MessageNode* rear;
    pthread_mutex_t lock;
} MessageQueue;

MessageQueue* createMessageQueue() {
    MessageQueue* queue = (MessageQueue*)malloc(sizeof(MessageQueue));
    if (!queue) {
        perror("Failed to allocate message queue");
        exit(EXIT_FAILURE);
    }
    queue->front = queue->rear = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    return queue;
}

void enqueueMessage(MessageQueue* queue, const char* message) {
    pthread_mutex_lock(&queue->lock);
    MessageNode* newNode = (MessageNode*)malloc(sizeof(MessageNode));
    newNode->message = strdup(message);  
    newNode->next = NULL;
    if (queue->rear == NULL) {
        queue->front = queue->rear = newNode;
    } else {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }
    pthread_mutex_unlock(&queue->lock);
}

char* dequeueMessage(MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    if (queue->front == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }
    MessageNode* temp = queue->front;
    char* message = temp->message;
    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
    free(temp);
    pthread_mutex_unlock(&queue->lock);
    return message;
}
void clearMessageQueue(MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    MessageNode* current = queue->front;
    while (current != NULL) {
        MessageNode* next = current->next;
        free(current->message);  
        free(current);         
        current = next;        
    }
    queue->front = queue->rear = NULL;
    pthread_mutex_unlock(&queue->lock);
}
