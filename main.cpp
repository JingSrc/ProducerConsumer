#include "ProducerConsumer.h"

#include <thread>
#include <iostream>
#include <chrono>

using namespace std::chrono_literals;

int main()
{
    ProducerConsumer<int> pc;
    bool running = true;

    pc.open();

    auto thd1 = std::thread([&]
    {
        int i = 0;
	      while (running) {
            pc.push(i++);
            std::this_thread::sleep_for(300ms);
	      }
    });
    auto thd2 = std::thread([&]{
        while (running) {
            std::cout << pc.pop() << " pop ---- 1 " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(1s);
        }
    });
    auto thd3 = std::thread([&] {
        for (const auto i : pc) {
            std::cout << i << " pop ---- 2 " << std::endl;
            std::this_thread::sleep_for(1s);
        }
    });

    getchar();
    running = false;

    pc.close();

    thd1.join();
    thd2.join();
    thd3.join();

    std::cout << "end" << std::endl;
}
