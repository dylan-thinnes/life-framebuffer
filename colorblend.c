#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <arm_neon.h>

static const int buffer_width_vec_count = 100;
static const int buffer_width_vec_size = 8;
static const int buffer_width = buffer_width_vec_count * buffer_width_vec_size;
static const int buffer_height = 600;
static const int multi_channel = 1;
static const int channel_count = multi_channel ? 3 : 1;

static int active_buffer = 0;
static uint16x8_t buffer[2][3][600][100];

static uint16_t (*mem)[800];

void load () {
  for (int yy = 0; yy < 600; yy++) {
    for (int xx = 0; xx < 100; xx++) {
      for (int xxi = 0; xxi < 8; xxi++) {
        uint16_t val = mem[yy][xx * 8 + xxi];
        buffer[active_buffer][0][yy][xx][xxi] = val; // val > 0x8000 ? val : 0x0000;
      }
    }
  }
}

void blend () {
  uint16_t mask_ = 0b0011100111100111;
  uint16x8_t mask = vld1q_dup_u16(&mask_);
  for (int yy = 1; yy < 600 - 1; yy++) {
    for (int xx = 0; xx < 100; xx++) {
      uint16x8_t above = buffer[active_buffer][0][yy - 1][xx];
      above = vandq_u16(mask, vshrq_n_u16(above, 2));
      uint16x8_t curr = buffer[active_buffer][0][yy][xx];
      curr = vandq_u16(mask, vshrq_n_u16(curr, 2));
      uint16x8_t below = buffer[active_buffer][0][yy + 1][xx];
      below = vandq_u16(mask, vshrq_n_u16(below, 2));
      uint16x8_t out = vaddq_u16(above, vaddq_u16(curr, vaddq_u16(curr, below)));
      buffer[!active_buffer][0][yy][xx] = out;
    }
  }

  active_buffer = !active_buffer;
}

void store () {
  for (int yy = 0; yy < 600; yy++) {
    for (int xx = 0; xx < 100; xx++) {
      for (int xxi = 0; xxi < 8; xxi++) {
        mem[yy][xx * 8 + xxi] = buffer[active_buffer][0][yy][xx][xxi];
      }
    }
  }
}

int main (int argc, char **argv) {
  int fd = open("/dev/fb0", O_RDWR);
  mem = (uint16_t (*)[buffer_width]) mmap(NULL, buffer_width * buffer_height * 2, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

  load();
  blend();
  store();

  munmap(mem, buffer_width * buffer_height * 2);
}
