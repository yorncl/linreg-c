#include "linreg.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int parse_data_line(char *input, double *t0, double *t1) {
  // first number
  if (input[strspn(input, "0123456789+-.eE \t\r\n")] != ',')
    return 1;
  char *end;
  double x = strtod(input, &end);
  if (input == end)
    return 1;
  while (isspace((unsigned char)*end))
    end++;
  if (*end != ',' || !isfinite(x))
    return 1;

  *t0 = x;
  // pass the ,
  end++;

  // second number
  input = end;
  if (input[strspn(input, "0123456789+-.eE \t\r\n")] != '\0')
    return 1;
  x = strtod(input, &end);
  if (input == end)
    return 1;
  while (isspace((unsigned char)*end))
    end++;
  if (*end != '\0' || !isfinite(x))
    return 1;
  *t1 = x;

  return 0;
}

int parse_data(ctx_t *ctx, FILE *f) {
  size_t len;
  char *line = NULL; // VERY IMPORTANT !!!
  double tmpx, tmpy;
  int r;
  size_t linen = 1;
  memset(ctx, 0, sizeof(ctx_t));

  // first line
  r = getline(&line, &len, f);
  if (r == -1) {
    if (ferror(f)) {
      return 1;
    }
    if (feof(f)) {
      fprintf(stderr, "File is empty\n");
      exit(1);
      return 0;
    }
  }
  if (r > 0) {
    linen++;
    // check if the header is 2 valid strings and not numbers
    // otherwise we need to setup our own labels
    if ((size_t)r != strlen(line) ||
        sscanf(line, "%255[^,],%255s", ctx->labelx, ctx->labely) != 2 ||
        sscanf(line, "%lf,%lf", &tmpx, &tmpy) == 2) {
      strcpy(ctx->labelx, "x");
      strcpy(ctx->labely, "y");
      // reset cursor position
      fseek(f, 0, SEEK_SET);
      // reset line count
      linen = 1;
    }
    free(line);
    line = NULL;
  } else {
    return 1;
  }

  size_t n = 0;
  for (;;) {
    line = NULL;
    // get the line
    r = getline(&line, &len, f);
    // oopsie an error
    if (ferror(f)) {
      perror("Error reading the data file");
      exit(1);
    }
    // eof
    if (r == -1 && feof(f)) {
      free(line);
      break;
    }
    // nul byte in the middle, discard
    if ((size_t)r != strlen(line)) {
      fprintf(stderr, "WARNING: null character in line %lu, skipping?\n",
              linen);
      free(line);
      linen++;
      continue;
    }

    // try to get the two values
    if (parse_data_line(line, &tmpx, &tmpy) != 0) {
      fprintf(stderr, "WARNING: malformed line %lu, skipping\n", linen);
      free(line);
      linen++;
      continue;
    }

    // we don't need it anymore
    free(line);
    // only consider well formed lines
    if (isfinite(tmpx) && isfinite(tmpy)) {
      // realloc every 1000 entries
      if (n % 1000 == 0) {
        //  the first time, ctx->entries is NULL
        ctx->entries =
            realloc(ctx->entries, (n / 1000 + 1) * 1000 * sizeof(pair_t));
        if (ctx->entries == NULL) {
          fprintf(stderr, "Allocation failed!\n");
          exit(2);
        }
      }
      ctx->entries[n].x = tmpx;
      ctx->entries[n].y = tmpy;
      n++;
    } else {
      fprintf(
          stderr,
          "WARNING: malformed line %lu: accept only finite values, skipping\n",
          linen);
    }
    linen++;
  }
  ctx->nentries = n;
  return 0;
}
