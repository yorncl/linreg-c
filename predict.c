#include "linreg.h"
#include <stdio.h>
#include <string.h>

int main(void) {
  FILE *f = fopen("vars.csv", "r");
  if (f == NULL) {
    perror("Could not open variable file");
    fprintf(stderr, "Are you sure you've run ./train first?\n");
    return 1;
  }

  double t0, t1;
  if (fscanf(f, "%lf,%lf", &t0, &t1) != 2) {
    fprintf(stderr,
            "Variables file looks malformed, expected format: \"%%lf,%%lf\"\n");
  }

  double x;
  char input[512];
  printf("Enter x, get a y, it's that simple\n");
  while (1) {
    printf("> ");
    memset(input, 0, 512);
    if (fgets(input, 512, stdin) == NULL)
      break;
    if (sscanf(input, "%lf\n", &x) != 1) {
      printf("Hmmm, are you sure you put a number in?");
    }
    printf("y = %lf\n", t0 + x * t1);
  }
  return 0;
}
