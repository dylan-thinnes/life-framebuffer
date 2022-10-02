#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <arm_neon.h>

#ifndef BUFFER_HEIGHT
  #define BUFFER_HEIGHT 600
#endif

#ifndef BUFFER_WIDTH
  #define BUFFER_WIDTH 800
#endif

static const int buffer_width_vec_count = BUFFER_WIDTH / 8;
static const int buffer_width_vec_size = 8;
static const int buffer_width = buffer_width_vec_count * buffer_width_vec_size;
static const int buffer_height = BUFFER_HEIGHT;

static uint16x8_t buffer[3][BUFFER_HEIGHT][BUFFER_WIDTH / 8];

#ifndef NO_WRITE
static uint16_t (*mem)[800];
#endif

static inline void randomize () {
  for (int yy = 0; yy < buffer_height; yy++) {
    for (int xx_outer = 0; xx_outer < buffer_width_vec_count; xx_outer++) {
      for (int xx_inner = 0; xx_inner < buffer_width_vec_size; xx_inner++) {
        buffer[0][yy][xx_outer][xx_inner] = rand() % 2;
      }
    }
  }
}

static inline void debug () {
  for (int yy = 0; yy < buffer_height; yy++) {
    for (int xx_outer = 0; xx_outer < buffer_width_vec_count; xx_outer++) {
      for (int xx_inner = 0; xx_inner < buffer_width_vec_size; xx_inner++) {
        printf("%d", buffer[0][yy][xx_outer][xx_inner]);
      }
    }
    printf("\n");
  }
  printf("\n");
}

static inline void step_state () {
  for (int yy = 0; yy < buffer_height; yy++) {
    uint16x8_t prev = buffer[0][yy][buffer_width_vec_count - 1];
    uint16x8_t curr = buffer[0][yy][0];
    uint16x8_t next = buffer[0][yy][1];

    uint16x8_t out = vaddq_u16(curr, vaddq_u16(vextq_u16(curr, next, 1), vextq_u16(prev, curr, buffer_width_vec_size - 1)));
    buffer[1][yy][0] = out;

    for (int xx = 1; xx < buffer_width_vec_count - 1; xx++) {
      prev = curr;
      curr = next;
      next = buffer[0][yy][xx + 1];

      out = vaddq_u16(curr, vaddq_u16(vextq_u16(curr, next, 1), vextq_u16(prev, curr, buffer_width_vec_size - 1)));
      buffer[1][yy][xx] = out;
    }

    prev = curr;
    curr = next;
    next = buffer[0][yy][0];

    out = vaddq_u16(curr, vaddq_u16(vextq_u16(curr, next, 1), vextq_u16(prev, curr, buffer_width_vec_size - 1)));
    buffer[1][yy][buffer_width_vec_count - 1] = out;
  }

  for (int xx = 0; xx < buffer_width_vec_count; xx++) {
    for (int yy = 0; yy < buffer_height; yy++) {
      uint16x8_t neighbours =
        vaddq_u16
          ( buffer[1][(yy - 1) % buffer_height][xx]
          , vaddq_u16
              ( buffer[1][yy][xx]
              , buffer[1][(yy + 1) % buffer_height][xx]
              )
          );
      uint16x8_t alive = buffer[0][yy][xx];
      const uint16_t three_const = 3;
      const uint16_t four_const = 4;
      const uint16_t one_const = 1;
      uint16x8_t three = vld1q_dup_u16(&three_const);
      uint16x8_t four = vld1q_dup_u16(&four_const);
      uint16x8_t one = vld1q_dup_u16(&one_const);
      uint16x8_t out = vandq_u16(one, vorrq_u16(vceqq_u16(neighbours, three), vandq_u16(vceqq_u16(neighbours, four), alive)));
      buffer[0][yy][xx] = out;
    }
  }
}

#ifndef NO_WRITE
static inline void redraw () {
  for (int yy = 0; yy < buffer_height; yy++) {
    for (int xx_outer = 0; xx_outer < buffer_width_vec_count; xx_outer++) {
      for (int xx_inner = 0; xx_inner < buffer_width_vec_size; xx_inner++) {
        int xx = xx_outer * buffer_width_vec_size + xx_inner;
        mem[yy][xx] = buffer[0][yy][xx_outer][xx_inner] ? 0xFFFF : 0x0000;
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
        buffer[0][yy][xx_outer][xx_inner] = val > 0x8000;
      }
    }
  }
}
#endif

int main (int argc, char **argv) {
  int steps = -1;
  if (argc > 1) sscanf(argv[1], "%d", &steps);

#ifndef NO_WRITE
  int fd = open("/dev/fb0", O_RDWR);
  mem = (uint16_t (*)[buffer_width]) mmap(NULL, buffer_width * buffer_height * 2, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
#endif

  //init_from_fb();
  randomize();
  //buffer[0][0][0][1] = 1;
  //buffer[0][1][0][2] = 1;
  //buffer[0][2][0][0] = 1;
  //buffer[0][2][0][1] = 1;
  //buffer[0][2][0][2] = 1;

#ifndef NO_WRITE
  redraw();
#endif
  uint64_t elapsed = 0;
  uint64_t interval = 100000000L;
  while (steps--) {
    struct timespec start_ts, end_ts;
    timespec_get(&start_ts, TIME_UTC);
    step_state();
    timespec_get(&end_ts, TIME_UTC);
    double start = (double) start_ts.tv_sec * 1000 + (double) start_ts.tv_nsec / 1000000.0f;
    double end = (double) end_ts.tv_sec * 1000 + (double) end_ts.tv_nsec / 1000000.0f;
    double diff = end - start;
    printf ("Calc:  %3.f ms\n", diff);

#ifndef NO_WRITE
    timespec_get(&start_ts, TIME_UTC);
    redraw();
    timespec_get(&end_ts, TIME_UTC);
    start = (double) start_ts.tv_sec * 1000 + (double) start_ts.tv_nsec / 1000000.0f;
    end = (double) end_ts.tv_sec * 1000 + (double) end_ts.tv_nsec / 1000000.0f;
    diff = end - start;
    printf ("Write: %3.f ms\n", diff);
#endif
  }

#ifndef NO_WRITE
  munmap(mem, buffer_width * buffer_height * 2);
#endif
}
