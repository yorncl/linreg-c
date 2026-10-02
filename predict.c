#include "linreg.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void var_file_error() {
  fprintf(stderr, "Variable file appears malformed, expecting a single line "
                  "with finite doubles "
                  "with format: \"%%lf,%%lf\n");
  exit(1);
}

int valid_input(char *input) {
  if (input[strspn(input, "0123456789+-.eE \t\r\n")] != '\0') {
    return 0;
  }
  char *end;
  double x = strtod(input, &end);
  while (isspace((unsigned char)*end))
    end++;
  if (*end != '\0' || !isfinite(x))
    return 0;
  return 1;
}

int main(void) {

  double t0, t1;
  FILE *f;
  double x;
  char *filename = "vars.csv";
  char *input;
  size_t len;
  ssize_t r;
  // gather variables
  f = fopen(filename, "r");
  if (f == NULL) {
    if (errno != ENOENT) { // exists but can't be read: refuse
      perror("Could not read variable file");
      return 1;
    }
    perror("WARNING: Could not access variable file");
    printf("Setting t0 and t1 to 0\n");
    t0 = 0;
    t1 = 0;
  } else {
    char *line = NULL;
    // directory -> getline fails with EISDIR, so r == -1 covers it
    r = getline(&line, &len, f);
    if (r == -1 || (size_t)r != strlen(line) ||
        parse_data_line(line, &t0, &t1) || getline(&line, &len, f) != -1)
      var_file_error();
    free(line);
    fclose(f);
  }

  // predict input loop
  printf("Enter x, get a y, it's that simple\n");
  while (1) {
    printf("> ");
    input = NULL;
    if ((r = getline(&input, &len, stdin)) == -1) {
      free(input);
      break;
    }
    if (input[strspn(input, " \t\r\n")] == '\0') {
      free(input);
      continue;
    }
    // check with strlen against r read bytes, in case of a null byte in the
    // middle
    if ((size_t)r != strlen(input) || !valid_input(input)) {
      fprintf(stderr, "Please enter a valid number\n");
      free(input);
      continue;
    }
    sscanf(input, "%lf", &x);
    free(input);
    double y = t0 + x * t1;
    if (!isfinite(y))
      fprintf(stderr,
              "The resulting number is not finite, try with another x;\n");
    else
      printf("y = %.17g\n", y);
  }
  return 0;
}
