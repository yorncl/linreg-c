#include "linreg.h"
#include <fcntl.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFF_SIZE (size_t)(1024 * 1024 * 10)
#define uint8 unsigned char

void usage() { printf("Usage: ./train [data.csv]\n"); }

void compute_range(ctx_t *ctx) {
  pair_t *p = ctx->entries;

  double minx = DBL_MAX;
  double maxx = -DBL_MAX;
  double miny = DBL_MAX;
  double maxy = -DBL_MAX;

  for (size_t i = 0; i < ctx->nentries; i++) {
    if (p[i].x < minx)
      minx = p[i].x;
    if (p[i].x > maxx)
      maxx = p[i].x;
    if (p[i].y < miny)
      miny = p[i].y;
    if (p[i].y > maxy)
      maxy = p[i].y;
  }

  ctx->minx = minx;
  ctx->maxx = maxx;
  ctx->miny = miny;
  ctx->maxy = maxy;
}

void compute_normalized(ctx_t *ctx) {
  ctx->normed = calloc(1, ctx->nentries * sizeof(pair_t));
  for (size_t i = 0; i < ctx->nentries; i++) {
    ctx->normed[i].x =
        (ctx->entries[i].x - ctx->minx) / (ctx->maxx - ctx->minx);
    ctx->normed[i].y =
        (ctx->entries[i].y - ctx->miny) / (ctx->maxy - ctx->miny);
  }
}

double estimate_t0(double t0, double t1, pair_t *entries, size_t m, double r) {

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

double estimate_t1(double t0, double t1, pair_t *entries, size_t m, double r) {

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

void train(int n, ctx_t *ctx) {

  // TODO init with values?
  double tmp0 = 0, tmp1 = 0;
  // TODO remove?
  ctx->r = 0.1;
  printf("Start           t0 = %lf, t1 = %lf\n", ctx->t0, ctx->t1);
  for (int i = 0; i < n; i++) {
    tmp0 = estimate_t0(ctx->t0, ctx->t1, ctx->normed, ctx->nentries, ctx->r);
    tmp1 = estimate_t1(ctx->t0, ctx->t1, ctx->normed, ctx->nentries, ctx->r);
    ctx->t0 -= tmp0;
    ctx->t1 -= tmp1;
    printf("Epoch %4d/%-4d t0 = %lf, t1 = %lf\n", i + 1, n, ctx->t0, ctx->t1);
  }

  double xr = ctx->maxx - ctx->minx;
  double yr = ctx->maxy - ctx->miny; 
  printf("Normalized theta0(%lf) and theta1(%lf)\n",ctx->t0, ctx->t1);
  //denormalize t1
  ctx->t1 = ctx->t1 * yr/xr;
  //denormalize t0
  ctx->t0 = ctx->miny + yr * ctx->t0 - ctx->t1 * ctx->minx;
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

  printf("Context: x = %s, y = %s, nentries = %lu\n", ctx.labelx, ctx.labely,
         ctx.nentries);
  printf("     %-20s %-20s\n", ctx.labelx, ctx.labely);
  for (size_t i = 0; i < ctx.nentries; i++) {
    printf("%-3lu: %-20lf %-20lf\n", i, ctx.entries[i].x, ctx.entries[i].y);
  }

  compute_range(&ctx);
  compute_normalized(&ctx);
  ctx.t0 = 0;
  ctx.t1 = 0;
  // run n epochs
  train(5000, &ctx);

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
  free(ctx.normed);
  return system("xdg-open ./result.html");
}
