#ifndef __SNP_TERM_UTIL_H__
#define __SNP_TERM_UTIL_H__

#include <chafa.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct {
  gint width_cells, height_cells;
  gint width_pixels, height_pixels;
} TermSize;

void detect_terminal_mode(ChafaTermInfo **term_info_out,
                          ChafaCanvasMode *mode_out,
                          ChafaPixelMode *pixel_mode_out);

TermSize get_tty_size();

#define TERM_ESC "\033"
#define TERM_CURSOR_RESTORE TERM_ESC "8"
#define term_c_bold(str) TERM_ESC "[1m" str TERM_ESC "[22m"
#define term_c_dim(str) TERM_ESC "[2m" str TERM_ESC "[22m"
#define term_c_ital(str) TERM_ESC "[3m" str TERM_ESC "[23m"
#define term_c_undr(str) TERM_ESC "[4m" str TERM_ESC "[24m"

void term_cursor_save();
void term_rel_clear();
void term_rel_cursor(int x, int y);
void term_print_image(unsigned char *buffer, int width, int height, int stride,
                      int t_width, int t_height);

struct term_dimensions {
  int w_cell;
  int h_cell;

  int w_px;
  int h_px;

  int cw_px;
  int ch_px;

  float font_ratio;
};
struct term_dimensions get_term_dimensions();

#endif /* __SNP_TERM_UTIL_H__ */
