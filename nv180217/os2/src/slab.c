#include "slab.h"


void kmem_init(void* space, int block_num) {
	buddyinit(space, block_num);

	void* pom = buddytake(1);

	slab_t* slab = (slab_t*)pom;
	cashe.slabs_free = pom;
	cashe.slabs_full = 0;
	cashe.slabs_partial = 0;

	strcpy(cashe.name, "CashOfCashe");
	cashe.objectSize = sizeof(kmem_cache_t);
	cashe.order = 0;

	cashe.growing = 0;
	cashe.konstruktor = 0;
	cashe.destruktor = 0;
	cashe.error_code = 0;
	cashe.next = 0;

	long int mem = BLOCK_SIZE;
	mem = mem - sizeof(slab_t);
	int n = 0;
	n = ((long int)mem) / (sizeof(unsigned int) + cashe.objectSize);
	mem = mem - n * (sizeof(unsigned int) + cashe.objectSize);

	slab->colouroff = 0;
	slab->freeList = (int*)((char*)pom + sizeof(slab_t));
	slab->inuse = 0;
	slab->myCache = &cashe;
	slab->next = 0;
	slab->prev = 0;
	slab->nextFreeObj = 0;
	slab->objects = (void*)((char*)pom + sizeof(slab_t) + sizeof(unsigned int) * n);

	kmem_cache_t* lista = (kmem_cache_t*)slab->objects;

	for (int i = 0; i < n; i++) {
		*lista[i].name = '\n';
		lista[i].mutex = CreateMutex(0, FALSE, 0);
		slab->freeList[i] = i + 1;
	}

	cashe.objectsInSlab = n;
	cashe.num_active = 0;
	cashe.num_allocations = n;


	cashe.colour_max = mem / CACHE_L1_LINE_SIZE;

	if (cashe.colour_max > 0) cashe.colour_next = 1;
	else cashe.colour_next = 0;

	allcashe = &cashe;
}



kmem_cache_t* kmem_cache_create(const char* name, size_t size, void (*ctor)(void*), void (*dtor)(void*)) {

	WaitForSingleObject(cashe.mutex, INFINITE);

	kmem_cache_t* ret = 0;
	cashe.error_code = 0;

	slab_t* s = cashe.slabs_partial;

	if (s == 0) {
		s = cashe.slabs_free;
	}

	if (s == 0) {
		void* pom = buddytake(1);
		if (pom == 0) {
			cashe.error_code = 1;
			ReleaseMutex(cashe.mutex);
			return 0;
		}
		s = (slab_t*)pom;
		cashe.slabs_partial = s;

		s->colouroff = cashe.colour_max;
		cashe.colour_next = (cashe.colour_next + 1) % (cashe.colour_max + 1);

		s->freeList = (int*)((char*)pom + sizeof(slab_t));
		s->nextFreeObj = 0;
		s->inuse = 0;
		s->next = 0;
		s->prev = 0;
		s->myCache = &cashe;

		s->objects = (void*)((char*)pom + sizeof(slab_t) +
			sizeof(unsigned int) * cashe.objectsInSlab + CACHE_L1_LINE_SIZE * s->colouroff);


		kmem_cache_t* lista = (kmem_cache_t*)s->objects;

		for (int i = 0; i < cashe.objectsInSlab; i++) {
			*lista[i].name = '\n';
			lista[i].mutex = CreateMutex(0, FALSE, 0);
			s->freeList[i] = i + 1;
		}

		cashe.num_allocations += cashe.objectsInSlab;
		cashe.growing = 1;

	}

	kmem_cache_t* list = (kmem_cache_t*)s->objects;
	ret = &list[s->nextFreeObj];
	s->nextFreeObj = s->freeList[s->nextFreeObj];
	s->inuse++;
	cashe.num_active++;

	if (s == cashe.slabs_free) {
		cashe.slabs_free = s->next;
		if (cashe.slabs_free != 0) cashe.slabs_free->prev = 0;

		if (s->inuse != cashe.objectsInSlab) {
			s->next = cashe.slabs_partial;
			if (cashe.slabs_partial != 0) cashe.slabs_partial->prev = s;
			cashe.slabs_partial = s;
		}
		else {
			s->next = cashe.slabs_free;
			if (cashe.slabs_free != 0) cashe.slabs_free->prev = s;
			cashe.slabs_free = s;
		}

	}
	else {

		if (s->inuse == cashe.objectsInSlab) {
			cashe.slabs_partial = s->next;
			if (cashe.slabs_partial != 0) cashe.slabs_partial->next = 0;
			s->next = cashe.slabs_full;
			if (cashe.slabs_full != 0) cashe.slabs_full->prev = s;
			cashe.slabs_full = s;

		}

	}

	long int mem = BLOCK_SIZE;
	int n = 0;
	while ((long int)(mem - sizeof(slab_t) - sizeof(unsigned int) - size) < 0) {
		n++;
		mem = mem << 1;
	}

	strcpy(ret->name, name);

	ret->slabs_free = 0;
	ret->slabs_partial = 0;
	ret->slabs_full = 0;

	ret->growing = 0;
	ret->konstruktor = ctor;
	ret->destruktor = dtor;
	ret->error_code = 0;
	ret->buff = 0;
	ret->order = n;
	ret->objectSize = size;
	ret->next = allcashe;
	allcashe = ret;

	mem = mem - sizeof(slab_t);
	n = 0;

	n = ((long int)(mem) / (sizeof(unsigned int) + size));
	mem = mem - n * (sizeof(unsigned int) + size);

	ret->objectsInSlab = n;
	ret->num_active = 0;
	ret->num_allocations = 0;
	ret->colour_max = mem / CACHE_L1_LINE_SIZE;
	ret->colour_next = 0;

	ReleaseMutex(cashe.mutex);

	return ret;
}

int kmem_cache_shrink(kmem_cache_t* cachep) {

	WaitForSingleObject(cachep->mutex, INFINITE);

	if (cachep->growing == 1) {
		cachep->growing = 0;
		ReleaseMutex(cachep->mutex);
		return 0;
	}
	int ret = 0;
	int n = 1 << cachep->order;
	while (cachep->slabs_free) {///drugacije destruktor?
		slab_t* freeslabs = cachep->slabs_free;
		cachep->slabs_free = freeslabs->next;
		freebuddy(freeslabs, cachep->order);
		ret = ret + n;
		cachep->num_allocations = cachep->num_allocations - cachep->objectsInSlab;
	}
	cachep->growing = 0;
	ReleaseMutex(cachep->mutex);
	return ret;
}

void* kmem_cache_alloc(kmem_cache_t* cachep) {

	WaitForSingleObject(cachep->mutex, INFINITE);

	void* ret = 0;
	cachep->error_code = 0;

	slab_t* s = cachep->slabs_partial;

	if (s == 0) {
		s = cachep->slabs_free;
	}

	if (s == 0) {

		void* pom = buddytake(cachep->order);
		if (pom == 0) {
			cachep->error_code = 1;
			ReleaseMutex(cachep->mutex);
			return 0;
		}
		s = (slab_t*)pom;

		cachep->slabs_partial = s;

		s->colouroff = cachep->colour_max;
		cachep->colour_next = (cachep->colour_next + 1) % (cachep->colour_max + 1);

		s->freeList = (int*)((char*)pom + sizeof(slab_t));
		s->nextFreeObj = 0;
		s->inuse = 0;
		s->next = 0;
		s->prev = 0;
		s->myCache = cachep;

		s->objects = (void*)((char*)pom + sizeof(slab_t) +
			sizeof(unsigned int) * cachep->objectsInSlab + CACHE_L1_LINE_SIZE * s->colouroff);


		kmem_cache_t* lista = (kmem_cache_t*)s->objects;
		void* obj = s->objects;
		for (int i = 0; i < cachep->objectsInSlab; i++) {
			if (cachep->konstruktor != 0) cachep->konstruktor(obj);
			obj = (void*)((char*)obj + cachep->objectSize);
			s->freeList[i] = i + 1;
		}

		cachep->num_allocations += cachep->objectsInSlab;
		cachep->growing = 1;
	}

	ret = (void*)((char*)s->objects + s->nextFreeObj * cachep->objectSize);
	s->nextFreeObj = s->freeList[s->nextFreeObj];
	s->inuse++;
	cachep->num_active++;

	if (s == cachep->slabs_free) {
		cachep->slabs_free = s->next;
		if (cachep->slabs_free != 0) cachep->slabs_free->prev = 0;

		if (s->inuse != cachep->objectsInSlab) {
			s->next = cachep->slabs_partial;
			if (cachep->slabs_partial != 0) cachep->slabs_partial->prev = s;
			cachep->slabs_partial = s;
		}
		else {
			s->next = cachep->slabs_free;
			if (cachep->slabs_free != 0) cachep->slabs_free->prev = s;
			cachep->slabs_free = s;
		}

	}
	else {

		if (s->inuse == cachep->objectsInSlab) {
			cachep->slabs_partial = s->next;
			if (cachep->slabs_partial != 0) cachep->slabs_partial->next = 0;
			s->next = cachep->slabs_full;
			if (cachep->slabs_full != 0) cachep->slabs_full->prev = s;
			cachep->slabs_full = s;

		}
	}

	ReleaseMutex(cachep->mutex);
	return ret;

}

void kmem_cache_free(kmem_cache_t* cachep, void* objp) {

	WaitForSingleObject(cachep->mutex, INFINITE);

	cachep->error_code = 0;
	slab_t* s = cachep->slabs_full;

	int slabsize = BLOCK_SIZE * (1 << cachep->order);
	int in = 1;

	while (s != 0)
	{
		if ((void*)objp > (void*)s && (void*)objp < (void*)((char*)s + slabsize))
			break;
		s = s->next;
	}

	if (s == 0) {
		in = 0;
		s = cachep->slabs_partial;

		while (s != 0)
		{
			if ((void*)objp > (void*)s && (void*)objp < (void*)((char*)s + slabsize))
				break;
			s = s->next;
		}

	}

	if (s == 0) {
		cachep->error_code = 2;
		ReleaseMutex(cachep->mutex);
		return;
	}
	else {
		s->inuse--;
		cachep->num_active--;
		int num = ((char*)objp - (char*)s->objects) / cachep->objectSize;
		if (objp != (void*)((char*)s->objects + num * cachep->objectSize))
		{
			return;
		}
		s->freeList[num] = s->nextFreeObj;
		s->nextFreeObj = num;

		if (cachep->destruktor != 0) {
			cachep->destruktor(objp);
		}


		if (in) {
			slab_t* preth, * sled;
			preth = s->prev;
			sled = s->next;
			s->prev = 0;
			if (preth != 0) preth->next = sled;
			if (sled != 0) sled->prev = preth;
			if (cachep->slabs_full == s) cachep->slabs_full = sled;

			if (s->inuse != 0) {
				s->next = cachep->slabs_partial;
				if (cachep->slabs_partial != 0) cachep->slabs_partial->prev = s;
				cachep->slabs_partial = s;
			}
			else {
				s->next = cachep->slabs_free;
				if (cachep->slabs_free != 0) cachep->slabs_free->prev = s;
				cachep->slabs_free = s;
			}
		}
		else {
			if (s->inuse == 0) {
				slab_t* preth, * sled;
				preth = s->prev;
				sled = s->next;
				s->prev = 0;
				if (preth != 0) preth->next = sled;
				if (sled != 0) sled->prev = preth;
				if (cachep->slabs_partial == s) cachep->slabs_partial = sled;

				s->next = cachep->slabs_free;
				if (cachep->slabs_free != 0) cachep->slabs_free->prev = s;
				cachep->slabs_free = s;

			}
		}

	}

	ReleaseMutex(cachep->mutex);

}

void* kmalloc(size_t size) {

	unsigned stepen = 5;
	unsigned x = 32;
	while (x < size) {
		x = x << 1;
		stepen++;
	}
	char ime[25];
	sprintf(ime, "Vel - %d", x);

	kmem_cache_t* buff = kmem_cache_create(ime, x, 0, 0);
	buff->buff = 1;

	return kmem_cache_alloc(buff);

}

void kfree(const void* objp) {

	WaitForSingleObject(cashe.mutex, INFINITE);
	int found = 1;
	kmem_cache_t* buff = 0;
	kmem_cache_t* tren = allcashe;
	while ((tren != 0) && (found)) {

		if (tren->buff) {
			slab_t* s = tren->slabs_full;
			int slabsize = BLOCK_SIZE * (1 << tren->order);
			while (s != 0) {
				if ((void*)objp > (void*)s && (void*)objp < (void*)((char*)s + slabsize)) {
					found = 0;
					buff = tren;
				}
				s = s->next;
			}
			s = tren->slabs_partial;
			while (s != 0) {
				if ((void*)objp > (void*)s && (void*)objp < (void*)((char*)s + slabsize)) {
					found = 0;
					buff = tren;
				}
				s = s->next;
			}


		}
		tren = tren->next;

	}
	if (buff == 0) {
		ReleaseMutex(cashe.mutex);
		return;
	}
	kmem_cache_free(buff, objp);

	if (buff->slabs_free != 0) kmem_cache_shrink(buff);

	ReleaseMutex(cashe.mutex);

}

void kmem_cache_destroy(kmem_cache_t* cachep) {


	WaitForSingleObject(cashe.mutex, INFINITE);
	WaitForSingleObject(cachep->mutex, INFINITE);

	kmem_cache_t* preth = 0, * tren = allcashe;
	while ((tren != cachep) && (tren != 0)) {
		preth = tren;
		tren = tren->next;
	}

	if (tren == 0) {
		ReleaseMutex(cachep->mutex);
		ReleaseMutex(cashe.mutex);
		return;
	}

	if (preth == 0) allcashe = allcashe->next;
	else preth->next = tren->next;
	tren->next = 0;

	cashe.error_code = 0;
	slab_t* s = cashe.slabs_full;

	int slabsize = BLOCK_SIZE * (1 << cashe.order);
	int in = 1;

	while (s != 0)
	{
		if ((void*)cachep > (void*)s && (void*)cachep < (void*)((char*)s + slabsize))
			break;
		s = s->next;
	}


	if (s == 0) {
		in = 0;
		s = cachep->slabs_partial;

		while (s != 0)
		{
			if ((void*)cachep > (void*)s && (void*)cachep < (void*)((char*)s + slabsize))
				break;
			s = s->next;
		}

	}

	if (s == 0) {
		cachep->error_code = 2;
		ReleaseMutex(cachep->mutex);
		ReleaseMutex(cashe.mutex);
		return;
	}
	else {
		s->inuse--;
		cashe.num_active--;
		int i = cachep - (kmem_cache_t*)s->objects;
		s->freeList[i] = s->nextFreeObj;
		s->nextFreeObj = i;
		*cachep->name = '\0';
		cachep->objectSize = 0;

		slab_t* pom = cachep->slabs_full;
		while (pom != 0) {
			void* ptr = pom;
			pom = pom->next;
			freebuddy(ptr, cachep->order);
		}
		pom = cachep->slabs_partial;
		while (pom != 0) {
			void* ptr = pom;
			pom = pom->next;
			freebuddy(ptr, cachep->order);
		}
		pom = cachep->slabs_free;
		while (pom != 0) {
			void* ptr = pom;
			pom = pom->next;
			freebuddy(ptr, cachep->order);
		}

		if (in) {
			slab_t* preth, * sled;
			preth = s->prev;
			sled = s->next;
			s->prev = 0;
			if (preth != 0) preth->next = sled;
			if (sled != 0) sled->prev = preth;
			if (cashe.slabs_full == s) cashe.slabs_full = sled;

			if (s->inuse != 0) {
				s->next = cashe.slabs_partial;
				if (cashe.slabs_partial != 0) cashe.slabs_partial->prev = s;
				cashe.slabs_partial = s;
			}
			else {
				s->next = cashe.slabs_free;
				if (cashe.slabs_free != 0) cashe.slabs_free->prev = s;
				cashe.slabs_free = s;
			}
		}
		else {
			if (s->inuse == 0) {
				slab_t* preth, * sled;
				preth = s->prev;
				sled = s->next;
				s->prev = 0;
				if (preth != 0) preth->next = sled;
				if (sled != 0) sled->prev = preth;
				if (cashe.slabs_partial == s) cashe.slabs_partial = sled;

				s->next = cachep->slabs_free;
				if (cashe.slabs_free != 0) cashe.slabs_free->prev = s;
				cashe.slabs_free = s;
			}

		}
		if (cashe.slabs_free != 0) {

			s = cashe.slabs_free;
			i = 0;
			while (s != 0) {
				i++;
				s = s->next;
			}

			while (i > 1) {
				i--;
				s = cashe.slabs_free;
				cashe.slabs_free = cashe.slabs_free->next;
				s->next = 0;
				cashe.slabs_free->prev = 0;
				freebuddy(s, cashe.order);
				cashe.num_allocations = cashe.num_allocations - cashe.objectsInSlab;

			}

		}


	}

	ReleaseMutex(cachep->mutex);
	ReleaseMutex(cashe.mutex);


}

void kmem_cache_info(kmem_cache_t* cachep) {

	WaitForSingleObject(cachep->mutex, INFINITE);

	int n = 0;
	slab_t* s = cachep->slabs_free;
	while (s != 0) {
		n++;
		s = s->next;
	}
	s = cachep->slabs_full;
	while (s != 0) {
		n++;
		s = s->next;
	}
	s = cachep->slabs_partial;
	while (s != 0) {
		n++;
		s = s->next;
	}

	unsigned int cashesize = n * (1 << cachep->order);
	double proc;
	if (cachep->num_allocations == 0) proc = 0;
	else proc = 100 * ((double)cachep->num_active / cachep->num_allocations);

	printf("\n");
	printf("Cache info:\n");
	printf("name: %s\n", cachep->name);
	printf("object size: %d\n", cachep->objectSize);
	printf("slab size: %d blocks\n", cashesize);
	printf("number of objects per slab: %d\n", n);
	printf("total number of blocks in cache: %d\n", cachep->objectsInSlab);
	printf("slot usage: %.2f %% \n", proc);


	ReleaseMutex(cachep->mutex);
}

int kmem_cache_error(kmem_cache_t* cachep) {


	WaitForSingleObject(cachep->mutex, INFINITE);

	int e = cachep->error_code;
	switch (e)
	{
	case 0:
		printf("\n No error \n");
		break;
	case 1:
		printf("\n Error no 1: no enough space \n");
		break;
	case 2:
		printf("\n Error no 2: forwarded arg to function doesn't exist in cache\n");
		break;
	default:
		printf("\n Nepoznat error \n");
		break;
	}

	ReleaseMutex(cachep->mutex);

	return e;

}

//ERRORS:
//0: NO ERROR
//1: NO ENOUGH SPACE
//2: FORWARDED ARG TO FUNCTION DOESN'T EXIST IN CACHE