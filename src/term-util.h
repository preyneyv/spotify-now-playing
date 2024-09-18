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

#endif /* __SNP_TERM_UTIL_H__ */
