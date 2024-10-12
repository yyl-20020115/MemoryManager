#pragma once

typedef void* (*alloc_function_type)(size_t size);
typedef void (*free_function_type)(void* ptr);

class memory_collector
{
protected:
	struct entry {
		int hash = -1;
		int next = -1;
		void* ptr = nullptr;
		size_t ref_count = 0;
		size_t out_count = 0;
		size_t out_length = 0;
		void** out_ptrs = nullptr;
	public:
		void reset();
		size_t add_out_ptr(void* out_ptr);
		size_t find_out_ptr(void* out_ptr) const;
		size_t remove_out_ptr(void* out_ptr);
	};
protected:
	int* buckets = nullptr;
	entry* entries = nullptr;
	size_t buckets_count = 0;
	size_t entries_count = 0;
	size_t count = 0;//当前已使用过的最长数量，值域为entries的长度
	size_t version = 0;//版本号，记录修改次数
	size_t free_list = 0;//空闲链表的第一位，对应entires的下标，0<freeList<count
	size_t free_count = 0;
	alloc_function_type alloc_function = nullptr;
	free_function_type free_function = nullptr;

public:
	memory_collector(int capacity = 256);
	virtual ~memory_collector();
public:

	void set_alloc_function(alloc_function_type alloc_function) { this->alloc_function = alloc_function; }
	alloc_function_type get_alloc_function() const { return this->alloc_function; }

	void set_free_function(free_function_type free_function) { this->free_function = free_function; }
	free_function_type get_free_function() const { return this->free_function; }

public:
	void* do_alloc(size_t size, void* container_ptr = nullptr);

public:
	void* addref(void* ptr, void* container_ptr);
	void  unlink(void* ptr, void* container_ptr);

	void  clear();
	
protected:
	void init(int capacity);
	bool get_entry(int hashCode, entry& entry);
	int get_hash_code(void* ptr);

	void resize(int newSize, bool forceNewHashCodes);
	int find_entry(void* key);
	bool try_get_count(void* ptr, size_t& count);
	bool remove(void* ptr);
	void insert(void* key, void* value, bool add);
};
