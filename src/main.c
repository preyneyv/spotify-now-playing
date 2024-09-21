#include <string.h>
#include <sys/_endian.h>
#include <sys/_types/_socklen_t.h>
#include <unistd.h>

#include <chafa.h>
#include <jansson.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "http-server.h"
#include "spotify.h"
#include "term-util.h"

#define PIX_WIDTH 3
#define PIX_HEIGHT 3
#define N_CHANNELS 4

void print_test_pattern(void) {
  const guint8 pixels[PIX_WIDTH * PIX_HEIGHT * N_CHANNELS] = {
      0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff,
      0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff};

  ChafaSymbolMap *symbol_map;
  ChafaCanvasConfig *config;
  ChafaTermInfo *term_info;
  ChafaCanvas *canvas;

  ChafaCanvasMode mode;
  ChafaPixelMode pixel_mode;

  gfloat font_ratio = 0.5;
  gint cell_width = -1, cell_height = -1;
  gint width_cells, height_cells;
  TermSize term_size = get_tty_size();

  if (term_size.width_cells > 0 && term_size.height_cells > 0 &&
      term_size.width_pixels > 0 && term_size.height_pixels > 0) {
    cell_width = term_size.width_pixels / term_size.width_cells;
    cell_height = term_size.height_pixels / term_size.height_cells;
    font_ratio = (gdouble)cell_width / (gdouble)cell_height;
  }

  width_cells = 80;
  height_cells = 20;
  chafa_calc_canvas_geometry(PIX_WIDTH, PIX_HEIGHT, &width_cells, &height_cells,
                             font_ratio, TRUE, FALSE);
  detect_terminal_mode(&term_info, &mode, &pixel_mode);

  symbol_map = chafa_symbol_map_new();
  chafa_symbol_map_add_by_tags(symbol_map, CHAFA_SYMBOL_TAG_ALL);

  config = chafa_canvas_config_new();
  chafa_canvas_config_set_symbol_map(config, symbol_map);
  chafa_canvas_config_set_canvas_mode(config, mode);
  chafa_canvas_config_set_pixel_mode(config, pixel_mode);
  chafa_canvas_config_set_geometry(config, width_cells, height_cells);
  if (cell_width > 0 && cell_height > 0)
    chafa_canvas_config_set_cell_geometry(config, cell_width, cell_height);

  canvas = chafa_canvas_new(config);

  chafa_canvas_draw_all_pixels(canvas, CHAFA_PIXEL_RGBA8_UNASSOCIATED, pixels,
                               PIX_WIDTH, PIX_HEIGHT, PIX_WIDTH * N_CHANNELS);

  GString *gs = chafa_canvas_print(canvas, term_info);
  fwrite(gs->str, sizeof(char), gs->len, stdout);
  fputc('\n', stdout);
  g_string_free(gs, TRUE);

  chafa_canvas_unref(canvas);
  chafa_term_info_unref(term_info);
  chafa_canvas_config_unref(config);
  chafa_symbol_map_unref(symbol_map);
}

#define CB_PORT 3000
int http_server_cb(HttpRequest *req, HttpResponse *res, void *usr) {
  res->code = 404;
  sprintf(res->body, "you said %s to %s", http_request_query_get(req, "param"),
          req->path);
  return 0;
}

int main(void) {
  printf("waiting...\n");
  http_server_run_until(3000, http_server_cb, NULL);
  printf("yay\n");
}
