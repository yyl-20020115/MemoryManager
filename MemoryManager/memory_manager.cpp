#include "memory_manager.h"
#include <cstddef>
#include <intrin.h>
#include <cstring>
intptr_t memory_manager::get_aligned(intptr_t value, intptr_t align)
{
	intptr_t parts = value / align;
	if (value % align != 0) {
		parts++;
	}
	return parts * align;
}
memory_manager::memory_manager(void* _base, intptr_t size, intptr_t entry_count)
	:_base(_base)
	, size(size)
	, updating(false)
{
	intptr_t ec = entry_count;
	this->total_pages = size / page_size; //the reminder maybe not usable.
	this->page_aligned_size = total_pages * page_size;

	if (ec == 0) {
		ec = (size >> 4) / sizeof(memory_block);
	}

	this->total_entry_count = ec;
	
	intptr_t start_offset = this->total_entry_count * sizeof(memory_block*) / sizeof(intptr_t) / 8;

	this->allocation_bitmap_ptr = (intptr_t*)_base;
	this->any_bitmap_size = start_offset;

	this->free_bitmap_ptr = (intptr_t*)((char*)_base + start_offset);

	start_offset += this->any_bitmap_size;
	this->swap_bitmap_ptr = (intptr_t*)((char*)_base + start_offset);
	
	start_offset += this->any_bitmap_size;
	this->lock_bitmap_ptr = (intptr_t*)((char*)_base + start_offset);

	start_offset += this->any_bitmap_size;
	this->snapshot_bitmap_ptr = (intptr_t*)((char*)_base + start_offset);

	start_offset += this->any_bitmap_size;

	this->freeing_bitmap_ptr = (intptr_t*)((char*)_base + start_offset);

	start_offset += this->any_bitmap_size;

	this->dictionary_ptr = (memory_block**)((char*)_base + start_offset);
	this->dictonary_size = this->total_entry_count * sizeof(memory_block*);

	start_offset += dictonary_size;

	memset(this->allocation_bitmap_ptr, 0, this->any_bitmap_size);
	memset(this->free_bitmap_ptr, 0, this->any_bitmap_size);
	memset(this->swap_bitmap_ptr, 0, this->any_bitmap_size);
	memset(this->lock_bitmap_ptr, 0, this->any_bitmap_size);
	memset(this->snapshot_bitmap_ptr, 0, this->any_bitmap_size);
	memset(this->freeing_bitmap_ptr, 0, this->any_bitmap_size);
	memset(this->dictionary_ptr, 0, this->dictonary_size);

	this->total_allocated_count = 0;
	this->total_allocated_size = 0;
	this->total_allocating_start = start_offset;
	//index = 0 is never used, then the index == 0 means null object
	this->allocate(0);
}

memory_manager::~memory_manager()
{

}

bool memory_manager::avail(intptr_t size, intptr_t refs)
{
	return this->total_allocating_start
		+ this->any_bitmap_size
		+ this->dictonary_size
		+ sizeof(memory_block)
		+ refs * sizeof(intptr_t)
		+ size <= this->size
		;
}

intptr_t memory_manager::allocate(intptr_t size, intptr_t refs)
{
	intptr_t handle = -1;
	memory_block* block = 0;
	if (this->avail(size, refs))
	{
		handle = this->find_and_set_bit(this->allocation_bitmap_ptr, this->any_bitmap_size);
		if (handle > 0 
			&& 0 == this->get_bit(handle, this->free_bitmap_ptr, this->any_bitmap_size)
			&& 0 == this->get_bit(handle, this->swap_bitmap_ptr, this->any_bitmap_size)) {

			//NOTICE: use allocator
			//block = (memory_block*)((char*)this->_base + this->total_allocating_start);
			block = (memory_block*)do_alloc(size);
			block->ref_indices_count = refs;
			for (int t = 0; t < block->ref_indices_count; t++)
				block->ref_indices[t] = 0;

			block->block_size 
				= sizeof(block->block_size) 
				+ sizeof(block->ref_count)
				+ sizeof(block->ref_indices_count)
				+ block->ref_indices_count * sizeof(intptr_t) + size;
			block->block_length = get_aligned(block->block_size, align_size);
			
			_interlockedadd64(&this->total_allocated_size, block->block_size);
			_interlockedadd64(&this->total_allocating_start, block->block_length);
			_interlockedexchange64((intptr_t*)&this->dictionary_ptr[handle], (intptr_t)block);
		}
	}

	return handle;
}

memory_block* memory_manager::get_with_lock(intptr_t handle)
{
	return handle >= 0 && handle > this->total_allocated_count 
		&& this->lock(handle) == 0
		? this->dictionary_ptr[handle]
		: 0
		;
}

void* memory_manager::get_pointer_with_lock(intptr_t handle)
{
	memory_block* block = this->get_with_lock(handle);
	return block == 0 ? 0 : block + (intptr_t)(sizeof(block)->block_size +
		sizeof(block->ref_count) + sizeof(block->ref_indices_count) +
		(intptr_t)sizeof(block->ref_indices[0]) * sizeof(block->ref_indices_count));
}


unsigned char memory_manager::lock(intptr_t handle)
{
	return this->get_and_set_bit(handle, this->lock_bitmap_ptr, this->any_bitmap_size);
}

unsigned char memory_manager::unlock(intptr_t handle)
{
	return this->get_and_clear_bit(handle, this->lock_bitmap_ptr, this->any_bitmap_size);
}

intptr_t memory_manager::connect(intptr_t handle, intptr_t target_handle)
{
	intptr_t ref_count = ~0;
	if (this->get_bit(handle, this->free_bitmap_ptr, this->any_bitmap_size) > 0
		|| this->get_bit(target_handle, this->free_bitmap_ptr, this->any_bitmap_size) > 0)
		return ref_count;//all handles should not be in free_bitmap
	
	if (this->get_and_set_bit(handle, this->allocation_bitmap_ptr, this->any_bitmap_size) == 0
		|| this->get_bit(target_handle, this->allocation_bitmap_ptr, this->any_bitmap_size) == 0)
		return ref_count;//all handles should be in alloc_bitmap

	if (this->lock(handle) > 0) return ref_count;

	memory_block* block = this->get_with_lock(handle);
	if (block != 0) {
		int i = 0;
		intptr_t r = 0;
		for (i = 0; i < block->ref_indices_count; i++) {
			r = _InterlockedCompareExchange64(block->ref_indices + i, target_handle, 0);
			if (r == 0 || r == target_handle) break;
		}

		memory_block* target_block = this->get_with_lock(target_handle);
		if (target_block != 0) {
			ref_count = _InterlockedIncrement64(&target_block->ref_count) + 1;
		}
	}

	this->unlock(handle);
	return ref_count;
}

intptr_t memory_manager::disconnect(intptr_t handle, intptr_t target_handle)
{
	intptr_t ref_count = ~0;

	if (this->get_bit(handle, this->free_bitmap_ptr, this->any_bitmap_size) > 0
		|| this->get_bit(target_handle, this->free_bitmap_ptr, this->any_bitmap_size) > 0)
		return ref_count;//all handles should not be in free_bitmap

	if (this->get_bit(handle, this->allocation_bitmap_ptr, this->any_bitmap_size) == 0
		|| this->get_bit(target_handle, this->allocation_bitmap_ptr, this->any_bitmap_size) == 0)
		return ref_count;//all handles should be in alloc_bitmap

	if (this->lock(handle) > 0) return ref_count;

	memory_block* block = this->get_with_lock(handle);
	if (block != 0) {
		memory_block* target_block = this->get_with_lock(target_handle);
		if (target_block != 0) {
			intptr_t ref_count = 0;
			if (target_block->ref_count > 0) {
				ref_count = _InterlockedDecrement64(&target_block->ref_count) - 1;
			}
			if (ref_count == 0)
			{
				//send this to free bitmap,but alloc bit is still 1 
				this->get_and_set_bit(target_handle, this->free_bitmap_ptr, this->any_bitmap_size);
			}
			//remove the link
			intptr_t r = 0;
			for (int i = 0; i < block->ref_indices_count; i++) {
				r = _InterlockedCompareExchange64(block->ref_indices + i, 0, target_handle);
				if (r == target_handle) break;
			}
		}
	}

	this->unlock(handle);

	return ref_count;
}

intptr_t memory_manager::remains()
{
	return this->size - this->total_allocating_start;
}

bool memory_manager::do_collect()
{
	if (!this->updating) {
		if (this->step_collect() > 0) {
			this->updating = true;
		}
	}
	else {
		this->update_maps();
		this->updating = false;
		return true;
	}
	return false;
}

void memory_manager::update_maps()
{
	memcpy(this->free_bitmap_ptr, this->swap_bitmap_ptr, this->any_bitmap_size);
	for (intptr_t i = 0; i < this->any_bitmap_size / sizeof(intptr_t); i++) {
		_InterlockedOr64(this->freeing_bitmap_ptr + i, this->swap_bitmap_ptr[i]);
	}
	memset(this->swap_bitmap_ptr, 0, this->any_bitmap_size);
	for (intptr_t i = 0; i < this->any_bitmap_size / sizeof(intptr_t); i++) {
		_InterlockedOr64(this->snapshot_bitmap_ptr + i, this->allocation_bitmap_ptr[i]);
	}
}

intptr_t memory_manager::step_collect()
{
	intptr_t pre_total_allocated_size = this->total_allocated_size;
	_interlockedbittestandset64(&this->updating, 1);
	//we focus on free-bitmap,just for one round
	for (int i = 0; i < this->any_bitmap_size / sizeof(intptr_t) / 8; i++) {
		int t = this->get_bit(i, this->free_bitmap_ptr, this->any_bitmap_size);
		if (t > 0) {
			memory_block* block = this->get_with_lock(t);
			if (block != 0) {
				for (int i = 0; i < block->ref_indices_count; i++) {
					intptr_t ri = block->ref_indices[i];
					if (ri != 0 && ri != i
						//we don't process blocks which are already in free bitmap
						&& 0 < this->get_bit(ri, this->free_bitmap_ptr, this->any_bitmap_size)) {

						memory_block* target = this->get_with_lock(ri);
						if (target != 0) {
							intptr_t ref_count = 0;
							if (target->ref_count > 0) {
								ref_count = _InterlockedDecrement64(&target->ref_count) - 1;
							}
							if (ref_count == 0)
							{
								this->get_and_set_bit(ri, this->swap_bitmap_ptr, this->any_bitmap_size);
							}
						}
						
						_interlockedexchange64((intptr_t*)block->ref_indices + i, 0);
					}
				}
				//we don't free this memory pointer until the arrange stage
				_interlockedadd64(&this->total_allocated_size, -block->block_size);
				_InterlockedDecrement64(&this->total_allocated_count);
				this->get_and_clear_bit(t, this->free_bitmap_ptr, this->any_bitmap_size);
				this->get_and_clear_bit(t, this->allocation_bitmap_ptr, this->any_bitmap_size);

				//NOTICE: this->total_allocating_start needs to be updated
			}
		}
	}
	_interlockedbittestandreset64(&this->updating, 1);
	//get delta size
	return pre_total_allocated_size - this->total_allocated_size;
}


void memory_manager::do_arrange()
{
	//TODO: get the idea of which memory ranges are to be overwriten
	//from which memeory ranges.
	for (intptr_t i = 0; i < this->any_bitmap_size * 8; i++) {
		//we simply skip anything that is locked
		if(1 == this->get_bit(i,this->lock_bitmap_ptr,this->any_bitmap_size)) 
			continue;
		//if freeing == 1, freeing is set to 0, and this is garbage to collect
		if (1 == get_and_clear_bit(i, this->freeing_bitmap_ptr, this->any_bitmap_size)) {
			//this is garbage, free this
			if (1 == get_and_clear_bit(i, this->snapshot_bitmap_ptr, this->any_bitmap_size))
			{
				//it was object to free(garbage) and now gets free
				memory_block* block = this->dictionary_ptr[i];
				if (block != 0) {
					do_free(block);
					this->dictionary_ptr[i] = 0;
				}
			}
			//this garbage is free now
			get_and_clear_bit(i, this->allocation_bitmap_ptr, this->any_bitmap_size);
		}
	}
}

intptr_t memory_manager::find_and_set_bit(intptr_t* bitmap, intptr_t size, intptr_t start)
{
	for (intptr_t i = start; i < size * 8; i+sizeof(intptr_t)*8)
	{
		intptr_t p = i / sizeof(intptr_t);
		intptr_t r = i % sizeof(intptr_t);
		unsigned char b = _interlockedbittestandreset64(bitmap + p, r);
		if (b) return i;
	}
	return ~0;
}

unsigned char memory_manager::get_bit(intptr_t i, intptr_t* bitmap, intptr_t size)
{
	char b = 0;
	if (i >= 0 && i < size * 8) {
		intptr_t p = i / sizeof(intptr_t);
		intptr_t r = i % sizeof(intptr_t);
		return _bittest64(bitmap + p, r);
	}
	return -1;
}
unsigned char memory_manager::get_and_set_bit(intptr_t i, intptr_t* bitmap, intptr_t size)
{
	unsigned char b = 0;
	if (i >= 0 && i < size * 8) {
		intptr_t p = i / sizeof(intptr_t);
		intptr_t r = i % sizeof(intptr_t);
		b = _interlockedbittestandset64(bitmap + i, r);
	}
	return b;
}

unsigned char memory_manager::get_and_clear_bit(intptr_t i, intptr_t* bitmap, intptr_t size)
{
	unsigned char b = 0;
	if (i >= 0 && i < size * 8) {
		intptr_t p = i / sizeof(intptr_t);
		intptr_t r = i % sizeof(intptr_t);
		b = _interlockedbittestandreset64(bitmap + i, r);
	}
	return b;
}
