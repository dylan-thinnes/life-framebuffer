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

  printf("P1\n%d %d\n", width, height);

  int emitted_count = 0;

  while (1) {
    int count = 1;
    scanf("%d", &count);

    char control = getc(stdin);
    fprintf(stderr, "Count %d, emitted_count %d, control %c\n", count, emitted_count, control);

    switch (control) {
      case '\n':
        break;
      case 'b':
	emitted_count += count;
        for (int i = 0; i < count; i++) {
          printf("1");
        }
        break;
      case 'o':
	emitted_count += count;
        for (int i = 0; i < count; i++) {
          printf("0");
        }
        break;
      case '$':
      case '!':
	for (int j = 0; j < count; j++) {
          for (int i = 0; i < width - emitted_count; i++) {
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
