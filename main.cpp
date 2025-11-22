#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <semaphore>
#include <latch>
#include <atomic>
#include <syncstream>
#include <chrono>

const int NT = 7;
const int COUNT_A = 6;
const int COUNT_B = 9;
const int COUNT_C = 6;
const int COUNT_D = 7;
const int COUNT_E = 9;
const int COUNT_F = 6;
const int COUNT_G = 9;
const int COUNT_H = 6;
const int COUNT_I = 6;
const int COUNT_J = 8;

const int TOTAL_TASKS = COUNT_A + COUNT_B + COUNT_C + COUNT_D + COUNT_E +
COUNT_F + COUNT_G + COUNT_H + COUNT_I + COUNT_J;

struct Task {
    char type;
    int id;
};

std::mutex queue_mutex;
std::queue<Task> task_queue;
std::counting_semaphore<1000> task_sem(0);
std::latch all_done(TOTAL_TASKS);

std::atomic<int> rem_a{ COUNT_A };
std::atomic<int> rem_b{ COUNT_B };
std::atomic<int> rem_c{ COUNT_C };
std::atomic<int> rem_d{ COUNT_D };
std::atomic<int> rem_e{ COUNT_E };
std::atomic<int> rem_f{ COUNT_F };
std::atomic<int> rem_g{ COUNT_G };
std::atomic<int> rem_h{ COUNT_H };
std::atomic<int> rem_i{ COUNT_I };
std::atomic<int> rem_j{ COUNT_J };

std::atomic<int> join_node_ef{ 2 };
std::atomic<int> join_node_gh{ 2 };

std::atomic<bool> running{ true };

void add_tasks(char type, int count) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        for (int i = 1; i <= count; ++i) {
            task_queue.push({ type, i });
        }
    }
    task_sem.release(count);
}

void f(char type, int id) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::osyncstream(std::cout) << "З набору " << type << " виконано дію " << id << ".\n";
}

void check_dependencies_and_add_next(char finished_type) {
    switch (finished_type) {
    case 'a':
        if (rem_a.fetch_sub(1) == 1) {
            add_tasks('b', COUNT_B);
            add_tasks('c', COUNT_C);
            add_tasks('d', COUNT_D);
        }
        break;
    case 'b':
        if (rem_b.fetch_sub(1) == 1) {
            add_tasks('e', COUNT_E);
        }
        break;
    case 'c':
        if (rem_c.fetch_sub(1) == 1) {
            add_tasks('f', COUNT_F);
            add_tasks('g', COUNT_G);
        }
        break;
    case 'd':
        if (rem_d.fetch_sub(1) == 1) {
            add_tasks('h', COUNT_H);
        }
        break;
    case 'e':
        if (rem_e.fetch_sub(1) == 1) {
            if (join_node_ef.fetch_sub(1) == 1) {
                add_tasks('i', COUNT_I);
            }
        }
        break;
    case 'f':
        if (rem_f.fetch_sub(1) == 1) {
            if (join_node_ef.fetch_sub(1) == 1) {
                add_tasks('i', COUNT_I);
            }
        }
        break;
    case 'g':
        if (rem_g.fetch_sub(1) == 1) {
            if (join_node_gh.fetch_sub(1) == 1) {
                add_tasks('j', COUNT_J);
            }
        }
        break;
    case 'h':
        if (rem_h.fetch_sub(1) == 1) {
            if (join_node_gh.fetch_sub(1) == 1) {
                add_tasks('j', COUNT_J);
            }
        }
        break;
    case 'i':
        rem_i.fetch_sub(1);
        break;
    case 'j':
        rem_j.fetch_sub(1);
        break;
    }
}

void worker_thread() {
    while (running) {
        task_sem.acquire();

        if (!running) return;

        Task task;
        bool have_task = false;

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!task_queue.empty()) {
                task = task_queue.front();
                task_queue.pop();
                have_task = true;
            }
        }

        if (have_task) {
            f(task.type, task.id);
            check_dependencies_and_add_next(task.type);
            all_done.count_down();
        }
    }
}

int main() {
    std::cout << "Обчислення розпочато.\n";

    std::vector<std::jthread> threads;
    threads.reserve(NT);
    for (int i = 0; i < NT; ++i) {
        threads.emplace_back(worker_thread);
    }

    add_tasks('a', COUNT_A);

    all_done.wait();

    running = false;
    task_sem.release(NT);

    std::cout << "Обчислення завершено.\n";
    return 0;
}