#include <sys/_endian.h>
#include <sys/_types/_socklen_t.h>
#include <time.h>
#include <unistd.h>

#include <chafa.h>
#include <jansson.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

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

struct ui_ctx {
  ChafaTermInfo *term_info;
  ChafaPixelMode pixel_mode;
  ChafaCanvasMode canvas_mode;
  ChafaSymbolMap *symbol_map;
};

void ui_setup(struct ui_ctx *ctx) {
  detect_terminal_mode(&ctx->term_info, &ctx->canvas_mode, &ctx->pixel_mode);
  ctx->symbol_map = chafa_symbol_map_new();
  chafa_symbol_map_add_by_tags(ctx->symbol_map, CHAFA_SYMBOL_TAG_ASCII);

  char buf[CHAFA_TERM_SEQ_LENGTH_MAX * 2];
  char *p = buf;
  p = chafa_term_info_emit_clear(ctx->term_info, p);
  p = chafa_term_info_emit_cursor_to_top_left(ctx->term_info, p);
  fwrite(buf, 1, p - buf, stdout);
}

void ui_teardown(struct ui_ctx *ctx) {
  chafa_term_info_unref(ctx->term_info);
  chafa_symbol_map_unref(ctx->symbol_map);
}

void ui_render(struct ui_ctx *ctx, SpotifyCurrentlyPlaying *playing) {
  static char last_rendered[64] = "";

  char buf[CHAFA_TERM_SEQ_LENGTH_MAX * 3];
  char *p;

  p = buf;
  p = chafa_term_info_emit_clear(ctx->term_info, p);
  fwrite(buf, 1, p - buf, stdout);

  p = buf;
  p = chafa_term_info_emit_cursor_to_pos(ctx->term_info, p, 17, 1);
  fwrite(buf, 1, p - buf, stdout);
  printf(term_c_bold("%s") "\n", playing->track_name);

  p = buf;
  p = chafa_term_info_emit_cursor_to_pos(ctx->term_info, p, 17, 2);
  fwrite(buf, 1, p - buf, stdout);
  printf(term_c_dim("%s") "\n", playing->album_name);

  p = buf;
  p = chafa_term_info_emit_cursor_to_pos(ctx->term_info, p, 17, 5);
  fwrite(buf, 1, p - buf, stdout);
  printf("%s\n", playing->artists[0]);

  p = buf;
  p = chafa_term_info_emit_cursor_to_top_left(ctx->term_info, p);
  fwrite(buf, 1, p - buf, stdout);

  ChafaCanvasConfig *config;
  ChafaCanvas *canvas;

  struct term_dimensions dim = get_term_dimensions();

  int width_cells = 14;
  int height_cells = 7;
  chafa_calc_canvas_geometry(playing->album_cover->width,
                             playing->album_cover->height, &width_cells,
                             &height_cells, dim.font_ratio, FALSE, FALSE);

  config = chafa_canvas_config_new();
  chafa_canvas_config_set_symbol_map(config, ctx->symbol_map);
  chafa_canvas_config_set_canvas_mode(config, ctx->canvas_mode);
  chafa_canvas_config_set_pixel_mode(config, ctx->pixel_mode);
  chafa_canvas_config_set_geometry(config, width_cells, height_cells);
  if (dim.ch_px > 0 && dim.cw_px > 0)
    chafa_canvas_config_set_cell_geometry(config, dim.cw_px, dim.ch_px);

  canvas = chafa_canvas_new(config);
  chafa_canvas_draw_all_pixels(
      canvas, CHAFA_PIXEL_RGB8, playing->album_cover->pixels,
      playing->album_cover->width, playing->album_cover->height,
      playing->album_cover->width * 3);

  GString *gs = chafa_canvas_print(canvas, ctx->term_info);
  fwrite(gs->str, sizeof(char), gs->len, stdout);
  fputc('\n', stdout);
  g_string_free(gs, TRUE);

  chafa_canvas_unref(canvas);
  chafa_canvas_config_unref(config);
}

int main(void) {
  struct ui_ctx ctx = {0};
  ui_setup(&ctx);

  SpotifyAuth *auth = spotify_auth_new_from_oauth();
  if (!auth)
    return EXIT_FAILURE;

  u_char it = 0;
  SpotifyCurrentlyPlaying *playing = NULL;
  while (1) {
    if (it == 0) {
      // term_rel_cursor(0, 0);
      // printf(term_c_dim(".\n.\n.\n"));
      spotify_currently_playing_free(playing);
      playing = spotify_currently_playing_get(auth);
      // term_rel_clear();
      // term_print_image(
      //     playing->album_cover->pixels, playing->album_cover->width,
      //     playing->album_cover->height, playing->album_cover->width * 3, 12,
      //     6);
      // break;
    }
    it = (it + 1) % 4;

    ui_render(&ctx, playing);

    sleep(1);
  }
  ui_teardown(&ctx);
  return 0;
}
