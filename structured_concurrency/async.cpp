#include <future>
#include <iostream>

int WorkerFunction(int n) {
  if (n == 228) {
    throw std::runtime_error{"Invalid value"};
  }

  return n * 2;
}

void RunWorker(int n) try {
  auto result = std::async(std::launch::async, WorkerFunction, n);
  std::cout << "Result: " << result.get() << "\n";
} catch (const std::exception &e) {
  std::cout << "Exception caught: " << e.what() << "\n";
}

int main() {
  RunWorker(10);
  RunWorker(228);
  return 0;
}
