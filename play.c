#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>

int active_buffer = 0;
int buffer_width = 800;
int buffer_height = 600;
int buffer[2][3][800][600];
int multi_channel = 0;

int (*mem)[800];

void randomize () {
  for (int xx = 0; xx < buffer_width; xx++) {
    for (int yy = 0; yy < buffer_height; yy++) {
      int channel_count = multi_channel ? 3 : 1;
      for (int channel = 0; channel < channel_count; channel++) {
        buffer[active_buffer][channel][xx][yy] = rand() % 2;
      }
    }
  }
}

void step_state () {
  for (int xx = 0; xx < buffer_width; xx++) {
    for (int yy = 0; yy < buffer_height; yy++) {
      int channel_count = multi_channel ? 3 : 1;
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

void redraw () {
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

void init_from_fb () {
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

  uint64_t elapsed = 0;
  uint64_t interval = 100000000L;
  while (1) {
    // Sleep 10ms
    struct timespec req, res;
    req.tv_sec = 0;
    req.tv_nsec = 10000000L;
    res.tv_sec = 0;
    res.tv_nsec = 0L;
    nanosleep(&req, &res);
    elapsed += req.tv_nsec - res.tv_nsec;

    if (elapsed > interval) {
      //printf("elapsed %d\n", elapsed);
      elapsed -= interval;
      //printf("redraw\n");
      step_state();
      redraw();
    }
  }
  redraw();

  munmap(mem, 800 * 600 * 4);
}
