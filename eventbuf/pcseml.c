#include <stdlib.h>
#include "eventbuf.h"
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>


int producer_count;
int consumer_count;
int events_per_producer;
int max_outstanding_events;

struct eventbuf *eventbuffer;

sem_t *mutex;
sem_t *product_count;
sem_t *availble_space;


sem_t *sem_open_temp(const char *name, int value)
{
    sem_t *sem;

    // Create the semaphore
    if ((sem = sem_open(name, O_CREAT, 0600, value)) == SEM_FAILED)
        return SEM_FAILED;

    // Unlink it so it will go away after this process exits
    if (sem_unlink(name) == -1) {
        sem_close(sem);
        return SEM_FAILED;
    }

    return sem;
}
void *producer(void *arg)
{   
    //producer id number
    int *pid = arg;
    
    for(int i = 0; i < events_per_producer; i++) {   
        //event
        int events_per_producer = *pid * 100 + i;
        // wait for space
        sem_wait(availble_space);
        // lock
        sem_wait(mutex);
        printf("P%d: adding event %d\n", *pid, events_per_producer);
        //buffer add event
        eventbuf_add(eventbuffer, events_per_producer);
        //post unlock
        sem_post(mutex);
        //post product count. In this case its seats or space
        sem_post(product_count);
    }

    printf("P%d: exiting\n", *pid);
    return 0;
}

void *consumer(void *arg)
{   
    //id number for consumer
    int *cid = arg;

    while(1){
        sem_wait(product_count);
        sem_wait(mutex);
        if(eventbuf_empty(eventbuffer)){
            sem_post(mutex);
            break;
        }

        int events_per_producer = eventbuf_get(eventbuffer);
        printf("C%d: got event %d\n", *cid, events_per_producer);
        sem_post(mutex);
        sem_post(availble_space);
    }

    printf("C%d: exiting\n", *cid);
    return 0;
}


int main(int argc, char *argv[])
{
   
    if (argc !=5){
        fprintf(stderr, "usage: pcseml, producers, coonsumers, events_per_producer, max_outstanding_events \n");
        exit(1);
    }
    producer_count = atoi(argv[1]);
    consumer_count = atoi(argv[2]);
    events_per_producer = atoi(argv[3]);
    max_outstanding_events = atoi(argv[4]);

    eventbuffer = eventbuf_create();
    product_count = sem_open_temp("number of products", 0);
    mutex = sem_open_temp("mutex", 1);
    availble_space = sem_open_temp("avaible space for products", max_outstanding_events);

     pthread_t *producer_thread = calloc(producer_count, sizeof *producer_thread);
    int *producer_thread_id = calloc(producer_count, sizeof *producer_thread_id);

    for (int i = 0; i < producer_count; i++) {
        producer_thread_id[i] = i;
        pthread_create(producer_thread + i, NULL, producer, producer_thread_id + i);
    }

    pthread_t *consumer_thread = calloc(consumer_count, sizeof *consumer_thread);
    int *consumer_thread_id = calloc(consumer_count, sizeof *consumer_thread_id);
    for (int i = 0; i < consumer_count; i++){
        consumer_thread_id[i] = i;
        pthread_create(consumer_thread + i, NULL, consumer, consumer_thread_id + i);
    }

    for (int i = 0; i < producer_count; i++){
        pthread_join(producer_thread[i], NULL);
    }
    
    for (int i = 0; i < consumer_count; i++){
        sem_post(product_count);
    }
    
    for (int i = 0; i < consumer_count; i++){
        pthread_join(consumer_thread[i], NULL);
    }
    
}

