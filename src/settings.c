#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <sys/stat.h>

static void ensure_config_dir(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/.config/vibe-light", g_get_home_dir());
    g_mkdir_with_parents(path, 0755);
}

char *settings_get_config_path(void) {
    static char path[1024];
    snprintf(path, sizeof(path), "%s/.config/vibe-light/settings.conf", g_get_home_dir());
    return path;
}

static double parse_double(const char *val) {
    /* locale-safe: parse "0.82" or "0,82" regardless of LC_NUMERIC */
    char buf[64];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char *p = buf; *p; p++)
        if (*p == ',') *p = '.';

    /* manual parse to avoid locale-dependent atof/strtod */
    double result = 0.0;
    int sign = 1;
    const char *p = buf;
    if (*p == '-') { sign = -1; p++; }
    while (*p >= '0' && *p <= '9') { result = result * 10 + (*p - '0'); p++; }
    if (*p == '.') {
        p++;
        double frac = 0.1;
        while (*p >= '0' && *p <= '9') { result += (*p - '0') * frac; frac *= 0.1; p++; }
    }
    return result * sign;
}

void settings_load(VibeSettings *s) {
    strncpy(s->theme, "system", sizeof(s->theme));
    s->font_intensity = 1.0;
    s->line_spacing = 1.0;

    strncpy(s->gui_font, "Monospace", sizeof(s->gui_font));
    s->gui_font_size = 14;

    strncpy(s->browser_font, "Monospace", sizeof(s->browser_font));
    s->browser_font_size = 14;

    strncpy(s->editor_font, "Monospace", sizeof(s->editor_font));
    s->editor_font_size = 14;
    s->show_line_numbers = FALSE;
    s->highlight_current_line = TRUE;
    s->wrap_lines = TRUE;

    strncpy(s->terminal_font, "Monospace", sizeof(s->terminal_font));
    s->terminal_font_size = 14;

    strncpy(s->prompt_font, "Monospace", sizeof(s->prompt_font));
    s->prompt_font_size = 14;
    s->prompt_send_enter = FALSE;
    s->prompt_switch_terminal = TRUE;

    s->window_width = 900;
    s->window_height = 600;
    s->last_directory[0] = '\0';

    FILE *f = fopen(settings_get_config_path(), "r");
    if (!f) return;

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line, *val = eq + 1;

        if (strcmp(key, "theme") == 0) strncpy(s->theme, val, sizeof(s->theme) - 1);
        else if (strcmp(key, "font_intensity") == 0) s->font_intensity = parse_double(val);
        else if (strcmp(key, "line_spacing") == 0) s->line_spacing = parse_double(val);

        else if (strcmp(key, "gui_font") == 0) strncpy(s->gui_font, val, sizeof(s->gui_font) - 1);
        else if (strcmp(key, "gui_font_size") == 0) s->gui_font_size = atoi(val);

        else if (strcmp(key, "browser_font") == 0) strncpy(s->browser_font, val, sizeof(s->browser_font) - 1);
        else if (strcmp(key, "browser_font_size") == 0) s->browser_font_size = atoi(val);

        else if (strcmp(key, "editor_font") == 0) strncpy(s->editor_font, val, sizeof(s->editor_font) - 1);
        else if (strcmp(key, "editor_font_size") == 0) s->editor_font_size = atoi(val);
        else if (strcmp(key, "show_line_numbers") == 0) s->show_line_numbers = atoi(val);
        else if (strcmp(key, "highlight_current_line") == 0) s->highlight_current_line = atoi(val);
        else if (strcmp(key, "wrap_lines") == 0) s->wrap_lines = atoi(val);

        else if (strcmp(key, "terminal_font") == 0) strncpy(s->terminal_font, val, sizeof(s->terminal_font) - 1);
        else if (strcmp(key, "terminal_font_size") == 0) s->terminal_font_size = atoi(val);

        else if (strcmp(key, "prompt_font") == 0) strncpy(s->prompt_font, val, sizeof(s->prompt_font) - 1);
        else if (strcmp(key, "prompt_font_size") == 0) s->prompt_font_size = atoi(val);
        else if (strcmp(key, "prompt_send_enter") == 0) s->prompt_send_enter = atoi(val);
        else if (strcmp(key, "prompt_switch_terminal") == 0) s->prompt_switch_terminal = atoi(val);

        else if (strcmp(key, "window_width") == 0) s->window_width = atoi(val);
        else if (strcmp(key, "window_height") == 0) s->window_height = atoi(val);
        else if (strcmp(key, "last_directory") == 0) strncpy(s->last_directory, val, sizeof(s->last_directory) - 1);

        /* backwards compat */
        else if (strcmp(key, "gui_font_intensity") == 0 ||
                 strcmp(key, "editor_font_intensity") == 0 ||
                 strcmp(key, "terminal_font_intensity") == 0 ||
                 strcmp(key, "prompt_font_intensity") == 0 ||
                 strcmp(key, "browser_font_intensity") == 0) {
            s->font_intensity = parse_double(val);
        }
        else if (strcmp(key, "font") == 0) {
            strncpy(s->gui_font, val, sizeof(s->gui_font) - 1);
            strncpy(s->browser_font, val, sizeof(s->browser_font) - 1);
            strncpy(s->editor_font, val, sizeof(s->editor_font) - 1);
            strncpy(s->terminal_font, val, sizeof(s->terminal_font) - 1);
            strncpy(s->prompt_font, val, sizeof(s->prompt_font) - 1);
        }
        else if (strcmp(key, "font_size") == 0) {
            int v = atoi(val);
            s->gui_font_size = v; s->browser_font_size = v;
            s->editor_font_size = v; s->terminal_font_size = v;
            s->prompt_font_size = v;
        }
    }
    fclose(f);

    /* clamp intensity to valid range */
    if (s->font_intensity < 0.3) s->font_intensity = 0.3;
    if (s->font_intensity > 1.0) s->font_intensity = 1.0;
}

void settings_save(const VibeSettings *s) {
    ensure_config_dir();
    FILE *f = fopen(settings_get_config_path(), "w");
    if (!f) return;

    char *prev = setlocale(LC_NUMERIC, NULL);
    char saved_locale[64] = "";
    if (prev) strncpy(saved_locale, prev, sizeof(saved_locale) - 1);
    setlocale(LC_NUMERIC, "C");

    fprintf(f, "theme=%s\n", s->theme);
    fprintf(f, "font_intensity=%.2f\n", s->font_intensity);
    fprintf(f, "line_spacing=%.1f\n", s->line_spacing);

    fprintf(f, "gui_font=%s\n", s->gui_font);
    fprintf(f, "gui_font_size=%d\n", s->gui_font_size);

    fprintf(f, "browser_font=%s\n", s->browser_font);
    fprintf(f, "browser_font_size=%d\n", s->browser_font_size);

    fprintf(f, "editor_font=%s\n", s->editor_font);
    fprintf(f, "editor_font_size=%d\n", s->editor_font_size);
    fprintf(f, "show_line_numbers=%d\n", s->show_line_numbers);
    fprintf(f, "highlight_current_line=%d\n", s->highlight_current_line);
    fprintf(f, "wrap_lines=%d\n", s->wrap_lines);

    fprintf(f, "terminal_font=%s\n", s->terminal_font);
    fprintf(f, "terminal_font_size=%d\n", s->terminal_font_size);

    fprintf(f, "prompt_font=%s\n", s->prompt_font);
    fprintf(f, "prompt_font_size=%d\n", s->prompt_font_size);
    fprintf(f, "prompt_send_enter=%d\n", s->prompt_send_enter);
    fprintf(f, "prompt_switch_terminal=%d\n", s->prompt_switch_terminal);

    fprintf(f, "window_width=%d\n", s->window_width);
    fprintf(f, "window_height=%d\n", s->window_height);
    fprintf(f, "last_directory=%s\n", s->last_directory);

    setlocale(LC_NUMERIC, saved_locale);
    fclose(f);
}
