#include "term-util.h"
#include <stdio.h>

void detect_terminal_mode(ChafaTermInfo **term_info_out,
                          ChafaCanvasMode *mode_out,
                          ChafaPixelMode *pixel_mode_out) {
  ChafaCanvasMode mode;
  ChafaPixelMode pixel_mode;
  ChafaTermInfo *term_info;

  gchar **envp;
  envp = g_get_environ();
  term_info = chafa_term_db_detect(chafa_term_db_get_default(), envp);
  g_strfreev(envp);

  // Determine what the best possible image quality setting is.
  if (chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_BEGIN_SIXELS)) {
    pixel_mode = CHAFA_PIXEL_MODE_SIXELS;
    mode = CHAFA_CANVAS_MODE_TRUECOLOR;
  } else if (chafa_term_info_have_seq(
                 term_info, CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1)) {
    pixel_mode = CHAFA_PIXEL_MODE_KITTY;
    mode = CHAFA_CANVAS_MODE_TRUECOLOR;
  } else if (chafa_term_info_have_seq(term_info,
                                      CHAFA_TERM_SEQ_BEGIN_ITERM2_IMAGE)) {
    pixel_mode = CHAFA_PIXEL_MODE_ITERM2;
    mode = CHAFA_CANVAS_MODE_TRUECOLOR;
  } else {
    pixel_mode = CHAFA_PIXEL_MODE_SYMBOLS;

    if (chafa_term_info_have_seq(term_info,
                                 CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT) &&
        chafa_term_info_have_seq(term_info,
                                 CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT) &&
        chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT))
      mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    else if (chafa_term_info_have_seq(term_info,
                                      CHAFA_TERM_SEQ_SET_COLOR_FGBG_256) &&
             chafa_term_info_have_seq(term_info,
                                      CHAFA_TERM_SEQ_SET_COLOR_FG_256) &&
             chafa_term_info_have_seq(term_info,
                                      CHAFA_TERM_SEQ_SET_COLOR_BG_256)

    )
      mode = CHAFA_CANVAS_MODE_INDEXED_240;
    else if (chafa_term_info_have_seq(term_info,
                                      CHAFA_TERM_SEQ_SET_COLOR_FGBG_16) &&
             chafa_term_info_have_seq(term_info,
                                      CHAFA_TERM_SEQ_SET_COLOR_FG_16) &&
             chafa_term_info_have_seq(term_info,
                                      CHAFA_TERM_SEQ_SET_COLOR_BG_16))
      mode = CHAFA_CANVAS_MODE_INDEXED_16;
    else if (chafa_term_info_have_seq(term_info,
                                      CHAFA_TERM_SEQ_INVERT_COLORS) &&
             chafa_term_info_have_seq(term_info,
                                      CHAFA_TERM_SEQ_RESET_ATTRIBUTES))
      mode = CHAFA_CANVAS_MODE_FGBG_BGFG;
    else
      mode = CHAFA_CANVAS_MODE_FGBG;
  }

  *term_info_out = term_info;
  *mode_out = mode;
  *pixel_mode_out = pixel_mode;
}

TermSize get_tty_size() {
  TermSize term_size;
  term_size.width_cells = term_size.height_cells = term_size.width_pixels =
      term_size.height_pixels = -1;

  struct winsize w;
  gboolean have_winsize = FALSE;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0 ||
      ioctl(STDERR_FILENO, TIOCGWINSZ, &w) >= 0 ||
      ioctl(STDIN_FILENO, TIOCGWINSZ, &w) >= 0)
    have_winsize = TRUE;

  if (have_winsize) {
    term_size.width_cells = w.ws_col;
    term_size.height_cells = w.ws_row;
    term_size.width_pixels = w.ws_xpixel;
    term_size.height_pixels = w.ws_ypixel;
  }

  if (term_size.width_cells <= 0)
    term_size.width_cells = -1;
  if (term_size.height_cells <= 0)
    term_size.height_cells = -1;

  if (term_size.width_pixels <= 0 || term_size.height_pixels <= 0) {
    term_size.width_pixels = -1;
    term_size.height_pixels = -1;
  }

  return term_size;
}

inline void term_cursor_save() { printf(TERM_ESC "7"); }
inline void term_rel_clear() { printf(TERM_CURSOR_RESTORE TERM_ESC "[0J"); }
inline void term_rel_cursor(int x, int y) {
  printf(TERM_CURSOR_RESTORE TERM_ESC "[%dB" TERM_ESC "[%dG", y, x);
}

struct term_dimensions get_term_dimensions() {
  TermSize term_size = get_tty_size();
  struct term_dimensions out = {.h_cell = term_size.height_cells,
                                .w_cell = term_size.width_cells,
                                .w_px = term_size.width_pixels,
                                .h_px = term_size.height_pixels,
                                .font_ratio = 0.5,
                                .cw_px = -1,
                                .ch_px = -1};

  if (term_size.width_cells > 0 && term_size.height_cells > 0 &&
      term_size.width_pixels > 0 && term_size.height_pixels > 0) {
    out.cw_px = term_size.width_pixels / term_size.width_cells;
    out.ch_px = term_size.height_pixels / term_size.height_cells;
    out.font_ratio = (gdouble)out.cw_px / (gdouble)out.ch_px;
  }
  return out;
}

// void term_print_image(unsigned char *buffer, int width, int height, int
// stride,
//                       int t_width, int t_height) {
//   ChafaSymbolMap *symbol_map;
//   ChafaCanvasConfig *config;
//   ChafaTermInfo *term_info;
//   ChafaCanvas *canvas;

//   struct term_dimensions dim = get_term_dimensions();

//   int width_cells = t_width;
//   int height_cells = t_height;
//   chafa_calc_canvas_geometry(width, height, &width_cells, &height_cells,
//                              dim.font_ratio, TRUE, FALSE);

//   symbol_map = chafa_symbol_map_new();
//   chafa_symbol_map_add_by_tags(symbol_map, CHAFA_SYMBOL_TAG_ASCII);

//   config = chafa_canvas_config_new();
//   chafa_canvas_config_set_symbol_map(config, symbol_map);
//   chafa_canvas_config_set_canvas_mode(config, mode);
//   chafa_canvas_config_set_pixel_mode(config, pixel_mode);
//   chafa_canvas_config_set_geometry(config, width_cells, height_cells);
//   if (dim.ch_px > 0 && dim.cw_px > 0)
//     chafa_canvas_config_set_cell_geometry(config, dim.cw_px, dim.ch_px);

//   canvas = chafa_canvas_new(config);
//   chafa_canvas_draw_all_pixels(canvas, CHAFA_PIXEL_RGB8, buffer, width,
//   height,
//                                stride);

//   GString *gs = chafa_canvas_print(canvas, term_info);
//   fwrite(gs->str, sizeof(char), gs->len, stdout);
//   fputc('\n', stdout);
//   g_string_free(gs, TRUE);

//   // chafa_term_info_emit

//   chafa_canvas_unref(canvas);
//   chafa_term_info_unref(term_info);
//   chafa_canvas_config_unref(config);
//   chafa_symbol_map_unref(symbol_map);
// }
