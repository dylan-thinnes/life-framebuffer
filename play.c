#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <arm_neon.h>
#include <pthread.h>

#define THREAD_NUMS 4
pthread_barrier_t barrier;

#ifndef BUFFER_HEIGHT
  #define BUFFER_HEIGHT 600
#endif

#ifndef BUFFER_WIDTH
  #define BUFFER_WIDTH 800
#endif

static const int buffer_width_vec_size = 16;
static const int buffer_width_vec_count = BUFFER_WIDTH / 16;
static const int buffer_width = buffer_width_vec_count * buffer_width_vec_size;
static const int buffer_height = BUFFER_HEIGHT;

static uint8x16_t buffer[3][BUFFER_HEIGHT][BUFFER_WIDTH / 16];

static uint16_t (*mem)[800];

static inline void randomize () {
  for (int yy = 0; yy < buffer_height; yy++) {
    for (int xx_outer = 0; xx_outer < buffer_width_vec_count; xx_outer++) {
      for (int xx_inner = 0; xx_inner < buffer_width_vec_size; xx_inner++) {
        buffer[0][yy][xx_outer][xx_inner] = rand() % 2;
      }
    }
  }
}

static inline void debug (int buffer_idx) {
  for (int yy = 0; yy < buffer_height; yy++) {
    for (int xx_outer = 0; xx_outer < buffer_width_vec_count; xx_outer++) {
      for (int xx_inner = 0; xx_inner < buffer_width_vec_size; xx_inner++) {
        printf("%d", buffer[buffer_idx][yy][xx_outer][xx_inner]);
      }
    }
    printf("\n");
  }
  printf("\n");
}

static inline void step_state_1 (int y_start, int y_end) {
  for (int yy = y_start; yy < y_end; yy++) {
    uint8x16_t prev = buffer[0][yy][buffer_width_vec_count - 1];
    uint8x16_t curr = buffer[0][yy][0];
    uint8x16_t next = buffer[0][yy][1];

    uint8x16_t out = vaddq_u8(curr, vaddq_u8(vextq_u8(curr, next, 1), vextq_u8(prev, curr, buffer_width_vec_size - 1)));
    buffer[1][yy][0] = out;

    for (int xx = 1; xx < buffer_width_vec_count - 1; xx++) {
      prev = curr;
      curr = next;
      next = buffer[0][yy][xx + 1];

      out = vaddq_u8(curr, vaddq_u8(vextq_u8(curr, next, 1), vextq_u8(prev, curr, buffer_width_vec_size - 1)));
      buffer[1][yy][xx] = out;
    }

    prev = curr;
    curr = next;
    next = buffer[0][yy][0];

    out = vaddq_u8(curr, vaddq_u8(vextq_u8(curr, next, 1), vextq_u8(prev, curr, buffer_width_vec_size - 1)));
    buffer[1][yy][buffer_width_vec_count - 1] = out;
  }
}

static inline void step_state_2 (int y_start, int y_end) {
  const uint8_t three_const = 3;
  const uint8_t four_const = 4;
  const uint8_t one_const = 1;
  uint8x16_t three = vld1q_dup_u8(&three_const);
  uint8x16_t four = vld1q_dup_u8(&four_const);
  uint8x16_t one = vld1q_dup_u8(&one_const);

  for (int xx = 0; xx < buffer_width_vec_count; xx++) {
    uint8x16_t prev = buffer[1][y_start == 0 ? buffer_height - 1 : y_start - 1][xx];
    uint8x16_t curr = buffer[1][y_start][xx];
    uint8x16_t next = buffer[1][y_start + 1][xx];

    uint8x16_t neighbours = vaddq_u8(prev, vaddq_u8(curr, next));
    uint8x16_t alive = buffer[0][y_start][xx];
    uint8x16_t out = vandq_u8(one, vorrq_u8(vceqq_u8(neighbours, three), vandq_u8(vceqq_u8(neighbours, four), alive)));
    uint8x16_t start_out = out;

    for (int yy = y_start + 1; yy < y_end - 1; yy++) {
      prev = curr;
      curr = next;
      next = buffer[1][yy + 1][xx];

      neighbours = vaddq_u8(prev, vaddq_u8(curr, next));

      alive = buffer[0][yy][xx];
      out = vandq_u8(one, vorrq_u8(vceqq_u8(neighbours, three), vandq_u8(vceqq_u8(neighbours, four), alive)));
      buffer[0][yy][xx] = out;
    }

    prev = curr;
    curr = next;
    next = buffer[1][y_end == buffer_height ? 0 : y_end][xx];

    neighbours = vaddq_u8(prev, vaddq_u8(curr, next));
    alive = buffer[0][y_end - 1][xx];
    out = vandq_u8(one, vorrq_u8(vceqq_u8(neighbours, three), vandq_u8(vceqq_u8(neighbours, four), alive)));
    buffer[0][y_end - 1][xx] = out;
    buffer[0][y_start][xx] = start_out;
  }
}

#ifndef NO_WRITE
static inline void redraw (int y_start, int y_end) {
  for (int yy = y_start; yy < y_end; yy++) {
    for (int xx_outer = 0; xx_outer < buffer_width_vec_count; xx_outer++) {
      for (int xx_inner = 0; xx_inner < buffer_width_vec_size; xx_inner++) {
        int xx = xx_outer * buffer_width_vec_size + xx_inner;
        mem[yy][xx] = buffer[0][yy][xx_outer][xx_inner] ? 0xFC00 : 0x0000;
      }
    }
  }
}
#endif

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

void* run_thread (void* vargs) {
  int* args = vargs;
  int n = (int) args[0];
  int start_y = (int) args[1];
  int end_y = (int) args[2];
  int is_first = (int) args[3];
  int i = n;
  while (i--) {
#ifdef TIMING
    struct timespec start_ts, end_ts;
    if (is_first) {
      timespec_get(&start_ts, TIME_UTC);
    }
#endif
    pthread_barrier_wait(&barrier);
    step_state_1(start_y, end_y);
    pthread_barrier_wait(&barrier);
    step_state_2(start_y, end_y);

#ifdef TIMING
    if (is_first) {
      timespec_get(&end_ts, TIME_UTC);
      double start = (double) start_ts.tv_sec * 1000 + (double) start_ts.tv_nsec / 1000000.0f;
      double end = (double) end_ts.tv_sec * 1000 + (double) end_ts.tv_nsec / 1000000.0f;
      double diff = end - start;
      printf ("Calc:  %3.f ms\n", diff);
      timespec_get(&start_ts, TIME_UTC);
    }
#endif

#ifndef NO_WRITE
    redraw(start_y, end_y);
#endif

#ifdef TIMING
    if (is_first) {
      timespec_get(&end_ts, TIME_UTC);
      double start = (double) start_ts.tv_sec * 1000 + (double) start_ts.tv_nsec / 1000000.0f;
      double end = (double) end_ts.tv_sec * 1000 + (double) end_ts.tv_nsec / 1000000.0f;
      double diff = end - start;
      printf ("Write: %3.f ms\n", diff);
    }
#endif
  }

#ifdef TIMING
  printf("Done!\n");
#endif
  return NULL;
}

int main (int argc, char **argv) {
  pthread_barrier_init(&barrier, NULL, THREAD_NUMS);

  int steps = -1;
  if (argc > 1) sscanf(argv[1], "%d", &steps);

  int fd = open("/dev/fb0", O_RDWR);
  mem = (uint16_t (*)[buffer_width]) mmap(NULL, buffer_width * buffer_height * 2, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

  init_from_fb();
  //randomize();
  //buffer[0][3 - 0][0][1] = 1;
  //buffer[0][3 - 1][0][2] = 1;
  //buffer[0][3 - 2][0][0] = 1;
  //buffer[0][3 - 2][0][1] = 1;
  //buffer[0][3 - 2][0][2] = 1;

#ifndef NO_WRITE
  redraw(0, buffer_height);
#endif

  int t_args[16] = {
    steps, 0, buffer_height / 4, 1,
    steps, buffer_height / 4, buffer_height * 2 / 4, 0,
    steps, buffer_height * 2 / 4, buffer_height * 3 / 4, 0,
    steps, buffer_height * 3 / 4, buffer_height, 0
  };
  pthread_t t[THREAD_NUMS];
  pthread_create(&t[0], NULL, &run_thread, (void*) &t_args[0]);
  pthread_create(&t[1], NULL, &run_thread, (void*) &t_args[4]);
  pthread_create(&t[2], NULL, &run_thread, (void*) &t_args[8]);
  run_thread((void*) &t_args[12]);
  pthread_join(t[0], NULL);
  pthread_join(t[1], NULL);
  pthread_join(t[2], NULL);

  munmap(mem, buffer_width * buffer_height * 2);
}
