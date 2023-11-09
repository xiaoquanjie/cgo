#include <iostream>
#include <mutex>
#include <thread>
#include "cgo.h"

void print_withtime(const char* msg) {
	static std::mutex mu;
	std::unique_lock<std::mutex> lock(mu);
	std::cout << std::chrono::system_clock::now().time_since_epoch().count() << " " << std::this_thread::get_id() << " " << msg << "\n";
}

void cgo_test() {
	Cgo[]() {
		while (true) {
			print_withtime("this is a coroutine");
			CgoWait(1000);
		}
	};
}

int main() {
	cgo_test();

	int i = 0;
	std::cin >> i;
	return 0;
}