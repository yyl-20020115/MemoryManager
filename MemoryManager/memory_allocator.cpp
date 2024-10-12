#include "memory_allocator.h"
#include <mimalloc.h>

#define SMALL_SIZE 128*sizeof(intptr_t)

void* memory_allocator::do_alloc(size_t size)\]	``````````````````````````````````````````1111111111111111111111111111111111111111111111
{
	if (size < SMALL_SIZE) {
		return do_alloc_small(size);
	}
	return nullptr;
}

void memory_allocator::do_free(void* p)
{
	//mi_free(p);
}

void* memory_allocator::do_alloc_small(size_t size)
{
	if (size == 0) {
		size = sizeof(void*);
	}
	return nullptr;
}
