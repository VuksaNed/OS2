#pragma once

#pragma warning(disable : 4996)

typedef struct kmem_cache_s kmem_cache_t;
#define BLOCK_SIZE (4096)
#define CACHE_L1_LINE_SIZE (64)

#include "buddy.h"
#include <stdlib.h>
#include <string.h>
#include "windows.h"

typedef struct slab_s {
	unsigned int colouroff;
	void* objects;
	int* freeList;
	int	nextFreeObj;
	unsigned int inuse;
	struct slab_s* next;
	struct slab_s* prev;
	kmem_cache_t* myCache;


}slab_t;

typedef struct kmem_cache_s {
	slab_t* slabs_full;
	slab_t* slabs_partial;
	slab_t* slabs_free;
	char name[25];
	unsigned int objectSize;
	unsigned int objectsInSlab;
	unsigned long num_active;
	unsigned long num_allocations;
	int buff;
	unsigned int order;
	unsigned int colour_max;
	unsigned int colour_next;
	int growing;
	void (*konstruktor)(void*);
	void (*destruktor)(void*);
	int	error_code;
	struct kmem_cache_s* next;
	HANDLE mutex;
};

static kmem_cache_t cashe;
static kmem_cache_t* allcashe;


void kmem_init(void* space, int block_num);

kmem_cache_t* kmem_cache_create(const char* name,
	size_t size, void (*ctor)(void*), void (*dtor)(void*)); // Allocate cache

int kmem_cache_shrink(kmem_cache_t* cachep); // Shrink cache

void* kmem_cache_alloc(kmem_cache_t* cachep); // Allocate one object from cache
void kmem_cache_free(kmem_cache_t* cachep, void* objp); // Deallocate one object from cache

void* kmalloc(size_t size); // Alloacate one small memory buffer
void kfree(const void* objp); // Deallocate one small memory buffer

void kmem_cache_destroy(kmem_cache_t* cachep); // Deallocate cache

void kmem_cache_info(kmem_cache_t* cachep); // Print cache info
int kmem_cache_error(kmem_cache_t* cachep); // Print error message


////////
