#include <future>
#include <iostream>
#include <thread>

void WorkerFunction(std::promise<int> promise, int n) {
  if (n == 228) {
    promise.set_exception(
        std::make_exception_ptr(std::runtime_error{"Invalid value"}));
  } else {
    promise.set_value(n * 2);
  }
}

void RunWorker(int n) {
  std::promise<int> promise;
  auto future = promise.get_future();
  //std::shared_future<int> future{promise.get_future()};

  std::jthread worker(WorkerFunction, std::move(promise), n);

  try {
    std::cout << "Result: " << future.get() << "\n";
    std::cout << "Another try: " << future.get() << "\n";
  } catch (const std::exception &e) {
    std::cout << "Exception caught: " << e.what() << "\n";
  }
}

int main() {
  RunWorker(10);
  RunWorker(228);
  return 0;
}
