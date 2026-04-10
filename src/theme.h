#ifndef THEME_H
#define THEME_H

#include "window.h"

typedef struct {
    const char *id;
    const char *name;
    const char *fg;
    const char *bg;
} ThemeDef;

#define N_CUSTOM_THEMES 10

extern const ThemeDef custom_themes[N_CUSTOM_THEMES];

gboolean is_dark_theme(const char *theme);
void build_theme_css(char *buf, size_t bufsize, const char *fg, const char *bg);
void apply_theme(VibeWindow *win);
void apply_terminal_colors(VibeWindow *win);
void apply_font_intensity(VibeWindow *win);

#endif
