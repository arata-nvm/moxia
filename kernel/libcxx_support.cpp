#include "printk.hpp"
#include <cerrno>
#include <new>

std::new_handler std::get_new_handler() noexcept {
  return [] {
    printk("not enough memory\n");
    exit(1);
  };
}

extern "C" int posix_memalign(void **, size_t, size_t) {
  return ENOMEM;
}

extern "C" void __cxa_pure_virtual() {
  while (1)
    ;
}
