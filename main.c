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

struct disk* disk = NULL;
char* algoritmo;
int nframes;
int npages;
char* virtmem = NULL;
char* physmem = NULL;

void algoritmo_rand(struct page_table *pt, int page);

typedef struct{
	int page;
	int bits;	

}entrada_tabla_marcos;

//Creo una lista de las entradas de la tabla de marcos
entrada_tabla_marcos* tabla_marcos;

void page_fault_handler( struct page_table *pt, int page )
{
	//RAND
	if(!strcmp("rand",algoritmo))
	{
		algoritmo_rand(pt, page);
	}

	//FIFO
	else if(!strcmp("fifo",algoritmo))
	{
		//fifo_fault_handler(pt, page);
	}

	
	/*int* frame;
	int* bits;
	printf("tabla de pagina:\n");
	for (int i=0 ; i<10; i++){
		page_table_get_entry(pt,i, &frame, &bits);
		printf("frame=%d bits=%d\n",frame,bits);
	}
	printf("--------------\n");
	printf("page fault on page #%d\n",page);
	page_table_set_entry(pt,page,page,PROT_WRITE);
	//exit(1);*/
}

int main( int argc, char *argv[] )
{
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

	return 0;
}

//Funcion para buscar marco vacio en la tabla de marcos, sino hay retorna -1
int buscar_marco_vacio(){
	int marco_vacio;
	marco_vacio=-1;
	for(int i= 0; i<nframes; i++)
	{
		if(tabla_marcos[i].bits == 0){
			marco_vacio=i;
		}
	}
	return marco_vacio;
}

void eliminar_pagina(struct page_table *pt, int marco)
{
	//set the entry to not be writen; update the frame table to represent that and update the front "pointer"
	page_table_set_entry(pt, tabla_marcos[marco].page, marco, 0);
	tabla_marcos[marco].bits = 0;
	//front=(front+1)%nframes;

}

void algoritmo_rand(struct page_table *pt, int page){

	printf("page fault on page #%d\n",page);
	int frame;
	int bits;
	page_table_get_entry(pt, page, &frame, &bits);

	printf("frame=%d bits=%d\n",frame,bits);

	//Falta de pagina por no tener marco disponible (bits es 0)
	if (bits==0){

		//Primero se busca un marco vacio
		int marco_vacio=buscar_marco_vacio();
		
		//Si es -1, entonces no hay marco vacio y hay que reemplazar
		if(marco_vacio ==-1){

			//como es RAND, eligir un marco victima al azar y remuevo la pagina asociada a ella en la tabla de paginas
			int marco_victima = lrand48() % nframes;

			//si la pagina del marco victima tiene bit de escritura, entonces hay que guardarlo a disco antes de borrarlo
			if(tabla_marcos[marco_victima].bits & PROT_WRITE)
			{
				disk_write(disk, tabla_marcos[marco_victima].page, &physmem[marco_victima *PAGE_SIZE]);
			}

			eliminar_pagina(pt, marco_victima);
		}
		
		
	}

	

	 
}
