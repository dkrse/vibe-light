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
    gboolean show_line_numbers;
    gboolean highlight_current_line;
    gboolean wrap_lines;

    /* Terminal */
    char terminal_font[256];
    int terminal_font_size;

    /* Prompt */
    char prompt_font[256];
    int prompt_font_size;
    gboolean prompt_send_enter;
    gboolean prompt_switch_terminal;

    /* Window */
    int window_width;
    int window_height;
    char last_directory[2048];
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
