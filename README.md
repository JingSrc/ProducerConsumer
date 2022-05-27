# Producer/Consumer（生产者/消费者）

示例 1 由用户管理生产者和消费者线程

```c++
ProducerConsumer<int> pc(0);
bool running = true;
    
pc.open();
    
auto thd1 = std::thread([&]{
    int i = 0;
	  while (running) {
            pc.push(i++);
            std::cout << i << " push ---- 0 " << std::endl;
            std::this_thread::sleep_for(500ms);
	    }
    });
auto thd2 = std::thread([&]{
    while (pc.is_opened()) {
            std::cout << pc.pop() << " pop ---- 1 " << std::endl;
            std::this_thread::sleep_for(1s);
        }
    });
auto thd3 = std::thread([&] {
        for (const auto i : pc) {
            std::cout << i << " pop ---- 2 " << std::endl;
            std::this_thread::sleep_for(20ms);
        }
    });

running = false;
    
pc.close();
    
thd1.join();
thd2.join();
thd3.join();
```

示例 2 由生产者消费者对象创建线程

```c++
ProducerConsumer_Ex<int> pce(10);
pce.open();

int i = 0, j = 5000;
pce.produce([&i] {
        std::this_thread::sleep_for(500ms);
        return i++;
    });
pce.produce([&j] {
        std::this_thread::sleep_for(400ms);
        return j++;
    });

pce.consume([](const auto& i) {
        std::cout << i << " pop ---- 1 " << std::endl;
        std::this_thread::sleep_for(1s);
    });

pce.consume([](const auto& i) {
        std::cout << i << " pop ---- 2 " << std::endl;
        std::this_thread::sleep_for(800ms);
    });

pce.close();
```
