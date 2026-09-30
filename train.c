#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "linreg.h"

#define BUFF_SIZE (size_t)(1024 * 1024 * 10)
#define uint8 unsigned char


void usage() { printf("Usage: ./train [data.csv]\n"); }

double estimate_t0(double t0, double t1, pair_t* entries, size_t m, double r) {

  double b = 0;
  // summation
  for (size_t i = 0; i < m; i++) {
    b += t1 * entries[i].x + t0 - entries[i].y;
  }
  // compute average
  b /= (double)m;
  // multiply by learning rate
  return r * b;
}

double estimate_t1(double t0, double t1, pair_t* entries, size_t m, double r) {

  double a = 0;
  // summation
  for (size_t i = 0; i < m; i++) {
    a += (t1 * entries[i].x + t0 - entries[i].y) * entries[i].x;
  }
  // compute average
  a /= (double)m;
  // multiply by learning rate
  return r * a;
}

void train(int n, ctx_t* ctx) {

  double tmp0 = 0, tmp1 = 0;
  printf("Start           t0 = %lf, t1 = %lf\n", ctx->t0, ctx->t1);
  for (int i = 0; i < n; i++) {
    tmp0 = estimate_t0(ctx->t0, ctx->t1, ctx->entries, ctx->nentries, ctx->r);
    tmp1 = estimate_t1(ctx->t0, ctx->t1, ctx->entries, ctx->nentries, ctx->r);
    ctx->t0 = tmp0;
    ctx->t1 = tmp1;
    printf("Epoch %4d/%-4d t0 = %lf, t1 = %lf\n", i, n, ctx->t0, ctx->t1);
  }
}

int save_variables(double t0, double t1) {
  printf("Saving theta0(%lf) and theta1(%lf)\n", t0, t1);
  FILE *f = fopen("vars.csv", "w+");
  if (f == NULL)
    return 1;
  int r = fprintf(f, "%lf,%lf\n", t0, t1);
  if (r < 0)
    return 1;
  fclose(f);
  return 0;
}

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
    if (n % 1000 == 0)
    {
      //  the first time, ctx->entries is NULL
      ctx->entries = realloc(ctx->entries, (n == 0 ? 1 : (n/1000)) * 1000 * sizeof(pair_t));
    }
    // building the pair
    pair_t* p = &ctx->entries[n];
    r = sscanf(line, "%lf,%lf\n", &p->x, &p->y);
    free(line);

    //end of file
    if (r == 0) {break;}
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


int main(int ac, char **av) {

  if (ac != 2) {
    usage();
    return 1;
  }

  FILE *f = fopen(av[1], "r");
  if (f == NULL) {
    perror("Could not open data file");
    return 1;
  }

  // parse the data
  ctx_t ctx;
  if (parse_data(&ctx, f)) {
    perror("Error while parsing data");
    return 1;
  }

  printf("Context: x = %s, y = %s, nentries = %lu\n", ctx.labelx, ctx.labely, ctx.nentries);
  printf("     %-20s %-20s\n", ctx.labelx, ctx.labely);
  for (size_t i = 0; i < ctx.nentries; i++) {
    printf("%-3lu: %-20lf %-20lf\n", i, ctx.entries[i].x, ctx.entries[i].y);
  }

  //learning rate
  ctx.r = 0.02;
  // run n epochs
  train(20, &ctx);

  if (save_variables(ctx.t0, ctx.t1)) {
    perror("could not save to file");
    exit(2);
  }

  if (gen_graph(&ctx)) {
    perror("Error generating graph file");
    return 1;
  }

  fclose(f);
  free(ctx.entries);
  return system("xdg-open ./result.html");
}
