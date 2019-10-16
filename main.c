/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

struct disk* disk = NULL;
char* algoritmo;
int nframes;
int npages;
char* virtmem = NULL;
char* physmem = NULL;
int cant_lecturas_disco=0;
int cant_escrituras_disco=0;
int cant_faltas=0;

//Variables para el algoritmo fifo(array(queue),inicio del array,final del array)
int*fifo_queue;
int inicio_queue = 0;
int final_queue = 0;

void algoritmo_rand(struct page_table *pt, int page);
void algoritmo_fifo(struct page_table *pt, int page);

typedef struct{
	int page;
	int bits;	

}entrada_tabla_marcos;

//Creo una lista de las entradas de la tabla de marcos
entrada_tabla_marcos* tabla_marcos;

void page_fault_handler( struct page_table *pt, int page )
{
	cant_faltas++;
	//RAND
	if(!strcmp("rand",algoritmo))
	{
		algoritmo_rand(pt, page);
	}

	//FIFO
	else if(!strcmp("fifo",algoritmo))
	{
		algoritmo_fifo(pt, page);
	}

	
	int frame;
	int bits;
	printf("tabla de pagina:\n");
	for (int i=0 ; i<npages; i++){
		page_table_get_entry(pt,i, &frame, &bits);
		printf("frame=%d bits=%d\n",frame,bits);
	}
	printf("--------------\n");
}

int main( int argc, char *argv[] )
{
	srand48 (time(0));
	
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <lru|fifo> <access pattern>\n");
		return 1;
	}

	npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	algoritmo = argv[3];
	const char *program = argv[4];

	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	//Le asigno el tamaño a la tabla de marcos
	tabla_marcos= malloc(nframes * sizeof(entrada_tabla_marcos));
	//Se crea un array del tamaño de la cantidad de marcos existentes para guardarlos y simular un queue
	fifo_queue = malloc (nframes * sizeof(int));

	virtmem = page_table_get_virtmem(pt);

	physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"pattern1")) {
		access_pattern1(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"pattern2")) {
		access_pattern2(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"pattern3")) {
		access_pattern3(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);

	}

	page_table_delete(pt);
	disk_close(disk);
	free(tabla_marcos);
	free(fifo_queue);

	printf("Algortimo utilizado: %s\n", argv[3]);
	printf("Cantidad de paginas: %d\n",npages);
	printf("Cantidad de marcos: %d\n",nframes);
	
	printf("\n-----------Resumen----------\n");
	printf("Cantidad de faltas de pagina: %d\n",cant_faltas);
	printf("Cantidad de lecturas en disco: %d\n",cant_lecturas_disco);
	printf("Cantidad de escrituras a disco: %d\n",cant_escrituras_disco);
	return 0;
}

void algoritmo_rand(struct page_table *pt, int page){

	//printf("page fault on page #%d\n",page);
	int frame;
	int bits;
	page_table_get_entry(pt, page, &frame, &bits);
	
	int marco_vacio;
	//printf("frame=%d bits=%d\n",frame,bits);

	//Falta de pagina por no tener marco disponible (bits es 0)
	if (bits==0){

		//Se busca un marco vacio
		marco_vacio=-1;
		for(int i= 0; i<nframes; i++)
		{
			if(tabla_marcos[i].bits == 0){
				marco_vacio=i; //Si hay un marco disponible, se le asigna a marco_vacio
				break;
			}
		}
		//SE cambia el bit de 0 a Read
		bits = PROT_READ;

		//Si es -1, entonces no hay marco vacio y hay que reemplazar
		if(marco_vacio ==-1){

			//como es RAND, eligir un marco victima al azar y remuevo la pagina asociada a ella en la tabla de paginas
			marco_vacio = (int)lrand48() % nframes;

			//si la pagina del marco victima tiene bit de escritura, entonces hay que guardarlo a disco antes de borrarlo
			if(tabla_marcos[marco_vacio].bits & PROT_WRITE)
			{
				disk_write(disk, tabla_marcos[marco_vacio].page, &physmem[marco_vacio *PAGE_SIZE]);
				cant_escrituras_disco++;
			}
			//Se elimina la pagina
			page_table_set_entry(pt, tabla_marcos[marco_vacio].page, marco_vacio, 0);
			tabla_marcos[marco_vacio].bits = 0;
		}
		//Swap del disco al marco
		disk_read(disk, page, &physmem[marco_vacio*PAGE_SIZE]);
		cant_lecturas_disco++;
	}
	//si los bits son de lectura, cambiar a escritura/lectura
	else if (bits & PROT_READ){
		bits = PROT_READ | PROT_WRITE;
		marco_vacio = frame;
	}
	//Actualizar la tabla de pagina
	page_table_set_entry(pt, page, marco_vacio, bits);

	//actualizar la tabla de marcos
	tabla_marcos[marco_vacio].bits = bits; 
	tabla_marcos[marco_vacio].page = page;
}

void algoritmo_fifo(struct page_table *pt, int page)
{
	int frame;//Marco 
	int bits;

	page_table_get_entry(pt, page, &frame, &bits);

	int marco_vacio;
	
	//Si el bit es 0, es porque todavia no está asignado a una pagina
	if(bits == 0)
	{ 
	
		//Se busca un marco vacio
		marco_vacio=-1;
		for(int i= 0; i<nframes; i++)
		{
			if(tabla_marcos[i].bits == 0){
				marco_vacio=i; //Si hay un marco disponible, se le asigna a marco_vacio
				break;
			}
		}

		//Se cambia el bit 0 a un bit de escritura
		bits = PROT_READ;
		

		//Si marco_vacio = -1 no existe un marco vacio, por lo tanto se debe liberar el primer marco dentro de la "queue" de marcos
		if(marco_vacio == -1)
		{
			//Se toma el primer marco de la queue de marcos, y se libera con eliminar pagina
			marco_vacio = fifo_queue[inicio_queue];
			//Se escribe en el disco
			disk_write(disk, tabla_marcos[marco_vacio].page, &physmem[marco_vacio *PAGE_SIZE]);
			cant_escrituras_disco++;
			//Se elimina la pagina
			page_table_set_entry(pt, tabla_marcos[marco_vacio].page, marco_vacio, 0);
			tabla_marcos[marco_vacio].bits = 0;
			//Se cambia el valor del inicio, por el siguiente dentro del array
			inicio_queue=(inicio_queue+1)%nframes;
		}
		disk_read(disk, page, &physmem[marco_vacio*PAGE_SIZE]);
		cant_lecturas_disco++;
		//Se asigna el nuevo marco, al final de la queue y se le suma 1 a la variable final_queue
		fifo_queue[final_queue]=marco_vacio;
		final_queue =(final_queue +1) % nframes;

	}
	//si los bits son de lectura, cambiar a escritura/lectura
	else if(bits & PROT_READ)
	{
		
		bits = PROT_READ | PROT_WRITE;
		marco_vacio = frame;
	} 
	//Se actualiza la tabla de pagina
	page_table_set_entry(pt, page, marco_vacio, bits);
	//Se actualiza la tabla de marcos
	tabla_marcos[marco_vacio].page = page;
	tabla_marcos[marco_vacio].bits = bits;
}
