#include "linreg.h"
#include <stdio.h>
#include <stdlib.h>

#define NGRAD 5

double predict(ctx_t *ctx, double x) { return ctx->t0 + ctx->t1 * x; }

double normed(double x, double min, double max) {
  return (x - min) / (max - min);
}

int gen_graph(ctx_t *ctx) {

  FILE *f = fopen("graph.svg", "w+");
  if (f == NULL) {
    return 1;
  }

  int h = 500;
  int w = 900;
  // offset within the svg to draw
  int offx = 100;
  int offy = 50;
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

  // horizontal axis
  fprintf(f,
          "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#333333\" "
          "stroke-width=\"2\" />\n",
          offx - 3, h - offy, w, h - offy);
  // vertical axis
  fprintf(f,
          "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#333333\" "
          "stroke-width=\"2\" />\n",
          offx, 0, offx, h - offy + 3);

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
          (int)(w * normed(x1, minx, maxx)) + offx,
          h - (int)(h * normed(y1, miny, maxy)) - offy,
          // p2
          (int)(w * normed(x2, minx, maxx)) + offx,
          h - (int)(h * normed(y2, miny, maxy)) - offy);

  // text styling
  fprintf(f, "<style>.scale {font-size: 10px;}</style>\n");
  double interval = (maxx - minx) / 5.0;
  // horizontal axis scale
  for (int i = 0; i < NGRAD; i++) {

    int posx = (int)(w * normed(minx + interval * i, minx, maxx)) + offx;
    fprintf(f,
            "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#333333\" "
            "stroke-width=\"2\" />\n",
            posx, h - offy, posx, h - offy + 3);

    fprintf(f, "<text x=\"%d\" y=\"%d\" class=\"scale\">%.2e</text>", posx - 30,
            h - offy + 15, minx + interval * i);
  }
  // horizontal axis label
  fprintf(f, "<text x=\"%d\" y=\"%d\" class=\"label\">%s</text>", w / 2, h - 15,
          ctx->labelx);

  // vertical axis scale
  interval = (maxy - miny) / 5.0;
  for (int i = 0; i < NGRAD; i++) {

    int posy = (int)(h * normed(miny + interval * i, miny, maxy)) + offy;
    fprintf(f,
            "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#333333\" "
            "stroke-width=\"2\" />\n",
            offx - 3, h - posy, offx, h - posy);

    fprintf(f, "<text x=\"%d\" y=\"%d\" class=\"scale\">%.2e</text>", offx - 55,
            h - posy + 3, miny + interval * i);
  }
  // horizontal axis label
  fprintf(f,
          "<text x=\"%d\" y=\"%d\" class=\"label\" transform=\"rotate(-90 %d "
          "%d)\">%s</text>",
          25, h / 2,
          25, h / 2,
          ctx->labely);
  fprintf(f, "</svg>\n");
  return 0;
}
