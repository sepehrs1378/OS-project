/* Writes to multiple sectors multiple times byte by byte. Access to disk
   must be near sum of all sectors. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

const int size = 65536, expected_writes = size / 512 + 20;

void test_main() {
  CHECK(create("test", size), "Create a file with 64K size");
  int i;
  int fd;
  CHECK(fd = open("test"), "Open the file!");
  for (i = 0; i < size; i++)
    if (!write(fd, (void*)"1", 1))
      fail("Can't write to the file\n");
  char c;
  for (i = 0; i < size; i++)
    if (!read(fd, (void*)&c, 1))
      fail("Can't read from the file\n");
  unsigned long long disk_w = stat_disk_w();
  if (disk_w >= expected_writes)
    fail("Too much disk writes (%lld). must be less than %d.", disk_w, expected_writes);
}
