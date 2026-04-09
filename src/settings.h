#ifndef SETTINGS_H
#define SETTINGS_H

#include <gtk/gtk.h>

typedef struct {
    /* Global */
    char theme[64];
    double font_intensity;
    double line_spacing;

    /* GUI */
    char gui_font[256];
    int gui_font_size;

    /* File Browser */
    char browser_font[256];
    int browser_font_size;

    /* Editor (file viewer) */
    char editor_font[256];
    int editor_font_size;
    int editor_font_weight;
    gboolean show_line_numbers;
    gboolean highlight_current_line;
    gboolean wrap_lines;
    int      show_gitignored;  /* 0=hide, 1=show gray */
    gboolean show_hidden;      /* show dotfiles */

    /* Terminal */
    char terminal_font[256];
    int terminal_font_size;

    /* Prompt */
    char prompt_font[256];
    int prompt_font_size;
    gboolean prompt_send_enter;
    gboolean prompt_switch_terminal;

    /* AI Model */
    gboolean ai_full_disk_access;
    gboolean ai_tool_read;
    gboolean ai_tool_edit;
    gboolean ai_tool_write;
    gboolean ai_tool_glob;
    gboolean ai_tool_grep;
    gboolean ai_tool_bash;

    /* Window */
    int window_width;
    int window_height;
    char last_directory[2048];

    /* Session restore */
    char last_file[4096];
    int  last_cursor_line;
    int  last_cursor_col;
    int  last_tab;

    /* Keybindings (GTK accelerator format, e.g. "<Control>o") */
    char key_open_folder[64];
    char key_zoom_in[64];
    char key_zoom_out[64];
    char key_tab_files[64];
    char key_tab_terminal[64];
    char key_tab_ai[64];
    char key_quit[64];
} VibeSettings;

/* SFTP/SSH connections */
#define MAX_CONNECTIONS 32

typedef struct {
    char name[128];
    char host[256];
    int  port;
    char user[128];
    char remote_path[1024];
    gboolean use_key;       /* TRUE = private key, FALSE = password */
    char key_path[1024];    /* path to private key file */
} SftpConnection;

typedef struct {
    SftpConnection items[MAX_CONNECTIONS];
    int count;
} SftpConnections;

void settings_load(VibeSettings *s);
void settings_save(const VibeSettings *s);
char *settings_get_config_path(void);

void connections_load(SftpConnections *c);
void connections_save(const SftpConnections *c);

#endif
