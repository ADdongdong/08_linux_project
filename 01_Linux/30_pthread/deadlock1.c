#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

//创建两个互斥量
pthread_mutex_t mutex1, mutex2;

void *workA(void *arg){
    pthread_mutex_lock(&mutex1);
    sleep(1);
    pthread_mutex_lock(&mutex2);

    printf("workA...\n");

    
    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);
    return NULL;
}

void *workB(void *arg){
    pthread_mutex_lock(&mutex2);
    pthread_mutex_lock(&mutex1);

    printf("workB...\n");

    
    pthread_mutex_unlock(&mutex1);
    pthread_mutex_unlock(&mutex2);
 
    return NULL;
}


int main(int argc, char const *argv[])
{
    //初始化两个互斥量
    pthread_mutex_init(&mutex1, NULL);
    pthread_mutex_init(&mutex2, NULL);

    //创建两个子线程
    pthread_t tid1, tid2;
    pthread_create(&tid1, NULL, workA, NULL);
    pthread_create(&tid2, NULL, workB, NULL);

    //回收线程资源
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    //释放互斥量资源
    pthread_mutex_destroy(&mutex1);
    pthread_mutex_destroy(&mutex2);

    /* code */
    return 0;
}
