#include <stdio.h>
#include <stdlib.h>

int main () {
  char buff[1000];
  do {
    fgets(buff, 1000, stdin);
    fprintf(stderr, "Skipping: %s\n", buff);
  } while (buff[0] == '#');

  fprintf(stderr, "Input line: %s\n", buff);
  int width, height;
  int scanned = sscanf(buff, "x = %d, y = %d", &width, &height);

  if (scanned < 2) {
    fprintf(stderr, "Could not get x and y.\n");
    exit(1);
  }

  fprintf(stderr, "Got x = %d and y = %d\n", width, height);

  int ignore_from = 100;
  int ignore_until = 100;

  printf("P1\n%d %d\n", width - (ignore_until - ignore_from), height);

  int emitted_count = 0;
  int range_start, range_end;

  while (1) {
    int count = 1;
    scanf("%d", &count);

    char control = getc(stdin);
    fprintf(stderr, "Count %d, emitted_count %d, control %c\n", count, emitted_count, control);

    switch (control) {
      case '\n':
        break;
      case 'b':
        range_start = emitted_count;
        emitted_count += count;
        range_end = emitted_count;
        for (int i = range_start; i < range_end; i++) {
          if (ignore_from <= i && i < ignore_until) continue;
          printf("1");
        }
        break;
      case 'o':
        range_start = emitted_count;
        emitted_count += count;
        range_end = emitted_count;
        for (int i = range_start; i < range_end; i++) {
          if (ignore_from <= i && i < ignore_until) continue;
          printf("0");
        }
        break;
      case '$':
      case '!':
        for (int j = 0; j < count; j++) {
          range_start = emitted_count;
          range_end = width;
          for (int i = range_start; i < range_end; i++) {
            if (ignore_from <= i && i < ignore_until) continue;
            printf("1");
          }
          printf("\n");
          emitted_count = 0;
        }
        break;
    }

    if (control == -1 || control == '!') break;
  }
}
