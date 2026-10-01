#include "linreg.h"
#include <stdio.h>
#include <string.h>

int parse_data(ctx_t *ctx, FILE *f) {
  size_t len;
  char *line = NULL; // VERY IMPORTANT !!!

  memset(ctx, 0, sizeof(ctx_t));
  if (getline(&line, &len, f) > 0) {
    sscanf(line, "%[^','],%s", ctx->labelx, ctx->labely);
    free(line);
    line = NULL;
  }

  size_t n = 0;
  for (;;) {
    size_t r = getline(&line, &len, f);
    if (ferror(f)) {
      perror("Error reading the data file");
      return 1;
    }
    // realloc every 1000 entries
    if (n % 1000 == 0) {
      //  the first time, ctx->entries is NULL
      ctx->entries = realloc(ctx->entries,
                             (n == 0 ? 1 : (n / 1000)) * 1000 * sizeof(pair_t));
    }
    // building the pair
    pair_t *p = &ctx->entries[n];
    r = sscanf(line, "%lf,%lf\n", &p->x, &p->y);
    free(line);

    // end of file
    if (r == 0) {
      break;
    }
    // otherwise we expect exactly 2 floats in our format
    if (r < 2) {
      fprintf(stderr, "csv is malformed");
      return 1;
    }
    line = NULL;
    n++;
  }
  ctx->nentries = n;
  // scan names
  return 0;
}
