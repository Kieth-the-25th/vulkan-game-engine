#include "threading.h"

void workerMain(std::vector<Work*>* workList) {
	int log = 0;
	while (true)
	{
		if (workList->size() > 0) {
			log++;
			if (log > 10000000) {
				//std::cout << "size: " <<  workList->size() << "\n";
				//std::cout << "ping " << std::this_thread::get_id() << "\n";
				log = 0;
			}

			for (size_t i = 0; i < workList->size(); i++) {
				if (!workList->at(i)->completion) {
					std::cout << "found work";
					if (workList->at(i)->ownership.try_lock()) {
						std::cout << "got work\n";
						workList->at(i)->func(workList->at(i)->args);
						workList->at(i)->completion = true;
						workList->at(i)->ownership.unlock();
						std::cout << "operation complete\n";
					}
				}
			}
		}
		else {
			std::this_thread::yield();
		}
	}
}

Work::Work(void* args, std::function<void(void* args)> func) {
	this->args = args;
	this->func = func;
	this->completion = false;
}

ThreadData::ThreadData(std::vector<Work*> scheduled) {
	//thread = std::thread(workerMain, std::ref(scheduled));
	//thread.detach();
}

Threading::Threading() {
	int maxConcurrent = std::thread::hardware_concurrency();
	std::cout << "Max concurrent threads: " << maxConcurrent << "\n";
	for (int i = 0; i < 2; i++) {
		workers.push_back(std::thread(&workerMain, &scheduled));
		workers[i].detach();
	}
	//std::thread(workerMain, nullptr, &(workers[0].signal), &(workers[0].activate));
}

void Threading::update() {
	for (int i = 0; i < scheduled.size(); i++) {
		if (scheduled[i]->completion) {
			scheduled.erase(scheduled.begin() + i);
		}
	}
}

void Threading::addWork(Work* w) {
	scheduled.push_back(w);
}