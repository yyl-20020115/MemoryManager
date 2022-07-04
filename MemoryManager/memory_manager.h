#pragma once
#include "memory_allocator.h"
struct memory_block
{
	intptr_t block_size; //including this field
	intptr_t block_length; //real size with alignment at beginning or after resize
	intptr_t ref_count; //indicates how many times this block has been refed.

	intptr_t ref_indices_count; //member count of the follwing array
	intptr_t ref_indices[0];
	//following real memory content aligned to machine word
};

class memory_manager : public memory_allocator
{
public:
	const intptr_t page_size = 4096;
	const intptr_t align_size = sizeof(intptr_t) * 8;
	static intptr_t get_aligned(intptr_t value, intptr_t align);
public:

	memory_manager(void* _base, intptr_t size, intptr_t entry_count = 0);
	~memory_manager();

public:

	bool avail(intptr_t size, intptr_t refs = 0);
	intptr_t allocate(intptr_t size, intptr_t refs = 0);
	memory_block* get_with_lock(intptr_t handle); //needs to call unlock after used
	void* get_pointer_with_lock(intptr_t handle); //needs to call unlock after used
	unsigned char lock(intptr_t handle);
	unsigned char unlock(intptr_t handle);
	intptr_t connect(intptr_t handle, intptr_t target_handle);
	intptr_t disconnect(intptr_t handle, intptr_t target_handle);
	intptr_t remains();
public: 
	//return true if we need to do arrangment
	bool do_collect();

	void do_arrange();

protected:
	void update_maps();
	intptr_t step_collect();
	intptr_t find_and_set_bit(intptr_t* bitmap, intptr_t size, intptr_t start = 0);
	unsigned char get_bit(intptr_t i, intptr_t* bitmap, intptr_t size);
	unsigned char get_and_set_bit(intptr_t i, intptr_t* bitmap, intptr_t size);
	unsigned char get_and_clear_bit(intptr_t i, intptr_t* bitmap, intptr_t size);
protected:
	intptr_t updating;
    void* _base;
	intptr_t size;
	intptr_t total_pages;
	intptr_t page_aligned_size;
	intptr_t total_entry_count;
	intptr_t* allocation_bitmap_ptr;
	intptr_t any_bitmap_size;
	intptr_t* free_bitmap_ptr;
	intptr_t* swap_bitmap_ptr;
	intptr_t* lock_bitmap_ptr;
	intptr_t* snapshot_bitmap_ptr;
	intptr_t* freeing_bitmap_ptr;
	memory_block** dictionary_ptr;
	intptr_t dictonary_size;
	intptr_t total_allocated_count;
	intptr_t total_allocated_size;
	intptr_t total_allocating_start;
};

