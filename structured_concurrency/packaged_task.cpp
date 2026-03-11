#include <future>
#include <iostream>
#include <optional>
#include <list>
#include <thread>
#include <vector>

// Bad implementation of a thread pool on purpose
class ThreadPool {
public:
  ThreadPool(const int numberOfThreads) {
    for (int i{}; i < numberOfThreads; ++i) {
      m_threads.emplace_back([this](std::stop_token stopToken) noexcept {
        WorkerThread(stopToken);
      });
    }
  }

  std::future<void> Submit(std::function<void()> task) {
    std::packaged_task<void()> packagedTask(std::move(task));
    std::future<void> future = packagedTask.get_future();
    
    m_tasks.emplace_back(std::move(packagedTask));

    return future;
  }

private:
  void WorkerThread(std::stop_token stopToken) noexcept {
    while (!stopToken.stop_requested()) {
      auto task = GetTask();

      if (!task.has_value()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        continue;
      }

      (*task)();
    }
  }

  std::optional<std::packaged_task<void()>> GetTask() try {
    std::lock_guard lock{m_mtx};

    if (m_tasks.empty()) {
        return std::nullopt;
    }

    auto task = std::move(m_tasks.front());
    m_tasks.pop_front();
    return std::make_optional(std::move(task));
  } catch (...) {
    return std::nullopt;
  }

private:
  std::mutex m_mtx;
  std::list<std::packaged_task<void()>> m_tasks;
  std::vector<std::jthread> m_threads;
};

int main() {
    ThreadPool pool{3};

    static constexpr auto TaskToPrint = [](){
        std::cout << "Printing from thread: " << std::this_thread::get_id() << '\n';
    };

    auto task1 = pool.Submit(TaskToPrint);
    auto task2 = pool.Submit(TaskToPrint);
    auto task3= pool.Submit([](){
        throw std::runtime_error{"Exception in task!"};
    });

    int value{};
    auto task4 = pool.Submit([&value](){
        value = 42;
    });

    task1.get();
    task2.get();

    try {
        task3.get();
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << '\n';
    }

    task4.get();
    std::cout << "Value is: " << value << '\n';

    return 0;
}
