#include "linreg.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

double predict(ctx_t *ctx, double x) {
  return ctx->t0 + ctx->t1 * x;
}

double normed(double x, double min, double max) {
  return (x - min) / (max - min);
}

int gen_graph(ctx_t *ctx) {

  FILE *f = fopen("graph.svg", "w+");
  if (f == NULL) {
    return 1;
  }

  int h = 300;
  int w = 500;
  // offset within the svg to draw
  int offx = 20;
  int offy = 20;
  double minx = ctx->minx;
  double maxx = ctx->maxx;
  double miny = ctx->miny;
  double maxy = ctx->maxy;

  fprintf(f,
          "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d "
          "%d\" width=\"100%%\" height=\"100%%\">\n",
          w, h);

  printf("Range x: [ min = %lf, max = %lf ]\n", minx, maxx);
  printf("Range y: [ min = %lf, max = %lf ]\n", miny, maxy);

  // extending the range a bit so some points aren't hidden out of the rendering
  // area
  double m = 0.15;
  minx -= m * (maxx - minx);
  maxx += m * (maxx - minx);
  miny -= m * (maxy - miny);
  maxy += m * (maxy - miny);

  pair_t *p = ctx->entries;
  for (size_t i = 0; i < ctx->nentries; i++) {

    fprintf(f, "<circle cx=\"%d\" cy=\"%d\" r=\"4\" fill=\"#2563eb\" />\n",
            (int)(w * normed(p[i].x, minx, maxx)) + offx,
            h - (int)(h * normed(p[i].y, miny, maxy)) - offy);
  }

  fprintf(f,
          "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#333333\" "
          "stroke-width=\"2\" />\n",
          0, h - offy, w, h - offy);
  fprintf(f,
          "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#333333\" "
          "stroke-width=\"2\" />\n",
          offx, 0, offx, h);

  // double max = minx < maxx ? maxx : minx;
  double x1 = ctx->minx;
  double y1 = predict(ctx, x1);
  double x2 = ctx->maxx;
  double y2 = predict(ctx, x2);

  printf("p1 %lf %lf\n", x1, y1);
  printf("p2 %lf %lf\n", x2, y2);
  fprintf(f,
          "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#ff0000\" "
          "stroke-width=\"3\" />\n",
          // p1
          (int)((double)w * normed(x1, minx, maxx)) + offx,
          h - (int)((double)h * normed(y1, miny, maxy)) - offy,
          // p2
          (int)((double)w * normed(x2, minx, maxx)) + offx,
          h - (int)((double)h * normed(y2, miny, maxy)) - offy);

  fprintf(f, "</svg>\n");
  return 0;
}
