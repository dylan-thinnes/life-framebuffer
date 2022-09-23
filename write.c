#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>

static const int buffer_width = 800;
static const int buffer_height = 600;
static uint16_t (*mem)[800];

int main (int argc, char** argv) {
  int offset_x, offset_y;
  sscanf(argv[1], "%d", &offset_x);
  sscanf(argv[2], "%d", &offset_y);

  int fd = open("/dev/fb0", O_RDWR);
  mem = (uint16_t (*)[buffer_width]) mmap(NULL, buffer_width * buffer_height * 2, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

  int height, width, prec;
  printf("%d\n", scanf("P6\n%d %d\n%d\n", &width, &height, &prec));

  for (int yy = 0; yy < height; yy++) {
    for (int xx = 0; xx < width; xx++) {
      int r = getc(stdin);
      int g = getc(stdin);
      int b = getc(stdin);
      int bits = (r >> 3) << 11 | (g >> 2) << 5 | b >> 3;
      mem[offset_y + yy][offset_x + xx] = bits;
    }
  }

  munmap(mem, buffer_width * buffer_height * 2);
}
