#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arm_neon.h>

#define HEIGHT 1000
#define WIDTH 1000
static int active_field = 0;
static uint8_t field[2][HEIGHT][WIDTH];

#define NEON_HEIGHT 1000
#define NEON_WIDTH 62
#define NEON_VECTOR_SIZE 16
static uint8x16_t neon_field[2][NEON_HEIGHT][NEON_WIDTH];

void debug (int field_idx) {
  for (int yy = 0; yy < HEIGHT; yy++) {
    for (int xx = 0; xx < WIDTH; xx++) {
      printf("%d", field[field_idx][yy][xx]);
    }
    printf("\n");
  }
}

void debug_neon (int field_idx) {
  for (int yy = 0; yy < NEON_HEIGHT; yy++) {
    for (int xx = 0; xx < NEON_WIDTH; xx++) {
      for (int xxx = 0; xxx < NEON_VECTOR_SIZE; xxx++) {
        printf("%d", neon_field[field_idx][yy][xx][xxx]);
      }
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

void step_neon () {
  for (int yy = 0; yy < NEON_HEIGHT; yy++) {
    uint8x16_t prev = neon_field[active_field][yy][NEON_WIDTH - 1];
    uint8x16_t curr = neon_field[active_field][yy][0];
    uint8x16_t next = neon_field[active_field][yy][1];

    uint8x16_t out = vaddq_u8(curr, vaddq_u8(vextq_u8(curr, next, 1), vextq_u8(prev, curr, 15)));
    neon_field[!active_field][yy][0] = out;

    for (int xx = 1; xx < NEON_WIDTH - 1; xx++) {
      prev = curr;
      curr = next;
      next = neon_field[active_field][yy][xx + 1];

      out = vaddq_u8(curr, vaddq_u8(vextq_u8(curr, next, 1), vextq_u8(prev, curr, 15)));
      neon_field[!active_field][yy][xx] = out;
    }

    prev = curr;
    curr = next;
    next = neon_field[active_field][yy][0];

    out = vaddq_u8(curr, vaddq_u8(vextq_u8(curr, next, 1), vextq_u8(prev, curr, 15)));
    neon_field[!active_field][yy][NEON_WIDTH - 1] = out;
  }

  for (int xx = 0; xx < NEON_WIDTH; xx++) {
    for (int yy = 0; yy < NEON_HEIGHT; yy++) {
      uint8x16_t neighbours =
        vaddq_u8
          ( neon_field[!active_field][(yy - 1) % NEON_HEIGHT][xx]
          , vaddq_u8
              ( neon_field[!active_field][yy][xx]
              , neon_field[!active_field][(yy + 1) % NEON_HEIGHT][xx]
              )
          );
      uint8x16_t alive = neon_field[active_field][yy][xx];
      const uint8_t three_const = 3;
      const uint8_t four_const = 4;
      const uint8_t one_const = 1;
      uint8x16_t three = vld1q_dup_u8(&three_const);
      uint8x16_t four = vld1q_dup_u8(&four_const);
      uint8x16_t one = vld1q_dup_u8(&one_const);
      uint8x16_t out = vandq_u8(one, vorrq_u8(vceqq_u8(neighbours, three), vandq_u8(vceqq_u8(neighbours, four), alive)));
      neon_field[active_field][yy][xx] = out;
    }
  }
}

void print_u8x16 (uint8x16_t v) {
  for (int ii = 0; ii < 16; ii++) {
    printf("%2x ", v[ii]);
  }
}

int main (int argc, char** argv) {
  //field[active_field][1][2] = 1;
  //field[active_field][2][3] = 1;
  //field[active_field][3][1] = 1;
  //field[active_field][3][2] = 1;
  //field[active_field][3][3] = 1;
  //for (int ii = 0; ii < 100; ii++) {
  //  step_conv();
  //}

  neon_field[active_field][1][0][2] = 1;
  neon_field[active_field][2][0][3] = 1;
  neon_field[active_field][3][0][1] = 1;
  neon_field[active_field][3][0][2] = 1;
  neon_field[active_field][3][0][3] = 1;
  for (int ii = 0; ii < 100; ii++) {
    step_neon();
  }

  /*
  uint8_t A[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  uint8_t B[16] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
  uint8_t C[16] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f};
  uint8x16_t v1 = vld1q_u8(A);
  uint8x16_t v2 = vld1q_u8(B);
  uint8x16_t v3 = vld1q_u8(C);
  print_u8x16(v1); printf("\n");
  print_u8x16(v2); printf("\n");
  print_u8x16(v3); printf("\n");

  uint8x16_t left_window = vextq_u8(v1, v2, 15);
  print_u8x16(left_window); printf("\n");
  uint8x16_t right_window = vextq_u8(v2, v3, 1);
  print_u8x16(right_window); printf("\n");
  uint8x16_t out = vaddq_u8(v2, vaddq_u8(vextq_u8(v2, v3, 1), vextq_u8(v1, v2, 15)));
  print_u8x16(out); printf("\n");
  */
}
