#include <exec/start_detached.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <format>
#include <iostream>

int Adder(int i) {
  std::cout << std::format("Adder: {}", i) << std::endl;
  return i + 10;
}

int Multiplier(int i) {
  std::cout << std::format("Multiplier: {}", i) << std::endl;
  return i * 2;
}

int Divider(int i) {
  std::cout << std::format("Divider: {}", i) << std::endl;
  return i / 3;
}

void HappyPath(const auto &scheduler) {
  auto work = stdexec::starts_on(
      scheduler, stdexec::just(20) | stdexec::then(Adder) |
                     stdexec::then(Multiplier) | stdexec::then(Divider));

  auto result = stdexec::sync_wait(std::move(work)).value();
  std::cout << "Result: " << std::get<int>(result) << std::endl;
}

void ErrorPath(const auto &scheduler) {
  auto errorSpawner = [](int i) -> int {
    throw std::runtime_error{"Some error"};
  };

  auto work = stdexec::starts_on(
      scheduler, stdexec::just(20) | stdexec::then(Adder) |
                     stdexec::then(Multiplier) | stdexec::then(errorSpawner) |
                     stdexec::then(Divider));

  try {
    auto result = stdexec::sync_wait(std::move(work)).value();
    std::cout << "Result: " << std::get<int>(result) << std::endl;
  } catch (const std::exception &ex) {
    std::cout << "Exception caught: " << ex.what() << std::endl;
  }
}

int main() {
  exec::static_thread_pool pool{3};

  auto sched = pool.get_scheduler();

  HappyPath(sched);
  ErrorPath(sched);

  return 0;
}
