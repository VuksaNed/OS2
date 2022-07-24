#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "windows.h"
#include "slab.h"


typedef union Buddydata {
	union Buddydata* sl;
	unsigned char data[BLOCK_SIZE];
} buddydata;


typedef struct HeadersList {
	buddydata* prvi;
	buddydata* posl;
} headerslist;

typedef struct Info {
	headerslist header[32];
	int brojulaza;
	int brojblokova;
	int brojslobodnih;
	buddydata* prvi;

}info;


static info* inform;
static HANDLE mutexbuddy;

void* buddyinit(void* space, int block_number);
void* buddytake(int n);
void freebuddy(void* space, int n);
void printbuddy();