#include "linreg.h"
#include <float.h>
#include <stdio.h>

void get_range(double *minx, double *maxx, double *miny, double *maxy,
               ctx_t *ctx) {
  pair_t *p = ctx->entries;

  *minx = DBL_MAX;
  *maxx = DBL_MIN;
  *miny = DBL_MAX;
  *maxy = DBL_MIN;
  for (size_t i = 0; i < ctx->nentries; i++) {
    if (p[i].x < *minx)
      *minx = p[i].x;
    if (p[i].x > *maxx)
      *maxx = p[i].x;
    if (p[i].y < *miny)
      *miny = p[i].y;
    if (p[i].y > *maxy)
      *maxy = p[i].y;
  }
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

  fprintf(f,
          "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d "
          "%d\" width=\"100%%\" height=\"100%%\">\n",
          w, h);

  double minx, maxx, miny, maxy;
  get_range(&minx, &maxx, &miny, &maxy, ctx);
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

    double relx = (p[i].x - minx) / (maxx - minx);
    double rely = (p[i].y - miny) / (maxy - miny);
    fprintf(f, "<circle cx=\"%d\" cy=\"%d\" r=\"4\" fill=\"#2563eb\" />\n",
            (int)(w * relx) + offx, (int)(h * rely) - offy);
  }

  fprintf(f,
          "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#333333\" "
          "stroke-width=\"2\" />",
          0, h - offy, w, h - offy);
  fprintf(f,
          "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#333333\" "
          "stroke-width=\"2\" />",
          offx, 0, offx, h);

  fprintf(f, "</svg>\n");
  return 0;
}

//<!-- Background -->  <rect width="500" height="300" fill="#ffffff" />  <!--
// Axes -->  <line x1="50" y1="250" x2="450" y2="250" stroke="#333333"
// stroke-width="2" />  <line x1="50" y1="50" x2="50" y2="250" stroke="#333333"
// stroke-width="2" />  <!-- Data Points -->  <circle cx="50" cy="200" r="4"
// fill="#2563eb" />  <circle cx="130" cy="160" r="4" fill="#2563eb" />  <circle
// cx="210" cy="180" r="4" fill="#2563eb" />  <circle cx="290" cy="90" r="4"
// fill="#2563eb" />  <circle cx="370" cy="120" r="4" fill="#2563eb" />  <circle
// cx="450" cy="70" r="4" fill="#2563eb" /></svg>
