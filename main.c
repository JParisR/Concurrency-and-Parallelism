//main e1

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "queue.h"

#define MAX_PRODUCTORES 3
#define MAX_CONSUMIDORES 2
#define TAM_COLA 4
#define MAX_ELEMENTOS 10

struct ProductorArgs {
	int id;
	int *elementosCreados;
	pthread_mutex_t *mutexProductor;
	queue *cola;
};

struct ConsumidorArgs {
	int id;
	int *elementosBorrados;
	pthread_mutex_t *mutexConsumidor;
	queue *cola;
};

void *productor(void *ptr){

	struct ProductorArgs *args = ptr;
	int *elemento;
	int contadorCreados = 0;
	
	while(1) {		
		pthread_mutex_lock(args->mutexProductor);
		if (*(args->elementosCreados) < MAX_ELEMENTOS){
			elemento = malloc(sizeof(elemento));
			*elemento = *args->elementosCreados;
			(*(args->elementosCreados))++;
			pthread_mutex_unlock(args->mutexProductor);
		}else{
			pthread_mutex_unlock(args->mutexProductor);
			break;
		}
		
		printf("El productor %d añade el elemento %i\n", args->id, *elemento);
		contadorCreados++;
		usleep(rand() % 800);
		q_insert(*args->cola,elemento);

	}
	printf("El productor %d ha añadido %d elementos\n", args->id, contadorCreados);
	return NULL;
}

void *consumidor(void *ptr){
	
	struct ConsumidorArgs *args = ptr;
	int *elemento;
	int contadorBorrados = 0;

	while(1){
		pthread_mutex_lock(args->mutexConsumidor);
		if (*(args->elementosBorrados) < MAX_ELEMENTOS){
			(*(args->elementosBorrados))++;
			pthread_mutex_unlock(args->mutexConsumidor);
		}else{
			pthread_mutex_unlock(args->mutexConsumidor);
			break;
		}
		elemento = q_remove(*args->cola);
		printf("El consumidor %d elimina el elemento %i\n", args->id, *elemento);
		contadorBorrados++;
		usleep(rand() % 800);
	}
	printf("El consumidor %d ha eliminado %i elementos\n", args->id, contadorBorrados);
	return NULL;
}

int main (int argc, char **argv)
{
	int i  = 0;
	int elementosCreados = 0;
	int elementosBorrados = 0;
	struct ProductorArgs  productoresArgs[MAX_PRODUCTORES];
	struct ConsumidorArgs consumidoresArgs[MAX_CONSUMIDORES];
	
	pthread_mutex_t mutexProductor = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mutexConsumidor = PTHREAD_MUTEX_INITIALIZER;	
	pthread_t productores[MAX_PRODUCTORES];
	pthread_t consumidores[MAX_CONSUMIDORES];

	
	//Creamos la cola
	queue cola = q_create(TAM_COLA);
	
	//Creamos los productores
	for(i=0; i<MAX_PRODUCTORES; i++){
		productoresArgs[i].id = i;
		productoresArgs[i].elementosCreados = &elementosCreados;
		productoresArgs[i].mutexProductor = &mutexProductor;
		productoresArgs[i].cola = &cola;
		pthread_create(&productores[i],NULL,productor,&productoresArgs[i]);
	}
	
	//Creamos los consumidores
	for(i=0; i<MAX_CONSUMIDORES; i++){
		consumidoresArgs[i].id = i;
		consumidoresArgs[i].elementosBorrados = &elementosBorrados;
		consumidoresArgs[i].mutexConsumidor = &mutexConsumidor;
		consumidoresArgs[i].cola = &cola;
		pthread_create(&consumidores[i],NULL,consumidor,&consumidoresArgs[i]);
	}
	
	//Esperamos a que los productores finalicen
	for(i = 0; i < MAX_PRODUCTORES; i++){
		pthread_join(productores[i],NULL);
		printf("Finalizo el productor %d\n", i);
	}
	
	//Esperamos a que los consumidores finalicen
	for(i = 0; i < MAX_CONSUMIDORES; i++){
		pthread_join(consumidores[i],NULL);
		printf("Finalizo el consumidor %d\n", i);
	}
	
	q_destroy(cola);
	pthread_exit (0);
}
