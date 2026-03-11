#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <iostream>

int main() {
  exec::static_thread_pool pool{3};

  auto sched = pool.get_scheduler();

  auto fun = [](int i) {
    std::cout << "Executing task for " << i << std::endl;
    return i * i;
  };
  auto work = stdexec::when_all(
      stdexec::starts_on(sched, stdexec::just(0) | stdexec::then(fun)),
      stdexec::starts_on(sched, stdexec::just(1) | stdexec::then(fun)),
      stdexec::starts_on(sched, stdexec::just(2) | stdexec::then(fun)));

  std::cout << "Before sync_wait" << std::endl;    
  auto [i, j, k] = stdexec::sync_wait(std::move(work)).value();

  // Print the results:
  std::printf("%d %d %d\n", i, j, k);
}
