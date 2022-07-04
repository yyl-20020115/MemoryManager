#pragma once
#include <cstdint>

class memory_allocator
{
public:

	void* do_alloc(size_t size);
	void do_free(void* p);

protected:

	void* do_alloc_small(size_t size);
};

