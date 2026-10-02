#include "linreg.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int parse_data(ctx_t *ctx, FILE *f) {
  size_t len;
  char *line = NULL; // VERY IMPORTANT !!!
  double tmpx, tmpy;

  size_t linen = 1;
  memset(ctx, 0, sizeof(ctx_t));
  if (getline(&line, &len, f) > 0) {
    linen++;
    // check if the header is 2 valid strings and not numbers
    // otherwise we need to setup our own labels
    if (sscanf(line, "%255[^,],%255s", ctx->labelx, ctx->labely) != 2 ||
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
  }

  size_t n = 0;
  for (;;) {

    // get the line
    ssize_t r = getline(&line, &len, f);
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

    // try to get the two values
    r = sscanf(line, "%lf,%lf\n", &tmpx, &tmpy);
    free(line);

    // only consider well formed lines
    if (r == 2 && isfinite(tmpx) && isfinite(tmpy)) {
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
      fprintf(stderr, "WARNING: skipped line %lu, malformed?\n", linen);
    }
    line = NULL;
    linen++;
  }
  ctx->nentries = n;
  return 0;
}
