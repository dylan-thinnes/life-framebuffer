#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define HEIGHT 1000
#define WIDTH 1000
static int active_field = 0;
static uint8_t field[2][HEIGHT][WIDTH];

void debug () {
  printf("Active:\n");
  for (int yy = 0; yy < HEIGHT; yy++) {
    for (int xx = 0; xx < WIDTH; xx++) {
      printf("%d", field[active_field][yy][xx]);
    }
    printf("\n");
  }

  printf("Inactive:\n");
  for (int yy = 0; yy < HEIGHT; yy++) {
    for (int xx = 0; xx < WIDTH; xx++) {
      printf("%d", field[!active_field][yy][xx]);
    }
    printf("\n");
  }
}

void step_state () {
  for (int xx = 0; xx < WIDTH; xx++) {
    for (int yy = 0; yy < HEIGHT; yy++) {
      int neighbours = 0;
      for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
          if (dx == 0 && dy == 0) continue;
          int x = (xx + dx) % WIDTH;
          int y = (yy + dy) % HEIGHT;
          neighbours += field[active_field][y][x];
        }
      }
      field[!active_field][yy][xx] =
        neighbours == 3 || (neighbours == 2 && field[active_field][yy][xx]);
    }
  }
  active_field = !active_field;
}

void step_conv () {
  for (int yy = 0; yy < HEIGHT; yy++) {
    field[!active_field][0][yy] =
      field[active_field][yy][WIDTH - 1] + field[active_field][yy][0] + field[active_field][yy][1];
    for (int xx = 1; xx < WIDTH - 1; xx++) {
      field[!active_field][xx][yy] =
        field[active_field][yy][xx - 1] + field[active_field][yy][xx] + field[active_field][yy][xx + 1];
    }
    field[!active_field][WIDTH - 1][yy] =
      field[active_field][yy][WIDTH - 2] + field[active_field][yy][WIDTH - 1] + field[active_field][yy][0];
  }
  for (int xx = 0; xx < WIDTH; xx++) {
    int neighbours =
      field[!active_field][xx][HEIGHT - 1] + field[!active_field][xx][0] + field[!active_field][xx][1];
    field[active_field][0][xx] = neighbours == 3 || neighbours == 4 && field[active_field][0][xx];
    for (int yy = 1; yy < HEIGHT - 1; yy++) {
      neighbours =
        field[!active_field][xx][yy - 1] + field[!active_field][xx][yy] + field[!active_field][xx][yy + 1];
      field[active_field][yy][xx] = neighbours == 3 || neighbours == 4 && field[active_field][yy][xx];
    }
    neighbours =
      field[!active_field][xx][HEIGHT - 2] + field[!active_field][xx][HEIGHT - 1] + field[!active_field][xx][0];
    field[active_field][HEIGHT - 1][xx] = neighbours == 3 || neighbours == 4 && field[active_field][HEIGHT - 1][xx];
  }
}

int main (int argc, char** argv) {
  field[active_field][1][2] = 1;
  field[active_field][2][3] = 1;
  field[active_field][3][1] = 1;
  field[active_field][3][2] = 1;
  field[active_field][3][3] = 1;
  for (int ii = 0; ii < 1000; ii++) {
    step_conv();
  }
}
