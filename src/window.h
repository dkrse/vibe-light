#ifndef WINDOW_H
#define WINDOW_H

#include <adwaita.h>
#include <gtksourceview/gtksource.h>
#include <vte/vte.h>
#include <webkit/webkit.h>
#include "settings.h"

typedef struct _VibeWindow VibeWindow;
struct _VibeWindow {
    GtkApplicationWindow *window;
    GtkNotebook          *notebook;

    /* Tab 1: File Browser (left) + File Viewer (right) */
    GtkListBox           *file_list;
    GtkLabel             *path_label;
    GtkSourceView        *file_view;
    GtkSourceBuffer      *file_buffer;
    guint                 intensity_idle_id;
    gboolean              file_modified;
    GtkWidget            *search_bar;
    GtkEntry             *search_entry;
    GtkSourceSearchContext *search_ctx;
    char                  current_dir[2048];
    char                  root_dir[2048];

    /* Tab 2: Terminal */
    VteTerminal          *terminal;

    /* Tab 3: AI-model (output view + prompt input) */
    WebKitWebView        *ai_webview;
    GString              *ai_conversation_md;   /* accumulated raw markdown for re-render */
    GtkLabel             *ai_status_label;      /* model info */
    GSubprocess          *ai_proc;           /* running claude process */
    GString              *ai_response_buf;   /* accumulates full JSON response */
    GDataInputStream     *ai_stream;         /* streaming: line-by-line stdout reader */
    char                  ai_session_id[128]; /* session ID for --resume */
    gint64                ai_session_start;  /* real time when session was created */
    int                   ai_session_turns;  /* number of prompts sent in this session */
    char                 *ai_last_prompt;     /* last sent prompt text (for deferred logging) */

    /* AI working directory (from terminal CWD) */
    char                  ai_cwd[2048];

    /* Token / stats tracking */
    int                   ai_input_tokens;    /* total input tokens this session */
    int                   ai_output_tokens;   /* total output tokens this session */
    gint64                ai_start_time;      /* monotonic µs when prompt was sent */
    guint                 ai_timer_id;        /* live elapsed time update timer */
    double                ai_last_elapsed;    /* seconds for last request */
    GtkLabel             *ai_token_label;     /* token display in AI tab */

    /* SSH state (for remote terminal + file browsing) */
    char                  ssh_host[256];
    char                  ssh_user[128];
    int                   ssh_port;
    char                  ssh_key[1024];
    char                  ssh_remote_path[1024]; /* remote root, e.g. /opt */
    char                  ssh_mount[2048];       /* local sshfs mount point */

    /* Cancellable for all async operations — cancelled on destroy */
    GCancellable         *cancellable;

    /* File system watcher (local: inotify via GFileMonitor) */
    GFileMonitor         *dir_monitor;
    guint                 fs_refresh_id;        /* debounce timer */
    GFileMonitor         *file_monitor;         /* watches open file */
    guint                 file_reload_id;       /* debounce for file reload */
    char                  current_file[4096];   /* path of file in editor */

    /* SSH multiplexing (ControlMaster) */
    char                  ssh_ctl_path[512];    /* control socket path */
    char                  ssh_ctl_dir[256];     /* mkdtemp directory */

    /* Remote watching */
    GSubprocess          *inotify_proc;         /* ssh inotifywait -m */
    GDataInputStream     *inotify_stream;       /* stdout of inotifywait */
    guint                 remote_dir_poll_id;   /* fallback poll timer */
    gboolean              dir_poll_in_flight;   /* guard: poll running */
    guint                 remote_file_poll_id;  /* file poll timer */
    gboolean              file_poll_in_flight;  /* guard: poll running */
    guint32               remote_dir_hash;      /* hash of last ls output */
    gint64                remote_file_mtime;    /* mtime of open file */

    /* Tab 3: Prompt input */
    GtkTextView          *prompt_view;
    GtkTextBuffer        *prompt_buffer;
    GtkTextTag           *prompt_intensity_tag;

    /* Status bar */
    GtkLabel             *status_label;
    GtkWidget            *sftp_box;          /* SFTP indicator + disconnect btn */
    GtkLabel             *sftp_label;
    GtkButton            *sftp_disconnect_btn;
    GtkLabel             *cursor_label;

    /* Git status */
    GHashTable           *git_status;       /* rel_path -> GINT status char */
    char                  git_root[4096];   /* absolute git root path */
    gboolean              git_status_in_flight;
    guint                 git_poll_id;          /* periodic git status timer */

    /* Toast notifications */
    AdwToastOverlay      *toast_overlay;

    VibeSettings          settings;
    GtkCssProvider       *css_provider;

    /* Active inline edit context (rename / new file) — NULL when idle */
    void                 *inline_edit_ctx;
};

VibeWindow *vibe_window_new(GtkApplication *app);
void vibe_window_apply_settings(VibeWindow *win);
void vibe_window_open_directory(VibeWindow *win, const char *path);
void vibe_window_set_root_directory(VibeWindow *win, const char *path);
void vibe_window_disconnect_sftp(VibeWindow *win);
void vibe_window_toast(VibeWindow *win, const char *message);
void vibe_window_refresh_current_dir(VibeWindow *win);
void vibe_window_switch_ai_mode(VibeWindow *win);

#endif
