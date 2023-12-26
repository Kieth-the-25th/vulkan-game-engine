#pragma once

#include <cstdlib>
#include <thread>
#include <vector>
#include <algorithm>
#include <optional>
#include <functional>
#include <atomic>
#include <semaphore>
#include <mutex>
#include <atomic>
#include <random>
#include <iostream>
#include <fstream>

struct Work {
	std::mutex ownership;
	volatile bool completion;
	void* args;
	std::function<void(void* args)> func;

	Work(void* args, std::function<void(void* args)> func);
};

struct ThreadData {
	std::thread thread;

	ThreadData(std::vector<Work*> scheduled);
};

class AsyncObject {
public:
	std::mutex ownership;
	virtual void update() = 0;
};

class Threading
{
	unsigned int increment = 0;
	std::vector<std::thread> workers;
	std::vector<Work*> scheduled;

public:
	Threading();

	void update();
	void addWork(Work* w);
};