#include "memory_collector.h"
#include "hash_helper.h"
#include <memory.h>

void memory_collector::entry::reset()
{
	this->hash = -1;
	this->next = -1;
	this->ptr = nullptr;
	this->ref_count = 0;
	if (this->out_ptrs != nullptr) {
		delete[] this->out_ptrs;
		this->out_ptrs = nullptr;
	}
}
size_t memory_collector::entry::add_out_ptr(void* out_ptr)
{
	if (!this->find_out_ptr(out_ptr)) {
		if (this->out_count == this->out_length) {
			this->out_length <<= 1;
			void** new_out_ptrs = new void* [this->out_length];
			memset(new_out_ptrs, 0, sizeof(void*) * this->out_length);
			if (this->out_ptrs != nullptr) {
				memcpy(new_out_ptrs, this->out_ptrs, sizeof(void*) * this->out_count);
				delete[] this->out_ptrs;
			}
			this->out_ptrs = new_out_ptrs;
		}
		for (size_t i = 0; i < this->out_length; i++) {
			if (this->out_ptrs[i] == nullptr) {
				this->out_ptrs[i] = out_ptr;
				this->out_count++;
				break;
			}
		}
	}

	return this->out_count;
}
size_t memory_collector::entry::find_out_ptr(void* out_ptr) const
{
	for (size_t i = 0; i < this->out_length; i++)
		if (this->out_ptrs[i] == out_ptr)
			return i;
	return ~0ULL;
}
size_t memory_collector::entry::remove_out_ptr(void* out_ptr)
{
	size_t i = find_out_ptr(out_ptr);
	if (i != ~0ULL) {
		this->out_ptrs[i] = nullptr;
		this->out_count--;
	}
	return this->out_count;
}
memory_collector::memory_collector(int capacity)
{
	this->init(capacity);
}
memory_collector::~memory_collector()
{
	if (this->buckets != nullptr) {
		delete[] this->buckets;
		this->buckets = nullptr;
	}
	if (this->entries != nullptr) {
		delete[] this->entries;
		this->entries = nullptr;
	}
}
int memory_collector::get_hash_code(void* ptr) {
	//TODO:
	return 0;
}

void* memory_collector::do_alloc(size_t size, void* container_ptr)
{
	return addref(this->alloc_function(size), container_ptr);
}

void* memory_collector::addref(void* ptr, void* container_ptr) {
	entry entry;
	if (get_entry(get_hash_code(ptr), entry)) {
		//TODO:
	}
	return ptr;
}

void memory_collector::unlink(void* ptr, void* container_ptr) {
	entry entry,c_entry;
	if (get_entry(get_hash_code(ptr), entry)) {
		if (--entry.ref_count == 0) {

		}
	}
	if (container_ptr != nullptr && get_entry(get_hash_code(ptr), c_entry)) {
		c_entry.add_out_ptr(ptr);
	}
}

void memory_collector::clear()
{
	if (this->count > 0) {
		for (int i = 0; i < this->buckets_count; i++) this->buckets[i] = -1;
		for (int i = 0; i < this->entries_count; i++) this->entries[i].reset();
		this->free_list = -1;
		this->count = 0;
		this->free_count = 0;
		this->version++;
	}
}

void memory_collector::init(int capacity)
{
	this->buckets_count = hash_helper::generate_prime_less_than(capacity);
	this->buckets = new int[this->buckets_count];
	for (int i = 0; i < this->buckets_count; i++) buckets[i] = -1;
	this->entries = new entry[this->buckets_count];
	this->free_list = -1;
}

bool memory_collector::get_entry(int hash, entry& entry) {
	//TODO:

	return false;
}

void memory_collector::resize(int size, bool forceNewHashCodes) {
	//TODO:

	int* new_buckets = new int[size];
	memset(new_buckets, 0xff, size * sizeof(int));
	entry* new_entries = new entry[size];
	//TODO:
	//Array.Copy(entries, 0, newEntries, 0, count);
	if (forceNewHashCodes) {
		for (int i = 0; i < count; i++) {
			if (new_entries[i].hash != -1) {
				new_entries[i].hash = get_hash_code(new_entries[i].ptr) & 0x7fffffff;
			}
		}
	}
	//TODO:
	for (int i = 0; i < count; i++) {
		if (new_entries[i].hash >= 0) {
			int bucket = new_entries[i].hash % size;
			new_entries[i].next = new_buckets[bucket];
			new_buckets[bucket] = i;
		}
	}
	delete[] this->buckets;
	delete[] this->entries;

	this->buckets = new_buckets;
	this->entries = new_entries;
}

int memory_collector::find_entry(void* ptr) {
	if (this->buckets != nullptr) {
		int hash = get_hash_code(ptr) & 0x7FFFFFFF;
		for (int i = this->buckets[hash % this->buckets_count]; i >= 0; i = this->entries[i].next)
			if (this->entries[i].hash == hash
				&& (this->entries[i].ptr == ptr)) return i;
	}
	return -1;
}
bool memory_collector::try_get_count(void* ptr, size_t& count) {
	int i = find_entry(ptr);
	if (i >= 0) {
		count = this->entries[i].ref_count;
		return true;
	}
	return false;
}

bool memory_collector::remove(void* ptr) {
	if (buckets != nullptr) {
		int hash = this->get_hash_code(ptr) & 0x7FFFFFFF;
		int bucket = hash % buckets_count;
		int last = -1;
		for (int i = this->buckets[bucket]; i >= 0; last = i, i = this->entries[i].next) {
			if (this->entries[i].hash == hash && this->entries[i].ptr == ptr) {
				if (last < 0) {
					this->buckets[bucket] = this->entries[i].next;
				}
				else {
					this->entries[last].next = this->entries[i].next;
				}
				this->entries[i].reset();
				this->free_list = i;
				this->free_count++;
				this->version++;
				return true;
			}
		}
	}
	return false;
}

void memory_collector::insert(void* ptr, void* value, bool add) {
	if (buckets == nullptr) this->init(0);

	int hash = get_hash_code(ptr) & 0x7FFFFFFF; //�����ϣ�룬�������EntryԪ�����ֵ�͹�ϣ������أ������鳤��/�����޹�
	int target = hash % buckets_count;//�ѹ�ϣ��ӳ�䵽ָ�����ȣ��õ���ֵ����Ϊbuckets������±ֵ꣬��ҲΪ��0��buckets.length��

	int collision = 0;

	//����ͻ/ѭ������/���ȼ���
	for (int i = this->buckets[target]; i >= 0; i = this->entries[i].next) //�����ѭ�����������Ա�����ǰkey(������±�targetBucket)��Ӧ��entry�����ÿһ��Ԫ��
	{
		//�����ǰkey�Ѵ��ڣ�������add�������޸�ֵ
		if (this->entries[i].hash == hash && (this->entries[i].ptr == ptr)) {
			if (add) {
				//ThrowHelper.ThrowArgumentException(ExceptionResource.Argument_AddingDuplicate);
			}
			this->entries[i].ref_count++;
			//TODO:
			this->version++;
			return;
		}

		collision++;//ѭ���������൱�ڵ�ǰkeyָ�������ĳ��ȣ���������ᴥ�������ĵ���
	}

	//�����ǰkey�����ڣ��¼Ӽ�ֵ�ԣ�Entry��
	int index;  //index == entries������±� == buckets����洢��ֵ
	if (this->free_count > 0) {//�����ǰ�����õĽڵ㣬��ʹ�����ýڵ㣨ɾ��Ԫ�ػ���������õ㣬����ɲο�Remove������
		index = this->free_list; //freeList����������ı�ͷ����Ϊ�±�ָ��entries�����ĳһ���ڵ�
		this->free_list = this->entries[index].next;//ȡ����������ı�ͷ
		this->free_count--;//�������ýڵ���
	}
	else {//û�����ýڵ��ˣ�����ĩβ���Ԫ��
		if (this->count == this->entries_count)//���Ȳ��������ݣ������Resize������
		{
			resize(this->count << 1, true);
			target = hash % buckets_count;//���¼����±�
		}
		index = this->count;
		this->count++;
	}
	//�����µ�EntryԪ��
	this->entries[index].hash = hash;
	this->entries[index].next = buckets[target];//��Ԫ�ز�����ͷ��һ��
	this->entries[index].ptr = ptr;
	this->entries[index].ref_count = 1;
	this->buckets[target] = index;//��Ԫ�ز�����ͷ�ڶ���
	this->version++;
}
