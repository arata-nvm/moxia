#include "printk.hpp"
#include "console.hpp"
#include <stdarg.h>
#include <stdio.h>

int printk(const char *format...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  console->PutString(s);
  return result;
}
