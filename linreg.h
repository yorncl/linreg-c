#ifndef LINREG
#define LINREG

#include <stdlib.h>

typedef struct pair_t {
  double x;
  double y;
} pair_t;

typedef struct ctx_t {
  pair_t *entries;
  size_t nentries;
  double t0;
  double t1;
  double r;
  char labelx[256];
  char labely[256];
} ctx_t;

int gen_graph(ctx_t* ctx);

#endif

