#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h> 

#define SHM_CAR "/shm_car"
#define SHM_TRUCK "/shm_truck"
#define SEM_ENTRANCE "/shm_entrance"
#define SEM_SHM "/shm_sem"
#define SEM_CHARGE_AUTOMOBILE "/charge_automobile"
#define SEM_CHARGE_PICKUP "/charge_pickup"
#define SEM_PARKING_LOT_CAR "/parking_lot_car"
#define SEM_PARKING_LOT_TRUCK "/parking_lot_truck"
#define SEM_NEW_AUTOMOBILE "/new_automobile"
#define SEM_NEW_PICKUP "/new_pickup"

#define NUM_THREADS 100
#define NUM_VALE 2

//type definition for type of carAttendant
typedef enum VALE_TYPE {
    CARVALE = 0,
    TRUCKVALE
} ValeType;

//struct for car thread parameters
typedef struct {
    sem_t *sem_entrance; //entrance semaphore
    sem_t *sem_shm; // shared memory semaphore
    int *mFree_automobile; //shared memory for car count
    int *mFree_pickup;//shared memory for truck count
    sem_t *new_automobile; //semaphore for new car indication
    sem_t *new_pickup;//semaphore for new truck indication
    int * thread_count; //thread count to exit
} CarThreadParams;

//struct for vale thread parameters
typedef struct {
    ValeType type; //type of carAttendant
    sem_t * sem_charge;//semaphore for charging the car one by one
    sem_t *sem_shm;//semaphore for shared memory
    sem_t *sem_parking_lot;//semaphore for parking lot
    int *count_of_type;//shared memory for car count or truck count
    sem_t *new_automobile;//semaphore for new car indication
    sem_t *new_pickup;//semaphore for new truck indication
    int * thread_count;//thread count to exit
} ValeThreadParams;

int printRandoms(int lower, int upper);
void* carOwner(void* params);
void* carAttendant(void* params);

int main() {
    //shared memory for car count and truck count
    int shm_car = shm_open(SHM_CAR, O_CREAT | O_RDWR, 0666);
    int shm_truck = shm_open(SHM_TRUCK, O_CREAT | O_RDWR, 0666);
    if (shm_car == -1 || shm_truck == -1) {
        perror("Cannot open shared memory");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_car, sizeof(int)) == -1 || ftruncate(shm_truck, sizeof(int)) == -1) {
        perror("Cannot set size of shared memory");
        close(shm_car);
        close(shm_truck);
        exit(EXIT_FAILURE);
    }

    //shared memory for temp area availabity in terms  car count and temp truck count
    int *mFree_automobile = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_car, 0);
    int *mFree_pickup = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_truck, 0);
    if (mFree_automobile == MAP_FAILED || mFree_pickup == MAP_FAILED) {
        perror("Cannot map shared memory");
        close(shm_car);
        close(shm_truck);
        exit(EXIT_FAILURE);
    }
    *mFree_automobile = 0;
    *mFree_pickup = 0;
    
    sem_t *sem_charge_automobile = sem_open(SEM_CHARGE_AUTOMOBILE, O_CREAT, 0666, 1);
    sem_t *sem_charge_pickup = sem_open(SEM_CHARGE_PICKUP, O_CREAT, 0666, 1);
    if (sem_charge_automobile == SEM_FAILED || sem_charge_pickup == SEM_FAILED) {
        perror("Cannot create semaphore");
    }

    //semaphore for counting available parking lot
    sem_t *sem_parking_lot_car = sem_open(SEM_PARKING_LOT_CAR, O_CREAT, 0666, 8);
    sem_t *sem_parking_lot_truck = sem_open(SEM_PARKING_LOT_TRUCK, O_CREAT, 0666,4);
    if (sem_parking_lot_car == SEM_FAILED || sem_parking_lot_truck == SEM_FAILED) {
        perror("Cannot create semaphore");
    }
    //semaphore for notifying car attendant
    sem_t *sem_new_automobile = sem_open(SEM_NEW_AUTOMOBILE, O_CREAT, 0666, 0);
    sem_t *sem_new_pickup = sem_open(SEM_NEW_PICKUP, O_CREAT, 0666, 0);
    if(sem_new_automobile == SEM_FAILED || sem_new_pickup == SEM_FAILED){
        perror("Cannot create semaphore");
    }

    //semaphore for entrance one by one
    sem_t *sem_entrance = sem_open(SEM_ENTRANCE, O_CREAT, 0666, 1);
    if (sem_entrance == SEM_FAILED) {
        perror("Cannot create semaphore");
    }

    //semaphore for shared memory
    sem_t *sem_shm = sem_open(SEM_SHM, O_CREAT, 0666, 1);
    if (sem_shm == SEM_FAILED) {
        perror("Cannot create semaphore");
    }
    int sem_value;

    //for cars
    pthread_t car_threads[NUM_THREADS];
    pthread_t vale_thread[2];   

    //parameters for vale threads
    ValeThreadParams *valeParamsTruck = malloc(sizeof(ValeThreadParams));
    ValeThreadParams *valeParamsCar = malloc(sizeof(ValeThreadParams));
    valeParamsTruck->type = TRUCKVALE;
    valeParamsTruck->sem_charge = sem_charge_pickup;
    valeParamsTruck->sem_shm = sem_shm;
    valeParamsTruck->count_of_type = mFree_pickup;
    valeParamsTruck->sem_parking_lot = sem_parking_lot_truck;
    valeParamsTruck->new_automobile = sem_new_automobile;
    valeParamsTruck->new_pickup = sem_new_pickup;
    valeParamsCar->type = CARVALE;
    valeParamsCar->sem_charge = sem_charge_automobile;
    valeParamsCar->sem_shm = sem_shm;
    valeParamsCar->count_of_type = mFree_automobile;
    valeParamsCar->sem_parking_lot = sem_parking_lot_car;
    valeParamsCar->new_automobile = sem_new_automobile;
    valeParamsCar->new_pickup = sem_new_pickup;
    int thread_counter = 2;
    valeParamsCar->thread_count = &thread_counter;


    int car_vale = pthread_create(&vale_thread[0], NULL, carAttendant, (void *)valeParamsCar);
    int truck_vale = pthread_create(&vale_thread[1], NULL, carAttendant, (void *)valeParamsTruck);
    int thread_count = 0;
    
    //create threads
    while(thread_count < 100){
        int rc;
        long t;
        //parameters for car threads or truck
        CarThreadParams *carParams = malloc(sizeof(CarThreadParams));
        carParams->sem_entrance = sem_entrance;
        carParams->sem_shm = sem_shm;
        carParams->mFree_automobile = mFree_automobile;
        carParams->mFree_pickup = mFree_pickup;
        carParams->new_automobile = sem_new_automobile;
        carParams->new_pickup = sem_new_pickup;
        carParams->thread_count = &thread_count;
        rc = pthread_create(&car_threads[thread_count], NULL, carOwner, (void *)carParams);

        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            free(carParams); // Free the allocated string
            exit(-1);
        }
        thread_count++;

        sem_wait(sem_shm);    

        int truck_count,car_count;
        truck_count = *mFree_pickup;
        car_count = *mFree_automobile;
        int summation = truck_count + car_count;

        sem_post(sem_shm);
    }

    //wait for all threads to complete
    for (int t = 0; t < thread_count; t++) {
        pthread_join(car_threads[t], NULL);
    }
    for (int t = 0; t < 2; t++) {
        pthread_join(vale_thread[t], NULL);
    }

    printf("Main: program completed. Exiting.\n");

    //free(valeParamsCar);
    //free(valeParamsTruck);
    //releasing resources
    munmap(mFree_automobile, sizeof(int));
    munmap(mFree_pickup, sizeof(int));
    close(shm_car);
    close(shm_truck);
    sem_close(sem_entrance);
    sem_close(sem_shm);
    sem_close(sem_charge_automobile);
    sem_close(sem_charge_pickup);
    sem_close(sem_parking_lot_car);
    sem_close(sem_parking_lot_truck);
    sem_close(sem_new_automobile);
    sem_close(sem_new_pickup);
    sem_unlink(SEM_ENTRANCE);
    sem_unlink(SEM_SHM);
    shm_unlink(SHM_CAR);
    shm_unlink(SHM_TRUCK);
    sem_unlink(SEM_CHARGE_AUTOMOBILE);
    sem_unlink(SEM_CHARGE_PICKUP);
    sem_unlink(SEM_PARKING_LOT_CAR);
    sem_unlink(SEM_PARKING_LOT_TRUCK);
    sem_unlink(SEM_NEW_AUTOMOBILE);
    sem_unlink(SEM_NEW_PICKUP);
    return 0;
}

int printRandoms(int lower, int upper) 
{    
    int num = (rand() % (upper - lower + 1)) + lower; 
    return num;
} 

void* carOwner(void* params) {
    CarThreadParams* parameter = (CarThreadParams*)params;
    
    int rand = printRandoms(0, 20);
    
    //waiting for entrance
    sem_wait(parameter->sem_entrance);
    if(rand < 10){//determine car or truck depends on random value if one digit it is car if not it is truck
        //waiting for shared memory
        sem_wait(parameter->sem_shm);
        int car_count_in_temp_space = *(parameter->mFree_automobile);
        //if there is available space in temp parking lot
        if(car_count_in_temp_space < 8){
            printf("Car entered temp parking lot\n");
            car_count_in_temp_space++;
            *(parameter->mFree_automobile) = car_count_in_temp_space;
            //notify the vale
            sem_post(parameter->new_automobile);
        }
        else{
            printf("Car Temp parking lot is full\n");
        }
        //release shared memory lock
        sem_post(parameter->sem_shm);
    }
    else{
        sem_wait(parameter->sem_shm);
        int truck_count_in_temp_space = *(parameter->mFree_pickup);
        if(truck_count_in_temp_space < 4){
            printf("Pickup entered temp parking lot\n");
            truck_count_in_temp_space++;
            *(parameter->mFree_pickup) = truck_count_in_temp_space;
            sem_post(parameter->new_pickup);
        }
        else{
            printf("Truck Temp parking lot is full\n");
        }
        sem_post(parameter->sem_shm);
    }
    sem_post(parameter->sem_entrance);

    //decrement thread count this one exiting 
    *(parameter->thread_count)--;
    free(parameter);
    pthread_exit(NULL);
}

void* carAttendant(void* params) {
    ValeThreadParams* parameter = (ValeThreadParams*)params;
    if(parameter->type == CARVALE){
        while(1){
                //check are there any new automobile or truck
                sem_wait(parameter->new_automobile);
                //wait for shared memory
                sem_wait(parameter->sem_shm);
                int car_count = *(parameter->count_of_type);
                //lock itself to work with one by one
                sem_wait(parameter->sem_charge);
                //if there is any car and parking lot is available
                if(car_count > 0 && sem_trywait(parameter->sem_parking_lot) == 0 ){
                    printf("Car is being charged to park area\n");
                    car_count--;
                    *(parameter->count_of_type) = car_count;
                    //release lock on itself and shared memory
                    sem_post(parameter->sem_charge);
                    sem_post(parameter->sem_shm);
                }
                else{
                    //if there is no car or parking lot is full
                    //then exit
                    printf("No car to charge\n");
                    sem_post(parameter->sem_charge);
                    sem_post(parameter->sem_shm);
                    break;
                }
            
        }

    }
    else if(parameter->type == TRUCKVALE){
        while(1){
                sem_wait(parameter->new_pickup);
                sem_wait(parameter->sem_shm);
                int truck_count = *(parameter->count_of_type);
                sem_wait(parameter->sem_charge);
                if(truck_count > 0 && sem_trywait(parameter->sem_parking_lot) == 0){
                    printf("Pickup is being charged to park area\n");
                    truck_count--;
                    *(parameter->count_of_type) = truck_count;
                    sem_post(parameter->sem_charge);
                    sem_post(parameter->sem_shm);
                }
                else{
                    printf("No pickup to charge\n");
                    sem_post(parameter->sem_charge);
                    sem_post(parameter->sem_shm);
                    break;
                }
        }

    }
    else{
        printf("Invalid type\n");
         
    }
    *(parameter->thread_count)--;
    free(parameter);
    pthread_exit(NULL);
}