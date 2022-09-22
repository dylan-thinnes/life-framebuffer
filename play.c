#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <arm_neon.h>

static const int buffer_width_vec_count = 50;
static const int buffer_width_vec_size = 16;
static const int buffer_width = buffer_width_vec_count * buffer_width_vec_size;
static const int buffer_height = 600;
static const int multi_channel = 1;
static const int channel_count = multi_channel ? 3 : 1;

static int active_buffer = 0;
static uint8x16_t buffer[2][3][600][50];

static uint16_t (*mem)[800];

static inline void randomize () {
  for (int channel = 0; channel < channel_count; channel++) {
    for (int yy = 0; yy < buffer_height; yy++) {
      for (int xx_outer = 0; xx_outer < buffer_width_vec_count; xx_outer++) {
        for (int xx_inner = 0; xx_inner < buffer_width_vec_size; xx_inner++) {
          buffer[active_buffer][channel][yy][xx_outer][xx_inner] = rand() % 2;
        }
      }
    }
  }
}

static inline void debug () {
  for (int channel = 0; channel < channel_count; channel++) {
    for (int yy = 0; yy < buffer_height; yy++) {
      for (int xx_outer = 0; xx_outer < buffer_width_vec_count; xx_outer++) {
        for (int xx_inner = 0; xx_inner < buffer_width_vec_size; xx_inner++) {
          printf("%d", buffer[active_buffer][channel][yy][xx_outer][xx_inner]);
        }
      }
      printf("\n");
    }
    printf("\n");
  }
  printf("\n");
}

static inline void step_state () {
  for (int channel = 0; channel < channel_count; channel++) {
    for (int yy = 0; yy < buffer_height; yy++) {
      uint8x16_t prev = buffer[active_buffer][channel][yy][buffer_width_vec_count - 1];
      uint8x16_t curr = buffer[active_buffer][channel][yy][0];
      uint8x16_t next = buffer[active_buffer][channel][yy][1];

      uint8x16_t out = vaddq_u8(curr, vaddq_u8(vextq_u8(curr, next, 1), vextq_u8(prev, curr, 15)));
      buffer[!active_buffer][channel][yy][0] = out;

      for (int xx = 1; xx < buffer_width_vec_count - 1; xx++) {
        prev = curr;
        curr = next;
        next = buffer[active_buffer][channel][yy][xx + 1];

        out = vaddq_u8(curr, vaddq_u8(vextq_u8(curr, next, 1), vextq_u8(prev, curr, 15)));
        buffer[!active_buffer][channel][yy][xx] = out;
      }

      prev = curr;
      curr = next;
      next = buffer[active_buffer][channel][yy][0];

      out = vaddq_u8(curr, vaddq_u8(vextq_u8(curr, next, 1), vextq_u8(prev, curr, 15)));
      buffer[!active_buffer][channel][yy][buffer_width_vec_count - 1] = out;
    }

    for (int xx = 0; xx < buffer_width_vec_count; xx++) {
      for (int yy = 0; yy < buffer_height; yy++) {
        uint8x16_t neighbours =
          vaddq_u8
            ( buffer[!active_buffer][channel][(yy - 1) % buffer_height][xx]
            , vaddq_u8
                ( buffer[!active_buffer][channel][yy][xx]
                , buffer[!active_buffer][channel][(yy + 1) % buffer_height][xx]
                )
            );
        uint8x16_t alive = buffer[active_buffer][channel][yy][xx];
        const uint8_t three_const = 3;
        const uint8_t four_const = 4;
        const uint8_t one_const = 1;
        uint8x16_t three = vld1q_dup_u8(&three_const);
        uint8x16_t four = vld1q_dup_u8(&four_const);
        uint8x16_t one = vld1q_dup_u8(&one_const);
        uint8x16_t out = vandq_u8(one, vorrq_u8(vceqq_u8(neighbours, three), vandq_u8(vceqq_u8(neighbours, four), alive)));
        buffer[active_buffer][channel][yy][xx] = out;
      }
    }
  }
}

static inline void redraw () {
  for (int yy = 0; yy < buffer_height; yy++) {
    for (int xx_outer = 0; xx_outer < buffer_width_vec_count; xx_outer++) {
      for (int xx_inner = 0; xx_inner < buffer_width_vec_size; xx_inner++) {
        int xx = xx_outer * buffer_width_vec_size + xx_inner;
        if (multi_channel) {
          mem[yy][xx] =
            (buffer[active_buffer][0][yy][xx_outer][xx_inner] ? 0xF800 : 0x0000) |
            (buffer[active_buffer][1][yy][xx_outer][xx_inner] ? 0x07E0 : 0x0000) |
            (buffer[active_buffer][2][yy][xx_outer][xx_inner] ? 0x001F : 0x0000);
        } else {
          mem[yy][xx] = buffer[active_buffer][0][yy][xx_outer][xx_inner] ? 0xffff : 0x0000;
        }
      }
    }
  }
}

static inline void init_from_fb () {
  for (int yy = 0; yy < buffer_height; yy++) {
    for (int xx_outer = 0; xx_outer < buffer_width_vec_count; xx_outer++) {
      for (int xx_inner = 0; xx_inner < buffer_width_vec_size; xx_inner++) {
        int xx = xx_outer * buffer_width_vec_size + xx_inner;
        uint16_t val = mem[yy][xx];
        uint16_t r = (val >> 11) & 0x1f;
        uint16_t g = (val >>  5) & 0x3f;
        uint16_t b = (val >>  0) & 0x1f;
        if (multi_channel) {
          buffer[active_buffer][0][yy][xx_outer][xx_inner] = r > 16;
          buffer[active_buffer][1][yy][xx_outer][xx_inner] = g > 32;
          buffer[active_buffer][2][yy][xx_outer][xx_inner] = b > 16;
        } else {
          buffer[active_buffer][0][yy][xx_outer][xx_inner] = r > 2 || g > 2 || b > 2;
        }
      }
    }
  }
}

int main (int argc, char **argv) {
  int fd = open("/dev/fb0", O_RDWR);
  mem = (uint16_t (*)[buffer_width]) mmap(NULL, buffer_width * buffer_height * 2, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  /*
  for (int xx = 0; xx < 256; xx++) {
    for (int yy = 0; yy < 256; yy++) {
      int x = xx + 100;
      int y = yy + 0;
      int rr = (xx * 10) % 256;
      int gg = (yy * 10) % 256;
      int bb = 0x80;
      //int bb = (xx * yy) % 256;
      mem[y][x] = rr << 16 | gg << 8 | bb;
    }
  }
  */

  init_from_fb();
  //randomize();
  //buffer[active_buffer][0][0][0][1] = 1;
  //buffer[active_buffer][0][1][0][2] = 1;
  //buffer[active_buffer][0][2][0][0] = 1;
  //buffer[active_buffer][0][2][0][1] = 1;
  //buffer[active_buffer][0][2][0][2] = 1;

  uint64_t elapsed = 0;
  uint64_t interval = 100000000L;
  while (1) {
    struct timespec start_ts, end_ts;
    timespec_get(&start_ts, TIME_UTC);
    step_state();
    timespec_get(&end_ts, TIME_UTC);
    double start = (double) start_ts.tv_sec * 1000 + (double) start_ts.tv_nsec / 1000000.0f;
    double end = (double) end_ts.tv_sec * 1000 + (double) end_ts.tv_nsec / 1000000.0f;
    double diff = end - start;
    printf ("Calc:  %3.f ms\n", diff);

    timespec_get(&start_ts, TIME_UTC);
    redraw();
    timespec_get(&end_ts, TIME_UTC);
    start = (double) start_ts.tv_sec * 1000 + (double) start_ts.tv_nsec / 1000000.0f;
    end = (double) end_ts.tv_sec * 1000 + (double) end_ts.tv_nsec / 1000000.0f;
    diff = end - start;
    printf ("Write: %3.f ms\n", diff);
  }
  redraw();

  munmap(mem, buffer_width * buffer_height * 2);
}
