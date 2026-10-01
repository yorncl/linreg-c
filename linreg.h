#ifndef LINREG
#define LINREG

#include <stdlib.h>
#include <stdio.h>

typedef struct pair_t {
  double x;
  double y;
} pair_t;

typedef struct ctx_t {
  pair_t *entries;
  pair_t *normed;
  size_t nentries;
  double t0;
  double t1;
  double r;
  char labelx[256];
  char labely[256];

  // range
  double minx;
  double maxx;
  double miny;
  double maxy;
} ctx_t;

int gen_graph(ctx_t* ctx);
int parse_data(ctx_t *ctx, FILE *f);

#endif

