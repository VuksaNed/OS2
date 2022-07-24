#include "buddy.h"

void* buddyinit(void* space, int block_number) {
	int brojblokova = block_number;

	inform = (info*)space;

	int stepen = 1;
	int x = 0;
	while (stepen < brojblokova) {
		x++;
		stepen = stepen << 1;
	}

	inform->brojblokova = stepen;
	inform->brojslobodnih = stepen;
	inform->prvi = (buddydata*)space + 1;
	inform->brojulaza = x;

	for (int i = 0; i < x; i++) {
		inform->header[i].prvi = inform->header[i].posl = 0;
	}


	inform->header[x - 1].prvi = inform->prvi;
	inform->header[x - 1].posl = inform->prvi;
	inform->prvi->sl = 0;

	mutexbuddy = CreateMutex(0, FALSE, 0);

	return inform;
}

void* buddytake(int n) {

	WaitForSingleObject(mutexbuddy, INFINITE);

	int x = n;
	n = 1 << n;

	if (inform->brojslobodnih < n) {

		ReleaseMutex(mutexbuddy);
		///ne moze da alocira dovoljno
		return 0;
	}

	int i;
	for (i = x; i <= inform->brojulaza; i++) {
		if (inform->header[i].prvi) {
			break;////
		}
	}

	if (i > inform->brojulaza) {
		//ne moze da alocira doovljno
		ReleaseMutex(mutexbuddy);
		return 0;
	}

	buddydata* b1 = inform->header[i].prvi;
	inform->header[i].prvi = inform->header[i].prvi->sl;
	if (inform->header[i].posl == b1) inform->header[i].posl = 0;

	if (i > x) {
		int y = i - x;
		int pomeraj = 1 << x;
		for (int j = 0; j < y; j++) {
			buddydata* b2 = b1 + pomeraj;
			b2->sl = inform->header[x + j].prvi;
			inform->header[x + j].prvi = b2;
			pomeraj = pomeraj << 1;

		}
	}
	inform->brojslobodnih -= n;
	ReleaseMutex(mutexbuddy);
	return b1;

}

void freebuddy(void* space, int n) {

	WaitForSingleObject(mutexbuddy, INFINITE);

	int x = n;
	n = 1 << x;

	buddydata* b = (buddydata*)space;
	if (inform->header[x].prvi == 0) {
		inform->header[x].prvi = inform->header[x].posl = b;
		b->sl = 0;
		inform->brojslobodnih += n;
	}
	else {
		int pomeraj = 1 << x;
		buddydata* pom = inform->header[x].prvi;
		buddydata* preth = 0;
		int i = x;
		while (pom != 0) {
			if (((b + pomeraj) == pom) ||
				(b == (pom + pomeraj))) {
				if (inform->header[i].prvi == pom)inform->header[i].prvi = inform->header[i].prvi->sl;
				else {
					preth->sl = pom->sl;
					//	inform->header[i].prvi = inform->header[i].posl = 0;
				}
				if (b > pom) b = pom;
				i++;
				pom = inform->header[i].prvi;
				preth = 0;
				pomeraj = pomeraj << 1;
			}
			else {
				preth = pom;
				pom = pom->sl;
			}
		}

		b->sl = inform->header[i].prvi;
		inform->header[i].prvi = b;

	}

	ReleaseMutex(mutexbuddy);

}

void printbuddy() {

	WaitForSingleObject(mutexbuddy, INFINITE);
	for (int i = 0; i < inform->brojulaza; i++) {
		printf("U %d. grani ima: ", i);
		buddydata* pom = inform->header[i].prvi;
		while (pom != 0) {
			printf(" %d ", pom);
			pom = pom->sl;
		}
		printf("\n");
	}
	printf("\n");

	ReleaseMutex(mutexbuddy);
}