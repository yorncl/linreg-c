#include "linreg.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {

  double t0, t1;
  FILE *f;
  double x;
  char input[512];
  char *filename = "vars.csv";

  if (access(filename, F_OK) != 0) {
    perror("Could not access variable file");
    printf("Setting t0 and t1 to 0\n");
    t0 = 0;
    t1 = 0;
  } else {
    f = fopen(filename, "r");
    if (f == NULL) {
      perror("Error reading variable file");
      return 1;
    }
    if (fscanf(f, "%lf,%lf", &t0, &t1) != 2) {
      fprintf(
          stderr,
          "Variable file looks malformed, expected format: \"%%lf,%%lf\"\n");
      return 1;
    }
  }

  printf("Enter x, get a y, it's that simple\n");
  while (1) {
    printf("> ");
    memset(input, 0, 512);
    if (fgets(input, 512, stdin) == NULL)
      break;
    if (sscanf(input, "%lf\n", &x) != 1) {
      printf("Hmmm, are you sure you put a number in?\n");
      continue;
    }
    printf("y = %lf\n", t0 + x * t1);
  }
  return 0;
}
