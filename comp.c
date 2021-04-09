//e2

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "compress.h"
#include "chunk_archive.h"
#include "queue.h"
#include "options.h"
#include <pthread.h>

#define CHUNK_SIZE (1024*1024)
#define QUEUE_SIZE 10

#define COMPRESS 1
#define DECOMPRESS 0

#define FALSE 0
#define TRUE 1

typedef struct {
	int  chunksTotales;
	int *chunksProcesados;
	pthread_mutex_t *mutexChunks;
	queue	in;
	queue	out;
} ArgsCompresor;


// take chunks from queue in, run them through process (compress or decompress), send them to queue out
void worker(queue in, queue out, chunk (*process)(chunk)) {
    chunk ch, res;
    while(q_elements(in)>0) {
        ch = q_remove(in);
        
        res = process(ch);
        free_chunk(ch);
        
        q_insert(out, res);
    }
}

void *hiloCompresor(void *ptr) {
	ArgsCompresor *args = ptr;
    chunk ch,res;
    while(TRUE){
		pthread_mutex_lock(args->mutexChunks);
		if(*args->chunksProcesados == args->chunksTotales){
			pthread_mutex_unlock(args->mutexChunks);
			break;
		}
		(*(args->chunksProcesados))++;
		pthread_mutex_unlock(args->mutexChunks);
		ch = q_remove(args->in);
		res = zcompress(ch);
		free_chunk(ch);
		q_insert(args->out, res);
	}
	
	return NULL;
}



// Compress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the archive file
void comp(struct options opt) {	
    int fd, chunks, i, offset;
    struct stat st;
    char comp_file[256];
    archive ar;
    queue in, out;
    chunk ch;
       
    if((fd=open(opt.file, O_RDONLY))==-1) {
        printf("Cannot open %s\n", opt.file);
        exit(0);
    }
    
    fstat(fd, &st);
    chunks = st.st_size/opt.size+(st.st_size % opt.size ? 1:0);

    if(opt.out_file) {
        strncpy(comp_file,opt.out_file,255);
    } else {
        strncpy(comp_file, opt.file, 255);
        strncat(comp_file, ".ch", 255);
    }

    ar = create_archive_file(comp_file);

    in  = q_create(opt.queue_size);
    out = q_create(opt.queue_size);
	

    int chunksProcesados = 0;
    pthread_mutex_t	 mutexChunksProcesados = PTHREAD_MUTEX_INITIALIZER;
    ArgsCompresor *argsList = malloc(opt.num_threads*sizeof(ArgsCompresor));
    pthread_t *compresorId = malloc(opt.num_threads*sizeof(pthread_t));   
    
    //Se arrancan los hilos
	for(i=0; i<opt.num_threads; i++){
		argsList[i].chunksTotales = chunks;
		argsList[i].chunksProcesados = &chunksProcesados;
		argsList[i].mutexChunks = &mutexChunksProcesados;
		argsList[i].in = in;
		argsList[i].out = out;
		pthread_create(&compresorId[i],NULL,hiloCompresor,&argsList[i]);
	}
	
	
    // read input file and send chunks to the in queue
    for(i=0; i<chunks; i++) {
        ch = alloc_chunk(opt.size);

        offset=lseek(fd, 0, SEEK_CUR);

        ch->size   = read(fd, ch->data, opt.size);
        ch->num    = i;
        ch->offset = offset;
        
        q_insert(in, ch);
    }

    
	//Esperamos a que terminen los hilos
	for(i=0; i<opt.num_threads; i++)
		pthread_join(compresorId[i],NULL);
	

    // send chunks to the output archive file
    for(i=0; i<chunks; i++) {
        ch = q_remove(out);
        
        add_chunk(ar, ch);
        free_chunk(ch);
    }
    
	//Se libera la memoria
    free(argsList);
    free(compresorId);
    close_archive_file(ar);
    close(fd);
    q_destroy(in);
    q_destroy(out);
}


// Decompress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the decompressed file

void decomp(struct options opt) {
    int fd, i;
    char uncomp_file[256];
    archive ar;
    queue in, out;
    chunk ch;
    
    if((ar=open_archive_file(opt.file))==NULL) {
        printf("Cannot open archive file\n");
        exit(0);
    };
    
    if(opt.out_file) {
        strncpy(uncomp_file, opt.out_file, 255);
    } else {
        strncpy(uncomp_file, opt.file, strlen(opt.file) -3);
        uncomp_file[strlen(opt.file)-3] = '\0';
    }

    if((fd=open(uncomp_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))== -1) {
        printf("Cannot create %s: %s\n", uncomp_file, strerror(errno));
        exit(0);
    } 

    in  = q_create(opt.queue_size);
    out = q_create(opt.queue_size);

    // read chunks with compressed data
    for(i=0; i<chunks(ar); i++) {
        ch = get_chunk(ar, i);
        q_insert(in, ch);
    }
    
    // decompress from in to out
    worker(in, out, zdecompress);
    
    // write chunks from output to decompressed file
    for(i=0; i<chunks(ar); i++) {
        ch=q_remove(out);
        lseek(fd, ch->offset, SEEK_SET);
        write(fd, ch->data, ch->size);
        free_chunk(ch);
    }
    
    close_archive_file(ar);    
    close(fd);
    q_destroy(in);
    q_destroy(out);
}


int main(int argc, char *argv[]) {    
    struct options opt;
    
    opt.compress    = COMPRESS;
    opt.num_threads = 3;
    opt.size        = CHUNK_SIZE;
    opt.queue_size  = QUEUE_SIZE;
    opt.out_file    = NULL;

    read_options(argc, argv, &opt);

    if(opt.compress == COMPRESS) comp(opt);
    else decomp(opt);
}
    
