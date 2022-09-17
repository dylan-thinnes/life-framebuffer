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
static const int multi_channel = 0;
static const int channel_count = multi_channel ? 3 : 1;

static int active_buffer = 0;
static int buffer[2][3][800][600];

static int (*mem)[800];

static inline void randomize () {
  for (int xx = 0; xx < buffer_width; xx++) {
    for (int yy = 0; yy < buffer_height; yy++) {
      for (int channel = 0; channel < channel_count; channel++) {
        buffer[active_buffer][channel][xx][yy] = rand() % 2;
      }
    }
  }
}

static inline void step_state () {
  for (int xx = 0; xx < buffer_width; xx++) {
    for (int yy = 0; yy < buffer_height; yy++) {
      for (int channel = 0; channel < channel_count; channel++) {
        int neighbours = 0;
        for (int dx = -1; dx <= 1; dx++) {
          for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            int x = (xx + dx) % buffer_width;
            int y = (yy + dy) % buffer_height;
            neighbours += buffer[active_buffer][channel][x][y];
          }
        }
        buffer[!active_buffer][channel][xx][yy] =
          neighbours == 3 || (neighbours == 2 && buffer[active_buffer][channel][xx][yy]);
      }
    }
  }
  active_buffer = !active_buffer;
}

static inline void redraw () {
  for (int xx = 0; xx < buffer_width; xx++) {
    for (int yy = 0; yy < buffer_height; yy++) {
      if (multi_channel) {
        for (int channel = 0; channel < 3; channel++) {
          mem[yy][xx] =
            (buffer[active_buffer][0][xx][yy] * 0x00ff0000) |
            (buffer[active_buffer][1][xx][yy] * 0x0000ff00) |
            (buffer[active_buffer][2][xx][yy] * 0x000000ff);
        }
      } else {
        mem[yy][xx] = buffer[active_buffer][0][xx][yy] ? 0x00ffffff : 0x00000000;
      }
    }
  }
}

static inline void init_from_fb () {
  for (int xx = 0; xx < buffer_width; xx++) {
    for (int yy = 0; yy < buffer_height; yy++) {
      if (multi_channel) {
        int val = mem[yy][xx];
        int r = (val >> 16) & 0xff;
        int g = (val >>  8) & 0xff;
        int b = (val >>  0) & 0xff;
        buffer[active_buffer][0][xx][yy] = r > 16;
        buffer[active_buffer][1][xx][yy] = g > 16;
        buffer[active_buffer][2][xx][yy] = b > 16;
      } else {
        int val = mem[yy][xx];
        int r = (val >> 16) & 0xff;
        int g = (val >>  8) & 0xff;
        int b = (val >>  0) & 0xff;
        buffer[active_buffer][0][xx][yy] = r > 16 || g > 16 || b > 16;
      }
    }
  }
}

int main (int argc, char **argv) {
  int fd = open("/dev/fb0", O_RDWR);
  mem = (int (*)[800]) mmap(NULL, 800 * 600 * 4, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
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

  randomize();

  struct timespec start_ts, end_ts;
  timespec_get(&start_ts, TIME_UTC);
  for (int ii = 0; ii < 1000; ii++) {
    step_state();
    redraw();
  }

  timespec_get(&end_ts, TIME_UTC);
  double start = (double) start_ts.tv_sec * 1000 + (double) start_ts.tv_nsec / 1000000.0f;
  double end = (double) end_ts.tv_sec * 1000 + (double) end_ts.tv_nsec / 1000000.0f;
  double diff = end - start;
  printf ("Elapsed: %f ms\n", diff);

  munmap(mem, 800 * 600 * 4);
}
