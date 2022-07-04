#include <iostream>
#include <process.h>
#include <signal.h>
#include <vector>
#include <string>
#include <fstream>
#include <windows.h>
#include "memory_manager.h"

HANDLE hArrangeEvent = 0;
HANDLE hQuitingEvent = 0;

DWORD WINAPI do_collect_thread(void* p)
{
	memory_manager* m = (memory_manager*)p;
	while (WAIT_TIMEOUT == ::WaitForSingleObjectEx(hQuitingEvent,25,FALSE)) 
	{
		if (m->do_collect()) {
			::SetEvent(hArrangeEvent);
		}
	}
	return 0;
}
DWORD WINAPI do_arrange_thread(void* p)
{
	memory_manager* m = (memory_manager*)p;
	DWORD ret = 0;
	HANDLE hs[] = { hQuitingEvent, hArrangeEvent };
	while (WAIT_TIMEOUT == (ret = ::WaitForMultipleObjectsEx(2,hs,FALSE,250,FALSE)))
	{
		switch (ret) {
		case WAIT_OBJECT_0 + 0:
			goto _exit_me;
		case WAIT_OBJECT_0 + 1:
			m->do_arrange();
			break;
		}
	}
_exit_me:

	return 0;
}
void __cdecl on_ctrl_c(int s) 
{
	if (s == SIGINT || s == SIGBREAK) {
		::SetEvent(hQuitingEvent);
	}
}
size_t get_random(size_t range) {
	double ratio = ((double)rand() / RAND_MAX);
	return (size_t)(ratio + range);
}
void dump_graph(memory_manager& m, std::vector<intptr_t>& nodes, std::vector<std::pair<intptr_t, intptr_t>>& links)
{

}
void generate_graph(memory_manager& m, size_t node_count,size_t link_count,size_t per_link_count, std::vector<intptr_t>& nodes, std::vector<std::pair<intptr_t, intptr_t>>& links) {
	for (size_t i = 0; i < node_count; i++) {
		intptr_t handle = m.allocate(get_random(1024), per_link_count);//32 connections
		if (handle != 0) {
			nodes.push_back(handle);
		}
	}
	for (size_t i = 0; i < link_count; i++) {
		intptr_t s = get_random(node_count);
		intptr_t t = get_random(node_count);

		m.connect(s, t);
		links.push_back(std::pair<intptr_t, intptr_t>(s, t));
	}
}
int generate_dot(const std::string fn, std::vector<intptr_t>& nodes, std::vector<std::pair<intptr_t, intptr_t>>& links, bool with_image = false) {
	std::ofstream writer(fn);
	if (writer) {

		writer << "digraph G {"<<std::endl;

		for (auto node : nodes) {
			writer <<"\t" << node << ";" <<std::endl;
		}

		for (auto pair : links)
		{
			writer << "\t" << pair.first << " -> " << pair.second << ";" << std::endl;
		}
		writer << "}" << std::endl;
	}
	//dot -Tpng -o hello.png tmp.dot
	if (with_image) {
		std::string output = "dot -Tpng -o " + fn + ".png " + fn;
		return system(output.c_str());
	}
	return 0;
}

int main()
{
	hArrangeEvent = ::CreateEvent(0, FALSE, FALSE, 0);
	hQuitingEvent = ::CreateEvent(0, FALSE, FALSE, 0);

	signal(SIGINT, on_ctrl_c);
	signal(SIGBREAK, on_ctrl_c);
	srand(time(0));

	const intptr_t giga = (1ULL << 30);
	const intptr_t size = 2 * giga;//2G
	void* ptr = malloc(size);
	if (ptr != 0) {
		std::vector<intptr_t> nodes;
		std::vector<std::pair<intptr_t, intptr_t>> links;

		memory_manager m(ptr, size);

		HANDLE hc = ::CreateThread(0, 0, do_collect_thread, &m, 0, 0);
		HANDLE ha = ::CreateThread(0, 0, do_arrange_thread, &m, 0, 0);
		HANDLE hs[] = { hc,ha };

		//generate random objects and connet them
		generate_graph(m, 256, 100, 32, nodes, links);
		generate_dot("init.dot", nodes, links, true);

		for (intptr_t c = 0; c < 10; c++) {
			//disconnect some to test gc
			for (intptr_t i = 0; i < 10; i++) {
				intptr_t s = get_random(links.size());
				//disconnect random link
				m.disconnect(links[s].first, links[s].second);
				links.erase(links.begin() + s);
			}

			std::vector<intptr_t> dump_nodes;
			std::vector<std::pair<intptr_t, intptr_t>> dump_links;
			dump_graph(m, dump_nodes, dump_links);
			generate_dot("current.dot", dump_nodes, dump_links, true);
		}

		::SetEvent(hQuitingEvent);
		::WaitForMultipleObjectsEx(2, hs, TRUE, INFINITE, TRUE);

		CloseHandle(hc);
		CloseHandle(ha);

		free(ptr);
	}

	::CloseHandle(hArrangeEvent);
	::CloseHandle(hQuitingEvent);

	return 0;
}
