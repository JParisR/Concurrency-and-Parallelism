#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "options.h"

struct buffer {
	int *data;
	int size;
};

struct thread_info {
	pthread_t       thread_id;        // id returned by pthread_create()
	int             thread_num;       // application defined thread #
};

struct args {
	int 		thread_num;       // application defined thread #
	int 	        delay;			  // delay between operations
	int		*iterations;
	struct buffer   *buffer;		  // Shared buffer
	pthread_mutex_t *swap_mutex;           // mutex
	pthread_mutex_t *mutex_iter;		
};

void *swap(void *ptr)
{
	struct args *args =  ptr;
	int i,j, tmp;
	
	while(1){
		pthread_mutex_lock(args->mutex_iter); //Bloqueamos mutex.
		
		if(!(*args->iterations)){ //Comprobamos el numero de iteraciones realizadas.
			pthread_mutex_unlock(args->mutex_iter);
			return NULL;
		}
		
		(*args->iterations)--; //Restamos una iteración y desbloqueamos.
		pthread_mutex_unlock(args->mutex_iter);
		
		//Si la posición es la misma, no se hace intercambio.
		do{
			i=rand() % args->buffer->size;
			j=rand() % args->buffer->size;		
		}while(i == j);
		
		while(1){
			pthread_mutex_lock(&args->swap_mutex[i]); //Bloqueamos el primer elemento.
			if(pthread_mutex_trylock(&args->swap_mutex[j])) { //Si el segundo ya esta bloqueado, liberamos el primero.
				pthread_mutex_unlock(&args->swap_mutex[i]);			
				continue;	
			} 
			else {
				printf("Thread %d swapping positions %d (== %d) and %d (== %d)\n", 
					args->thread_num, i, args->buffer->data[i], j, args->buffer->data[j]);
					
				tmp = args->buffer->data[i];
				if(args->delay) usleep(args->delay); // Force a context switch

				args->buffer->data[i] = args->buffer->data[j];
				if(args->delay) usleep(args->delay);
			
				args->buffer->data[j] = tmp;
				if(args->delay) usleep(args->delay);
			
				pthread_mutex_unlock(&args->swap_mutex[i]); //Se desbloquean ambos mutex.
				pthread_mutex_unlock(&args->swap_mutex[j]);
				break;
			}
		}

}
	return NULL;
}

void print_buffer(struct buffer buffer) {
	int i;
	
	for (i = 0; i < buffer.size; i++)
		printf("%i ", buffer.data[i]);
	printf("\n");
}

void start_threads(struct options opt)
{
	int i, j;
	struct thread_info *threads;
	struct args *args;
	struct buffer buffer;
	
	int iterations = opt.iterations; //Valor recibido de iteraciones a realizar.
	
	srand(time(NULL));

	if((buffer.data=malloc(opt.buffer_size*sizeof(int)))==NULL) {
		printf("Out of memory\n");
		exit(1);
	}
	buffer.size = opt.buffer_size;
	
	//Creamos el mutex general.	
	pthread_mutex_t *mutex_array;
	if((mutex_array = malloc(sizeof(pthread_mutex_t)*buffer.size)) == NULL){
		printf("No ha sido posible crear el mutex.n");
		exit(1);
	}
	
	//Creamos el mutex para el nº de iteraciones.
	pthread_mutex_t *m_iter;
	if((m_iter = malloc(sizeof(pthread_mutex_t))) == NULL){
		printf("No ha sido posible crear el mutex para las iteraciones.n");
		exit(1);
	}
	
	//Iniciamos los mutex.
	for(i=0; i<buffer.size; i++) {
        pthread_mutex_init(&mutex_array[i], NULL);
	}
	
	pthread_mutex_init(m_iter, NULL);
	
	for(i=0; i<buffer.size; i++)
		buffer.data[i]=i;

	printf("creating %d threads\n", opt.num_threads);
	threads = malloc(sizeof(struct thread_info) * opt.num_threads);
	args = malloc(sizeof(struct args) * opt.num_threads);

	if (threads == NULL || args==NULL) {
		printf("Not enough memory\n");
		exit(1);
	}

	printf("Buffer before: ");
	print_buffer(buffer);


	// Create num_thread threads running swap() 
	for (i = 0; i < opt.num_threads; i++) {
		threads[i].thread_num = i;
		
		args[i].thread_num = i;
		args[i].buffer     = &buffer;
		args[i].delay      = opt.delay;
		args[i].iterations = &iterations;
		args[i].swap_mutex = mutex_array; //Se comparten los mutex a los threads.
		args[i].mutex_iter = m_iter;
		
		if ( 0 != pthread_create(&threads[i].thread_id, NULL,
					 swap, &args[i])) {
			printf("Could not create thread #%d", i);
			exit(1);
		}
	}
	
	// Wait for the threads to finish
	for (i = 0; i < opt.num_threads; i++)
		pthread_join(threads[i].thread_id, NULL);
	
	//Se borran los mutex.	
	for (j = 0; j < buffer.size; j++){
		pthread_mutex_destroy(&mutex_array[j]);
	}
	
	pthread_mutex_destroy(m_iter);	

	// Print the buffer
	printf("Buffer after:  ");
	print_buffer(buffer);
		
	free(args);
	free(threads);
	free(buffer.data);
	free(mutex_array); //Se libera el espacio de los mutex.
	free(m_iter);
        
	pthread_exit(NULL);
}

int main (int argc, char **argv)
{
	struct options opt;
	
	// Default values for the options
	opt.num_threads = 10;
	opt.buffer_size = 10;
	opt.iterations  = 100;
	opt.delay       = 10;
	
	read_options(argc, argv, &opt);

	start_threads(opt);

	exit (0);
}
