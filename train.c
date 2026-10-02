#include "linreg.h"
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NEPOCHS 10000

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

  // case where all the data points have equal coordinates
  // widen the range artificially a bit
  if (minx == maxx) {
    fprintf(stderr, "Ahem, all x have the same coordinates, how am I supposed "
                    "to find a slope?\n");
    exit(1);
  }
  // Just add 5, don't care about order of magnitude
  // the user is asking for it at this point
  if (miny == maxy) {
    miny -= 5;
    maxy += 5;
  }
  ctx->minx = minx;
  ctx->maxx = maxx;
  ctx->miny = miny;
  ctx->maxy = maxy;

  // range check
  if (!isfinite(maxx - minx) || !isfinite(maxy - miny)) {
    fprintf(stderr, "Data range is too large\n");
    exit(1);
  }
}

void compute_normalized(ctx_t *ctx) {
  ctx->normed = calloc(1, ctx->nentries * sizeof(pair_t));
  if (ctx->normed == NULL) {
    fprintf(stderr, "Allocation failed!\n");
    exit(2);
  }
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

  double tmp0 = 0, tmp1 = 0;
  ctx->r = 0.1;
  printf("Start           t0 = %lf, t1 = %lf\n", ctx->t0, ctx->t1);
  for (int i = 0; i < n; i++) {
    tmp0 = estimate_t0(ctx->t0, ctx->t1, ctx->normed, ctx->nentries, ctx->r);
    tmp1 = estimate_t1(ctx->t0, ctx->t1, ctx->normed, ctx->nentries, ctx->r);
    ctx->t0 -= tmp0;
    ctx->t1 -= tmp1;
    if ((i + 1) % 500 == 0)
      printf("Epoch %4d/%-4d t0 = %lf, t1 = %lf\n", i + 1, n, ctx->t0, ctx->t1);
  }

  double xr = ctx->maxx - ctx->minx;
  double yr = ctx->maxy - ctx->miny;
  printf("Normalized theta0(%lf) and theta1(%lf)\n", ctx->t0, ctx->t1);
  // denormalize t1
  ctx->t1 = ctx->t1 * yr / xr;
  // denormalize t0
  ctx->t0 = ctx->miny + yr * ctx->t0 - ctx->t1 * ctx->minx;
}

int save_variables(double t0, double t1) {
  if (!isfinite(t0) || !isfinite(t1)) {
    fprintf(stderr,
            "Resulting t0 or t1 isn't finite, aborting (t0=%lf, t1=%lf)\n", t0,
            t1);
    exit(1);
  }
  printf("Saving theta0(%lf) and theta1(%lf)\n", t0, t1);
  FILE *f = fopen("vars.csv", "w+");
  if (f == NULL)
    return 1;
  int r = fprintf(f, "%.17g,%.17g\n", t0, t1);
  if (r < 0)
    return 1;
  fclose(f);
  return 0;
}

int main(int ac, char **av) {

  if (ac != 2) {
    usage();
    exit(1);
  }

  FILE *f = fopen(av[1], "r");
  if (f == NULL) {
    perror("Could not open data file");
    exit(1);
  }

  // parse the data
  ctx_t ctx;
  if (parse_data(&ctx, f)) {
    perror("Error while parsing data");
    exit(1);
  }

  printf("Context: x = %s, y = %s, nentries = %lu\n", ctx.labelx, ctx.labely,
         ctx.nentries);
  printf("     %-20s %-20s\n", ctx.labelx, ctx.labely);
  for (size_t i = 0; i < ctx.nentries; i++) {
    printf("%-3lu: %-20lf %-20lf\n", i, ctx.entries[i].x, ctx.entries[i].y);
  }

  if (ctx.nentries == 0) {
    fprintf(stderr, "What are we doing with our life? Nothing apparently, add "
                    "some entries\n");
    exit(1);
  }
  compute_range(&ctx);
  compute_normalized(&ctx);
  ctx.t0 = 0;
  ctx.t1 = 0;
  // run n epochs
  train(NEPOCHS, &ctx);

  if (save_variables(ctx.t0, ctx.t1)) {
    perror("could not save to file");
    exit(2);
  }

  if (gen_graph(&ctx)) {
    perror("Error generating graph file");
    exit(1);
  }

  fclose(f);
  free(ctx.entries);
  free(ctx.normed);
  return 0;
}
