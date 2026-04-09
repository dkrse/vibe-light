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
    g_strlcpy(buf, val, sizeof(buf));
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
    g_strlcpy(s->theme, "system", sizeof(s->theme));
    s->font_intensity = 1.0;
    s->line_spacing = 1.0;

    g_strlcpy(s->gui_font, "Monospace", sizeof(s->gui_font));
    s->gui_font_size = 14;

    g_strlcpy(s->browser_font, "Monospace", sizeof(s->browser_font));
    s->browser_font_size = 14;

    g_strlcpy(s->editor_font, "Monospace", sizeof(s->editor_font));
    s->editor_font_size = 14;
    s->editor_font_weight = 400;
    s->show_line_numbers = FALSE;
    s->highlight_current_line = TRUE;
    s->wrap_lines = TRUE;
    s->show_gitignored = 0; /* 0=hide, 1=show gray */
    s->show_hidden = FALSE;

    g_strlcpy(s->terminal_font, "Monospace", sizeof(s->terminal_font));
    s->terminal_font_size = 14;

    g_strlcpy(s->prompt_font, "Monospace", sizeof(s->prompt_font));
    s->prompt_font_size = 14;
    s->prompt_send_enter = FALSE;
    s->prompt_switch_terminal = TRUE;

    s->ai_full_disk_access = FALSE;
    s->ai_tool_read = TRUE;
    s->ai_tool_edit = TRUE;
    s->ai_tool_write = TRUE;
    s->ai_tool_glob = TRUE;
    s->ai_tool_grep = TRUE;
    s->ai_tool_bash = TRUE;

    s->window_width = 900;
    s->window_height = 600;
    s->last_directory[0] = '\0';

    /* Session restore */
    s->last_file[0] = '\0';
    s->last_cursor_line = 0;
    s->last_cursor_col = 0;
    s->last_tab = 0;

    /* Keybindings defaults */
    g_strlcpy(s->key_open_folder, "<Control>o", sizeof(s->key_open_folder));
    g_strlcpy(s->key_zoom_in, "<Control>plus", sizeof(s->key_zoom_in));
    g_strlcpy(s->key_zoom_out, "<Control>minus", sizeof(s->key_zoom_out));
    g_strlcpy(s->key_tab_files, "<Alt>1", sizeof(s->key_tab_files));
    g_strlcpy(s->key_tab_terminal, "<Alt>2", sizeof(s->key_tab_terminal));
    g_strlcpy(s->key_tab_ai, "<Alt>3", sizeof(s->key_tab_ai));
    g_strlcpy(s->key_quit, "<Control>q", sizeof(s->key_quit));

    FILE *f = fopen(settings_get_config_path(), "r");
    if (!f) return;

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line, *val = eq + 1;

        if (strcmp(key, "theme") == 0) g_strlcpy(s->theme, val, sizeof(s->theme));
        else if (strcmp(key, "font_intensity") == 0) s->font_intensity = parse_double(val);
        else if (strcmp(key, "line_spacing") == 0) s->line_spacing = parse_double(val);

        else if (strcmp(key, "gui_font") == 0) g_strlcpy(s->gui_font, val, sizeof(s->gui_font));
        else if (strcmp(key, "gui_font_size") == 0) s->gui_font_size = atoi(val);

        else if (strcmp(key, "browser_font") == 0) g_strlcpy(s->browser_font, val, sizeof(s->browser_font));
        else if (strcmp(key, "browser_font_size") == 0) s->browser_font_size = atoi(val);

        else if (strcmp(key, "editor_font") == 0) g_strlcpy(s->editor_font, val, sizeof(s->editor_font));
        else if (strcmp(key, "editor_font_size") == 0) s->editor_font_size = atoi(val);
        else if (strcmp(key, "editor_font_weight") == 0) s->editor_font_weight = atoi(val);
        else if (strcmp(key, "show_line_numbers") == 0) s->show_line_numbers = atoi(val);
        else if (strcmp(key, "highlight_current_line") == 0) s->highlight_current_line = atoi(val);
        else if (strcmp(key, "wrap_lines") == 0) s->wrap_lines = atoi(val);
        else if (strcmp(key, "show_gitignored") == 0) s->show_gitignored = atoi(val);
        else if (strcmp(key, "show_hidden") == 0) s->show_hidden = atoi(val);

        else if (strcmp(key, "terminal_font") == 0) g_strlcpy(s->terminal_font, val, sizeof(s->terminal_font));
        else if (strcmp(key, "terminal_font_size") == 0) s->terminal_font_size = atoi(val);

        else if (strcmp(key, "prompt_font") == 0) g_strlcpy(s->prompt_font, val, sizeof(s->prompt_font));
        else if (strcmp(key, "prompt_font_size") == 0) s->prompt_font_size = atoi(val);
        else if (strcmp(key, "prompt_send_enter") == 0) s->prompt_send_enter = atoi(val);
        else if (strcmp(key, "prompt_switch_terminal") == 0) s->prompt_switch_terminal = atoi(val);


        else if (strcmp(key, "ai_full_disk_access") == 0) s->ai_full_disk_access = atoi(val);
        else if (strcmp(key, "ai_tool_read") == 0) s->ai_tool_read = atoi(val);
        else if (strcmp(key, "ai_tool_edit") == 0) s->ai_tool_edit = atoi(val);
        else if (strcmp(key, "ai_tool_write") == 0) s->ai_tool_write = atoi(val);
        else if (strcmp(key, "ai_tool_glob") == 0) s->ai_tool_glob = atoi(val);
        else if (strcmp(key, "ai_tool_grep") == 0) s->ai_tool_grep = atoi(val);
        else if (strcmp(key, "ai_tool_bash") == 0) s->ai_tool_bash = atoi(val);

        else if (strcmp(key, "window_width") == 0) s->window_width = atoi(val);
        else if (strcmp(key, "window_height") == 0) s->window_height = atoi(val);
        else if (strcmp(key, "last_directory") == 0) g_strlcpy(s->last_directory, val, sizeof(s->last_directory));

        /* session restore */
        else if (strcmp(key, "last_file") == 0) g_strlcpy(s->last_file, val, sizeof(s->last_file));
        else if (strcmp(key, "last_cursor_line") == 0) s->last_cursor_line = atoi(val);
        else if (strcmp(key, "last_cursor_col") == 0) s->last_cursor_col = atoi(val);
        else if (strcmp(key, "last_tab") == 0) s->last_tab = atoi(val);

        /* keybindings */
        else if (strcmp(key, "key_open_folder") == 0) g_strlcpy(s->key_open_folder, val, sizeof(s->key_open_folder));
        else if (strcmp(key, "key_zoom_in") == 0) g_strlcpy(s->key_zoom_in, val, sizeof(s->key_zoom_in));
        else if (strcmp(key, "key_zoom_out") == 0) g_strlcpy(s->key_zoom_out, val, sizeof(s->key_zoom_out));
        else if (strcmp(key, "key_tab_files") == 0) g_strlcpy(s->key_tab_files, val, sizeof(s->key_tab_files));
        else if (strcmp(key, "key_tab_terminal") == 0) g_strlcpy(s->key_tab_terminal, val, sizeof(s->key_tab_terminal));
        else if (strcmp(key, "key_tab_ai") == 0) g_strlcpy(s->key_tab_ai, val, sizeof(s->key_tab_ai));
        else if (strcmp(key, "key_quit") == 0) g_strlcpy(s->key_quit, val, sizeof(s->key_quit));

        /* backwards compat */
        else if (strcmp(key, "gui_font_intensity") == 0 ||
                 strcmp(key, "editor_font_intensity") == 0 ||
                 strcmp(key, "terminal_font_intensity") == 0 ||
                 strcmp(key, "prompt_font_intensity") == 0 ||
                 strcmp(key, "browser_font_intensity") == 0) {
            s->font_intensity = parse_double(val);
        }
        else if (strcmp(key, "font") == 0) {
            g_strlcpy(s->gui_font, val, sizeof(s->gui_font));
            g_strlcpy(s->browser_font, val, sizeof(s->browser_font));
            g_strlcpy(s->editor_font, val, sizeof(s->editor_font));
            g_strlcpy(s->terminal_font, val, sizeof(s->terminal_font));
            g_strlcpy(s->prompt_font, val, sizeof(s->prompt_font));
        }
        else if (strcmp(key, "font_size") == 0) {
            int v = atoi(val);
            s->gui_font_size = v; s->browser_font_size = v;
            s->editor_font_size = v; s->terminal_font_size = v;
            s->prompt_font_size = v;
        }
    }
    fclose(f);

    /* clamp / fix invalid values */
    if (s->font_intensity < 0.3) s->font_intensity = 0.3;
    if (s->font_intensity > 1.0) s->font_intensity = 1.0;
    if (s->line_spacing < 1.0) s->line_spacing = 1.0;

    /* restore defaults for empty/zero font fields */
    if (!s->gui_font[0])      g_strlcpy(s->gui_font, "Monospace", sizeof(s->gui_font));
    if (s->gui_font_size < 6)  s->gui_font_size = 14;
    if (!s->browser_font[0])   g_strlcpy(s->browser_font, "Monospace", sizeof(s->browser_font));
    if (s->browser_font_size < 6) s->browser_font_size = 14;
    if (!s->editor_font[0])    g_strlcpy(s->editor_font, "Monospace", sizeof(s->editor_font));
    if (s->editor_font_size < 6)  s->editor_font_size = 14;
    if (s->editor_font_weight < 100) s->editor_font_weight = 400;
    if (!s->terminal_font[0])  g_strlcpy(s->terminal_font, "Monospace", sizeof(s->terminal_font));
    if (s->terminal_font_size < 6) s->terminal_font_size = 14;
    if (!s->prompt_font[0])    g_strlcpy(s->prompt_font, "Monospace", sizeof(s->prompt_font));
    if (s->prompt_font_size < 6)  s->prompt_font_size = 14;
}

static char *connections_get_path(void) {
    static char path[1024];
    snprintf(path, sizeof(path), "%s/.config/vibe-light/connections.conf", g_get_home_dir());
    return path;
}

void connections_load(SftpConnections *c) {
    memset(c, 0, sizeof(*c));
    FILE *f = fopen(connections_get_path(), "r");
    if (!f) return;

    char line[4096];
    int idx = -1;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '[') {
            idx++;
            if (idx >= MAX_CONNECTIONS) break;
            c->count = idx + 1;
            /* parse name from [name] */
            char *end = strchr(line, ']');
            if (end) *end = '\0';
            g_strlcpy(c->items[idx].name, line + 1, sizeof(c->items[idx].name));
            c->items[idx].port = 22;
            continue;
        }
        if (idx < 0 || idx >= MAX_CONNECTIONS) continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line, *val = eq + 1;
        SftpConnection *s = &c->items[idx];
        if (strcmp(key, "host") == 0) g_strlcpy(s->host, val, sizeof(s->host));
        else if (strcmp(key, "port") == 0) s->port = atoi(val);
        else if (strcmp(key, "user") == 0) g_strlcpy(s->user, val, sizeof(s->user));
        else if (strcmp(key, "remote_path") == 0) g_strlcpy(s->remote_path, val, sizeof(s->remote_path));
        else if (strcmp(key, "use_key") == 0) s->use_key = atoi(val);
        else if (strcmp(key, "key_path") == 0) g_strlcpy(s->key_path, val, sizeof(s->key_path));
    }
    fclose(f);
}

void connections_save(const SftpConnections *c) {
    ensure_config_dir();
    FILE *f = fopen(connections_get_path(), "w");
    if (!f) return;
    /* restrict permissions — contains hostnames, usernames, key paths */
    fchmod(fileno(f), 0600);
    for (int i = 0; i < c->count; i++) {
        const SftpConnection *s = &c->items[i];
        fprintf(f, "[%s]\n", s->name);
        fprintf(f, "host=%s\n", s->host);
        fprintf(f, "port=%d\n", s->port);
        fprintf(f, "user=%s\n", s->user);
        fprintf(f, "remote_path=%s\n", s->remote_path);
        fprintf(f, "use_key=%d\n", s->use_key);
        fprintf(f, "key_path=%s\n", s->key_path);
        fprintf(f, "\n");
    }
    fclose(f);
}

void settings_save(const VibeSettings *s) {
    ensure_config_dir();
    FILE *f = fopen(settings_get_config_path(), "w");
    if (!f) return;
    fchmod(fileno(f), 0600);

    char *prev = setlocale(LC_NUMERIC, NULL);
    char saved_locale[64] = "";
    if (prev) g_strlcpy(saved_locale, prev, sizeof(saved_locale));
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
    fprintf(f, "editor_font_weight=%d\n", s->editor_font_weight);
    fprintf(f, "show_line_numbers=%d\n", s->show_line_numbers);
    fprintf(f, "highlight_current_line=%d\n", s->highlight_current_line);
    fprintf(f, "wrap_lines=%d\n", s->wrap_lines);
    fprintf(f, "show_gitignored=%d\n", s->show_gitignored);
    fprintf(f, "show_hidden=%d\n", s->show_hidden);

    fprintf(f, "terminal_font=%s\n", s->terminal_font);
    fprintf(f, "terminal_font_size=%d\n", s->terminal_font_size);

    fprintf(f, "prompt_font=%s\n", s->prompt_font);
    fprintf(f, "prompt_font_size=%d\n", s->prompt_font_size);
    fprintf(f, "prompt_send_enter=%d\n", s->prompt_send_enter);
    fprintf(f, "prompt_switch_terminal=%d\n", s->prompt_switch_terminal);


    fprintf(f, "ai_full_disk_access=%d\n", s->ai_full_disk_access);
    fprintf(f, "ai_tool_read=%d\n", s->ai_tool_read);
    fprintf(f, "ai_tool_edit=%d\n", s->ai_tool_edit);
    fprintf(f, "ai_tool_write=%d\n", s->ai_tool_write);
    fprintf(f, "ai_tool_glob=%d\n", s->ai_tool_glob);
    fprintf(f, "ai_tool_grep=%d\n", s->ai_tool_grep);
    fprintf(f, "ai_tool_bash=%d\n", s->ai_tool_bash);

    fprintf(f, "window_width=%d\n", s->window_width);
    fprintf(f, "window_height=%d\n", s->window_height);
    fprintf(f, "last_directory=%s\n", s->last_directory);

    /* session restore */
    fprintf(f, "last_file=%s\n", s->last_file);
    fprintf(f, "last_cursor_line=%d\n", s->last_cursor_line);
    fprintf(f, "last_cursor_col=%d\n", s->last_cursor_col);
    fprintf(f, "last_tab=%d\n", s->last_tab);

    /* keybindings */
    fprintf(f, "key_open_folder=%s\n", s->key_open_folder);
    fprintf(f, "key_zoom_in=%s\n", s->key_zoom_in);
    fprintf(f, "key_zoom_out=%s\n", s->key_zoom_out);
    fprintf(f, "key_tab_files=%s\n", s->key_tab_files);
    fprintf(f, "key_tab_terminal=%s\n", s->key_tab_terminal);
    fprintf(f, "key_tab_ai=%s\n", s->key_tab_ai);
    fprintf(f, "key_quit=%s\n", s->key_quit);

    setlocale(LC_NUMERIC, saved_locale);
    fclose(f);
}
