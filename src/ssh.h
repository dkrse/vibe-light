#ifndef SSH_H
#define SSH_H

#include <glib.h>
#include <gio/gio.h>

/* Forward declaration — full definition in window.h */
typedef struct _VibeWindow VibeWindow;

/* ── SSH utility functions ── */

/* Check if a path refers to a remote SFTP mount */
gboolean ssh_path_is_remote(const char *path);

/* Convert local mount path to remote path.
   e.g. /tmp/vibe-light-sftp-123-user@host/subdir -> /opt/subdir */
const char *ssh_to_remote_path(const char *ssh_mount, const char *ssh_remote_path,
                               const char *local_path, char *buf, size_t buflen);

/* Build SSH base argv from explicit parameters.
   Caller must add command args + trailing NULL, then g_ptr_array_unref(). */
GPtrArray *ssh_argv_from_params(const char *host, const char *user,
                                int port, const char *key,
                                const char *ctl_path);

/* Build SSH base argv from VibeWindow state.
   Caller must add command args + trailing NULL, then g_ptr_array_unref(). */
GPtrArray *ssh_argv_new(const char *host, const char *user, int port,
                        const char *key, const char *ctl_path);

/* Start SSH ControlMaster for multiplexed connections */
void ssh_ctl_start(char *ctl_dir, size_t ctl_dir_size,
                   char *ctl_path, size_t ctl_path_size,
                   const char *host, const char *user, int port, const char *key);

/* Stop SSH ControlMaster and clean up socket */
void ssh_ctl_stop(char *ctl_path, char *ctl_dir,
                  const char *host, const char *user);

/* Run SSH command synchronously with argv (no shell -- immune to injection).
   Returns TRUE on success. out_stdout/out_len may be NULL.
   Appends+removes trailing NULL internally. */
gboolean ssh_spawn_sync(GPtrArray *argv, char **out_stdout, gsize *out_len);

/* Simple djb2 hash for comparing directory listing output */
guint32 ssh_djb2_hash(const char *str);

/* Read remote file via SSH cat using GSubprocess (binary-safe).
   Returns TRUE on success. Caller must g_free(*out_contents). */
gboolean ssh_cat_file(const char *host, const char *user, int port,
                      const char *key, const char *ctl_path,
                      const char *ssh_mount, const char *ssh_remote_path,
                      const char *local_path,
                      char **out_contents, gsize *out_len,
                      gsize max_file_size);

/* ── Remote polling thread functions (for use with GTask) ── */

typedef struct {
    void       *win;     /* VibeWindow* — opaque to avoid circular include */
    char        remote_path[4096];
    char        local_dir[4096];
    char        ssh_host[256];
    char        ssh_user[128];
    int         ssh_port;
    char        ssh_key[1024];
    char        ssh_ctl_path[256];
} SshDirPollCtx;

typedef struct {
    void       *win;
    char        remote_path[4096];
    char        local_path[4096];
    char        ssh_host[256];
    char        ssh_user[128];
    int         ssh_port;
    char        ssh_key[1024];
    char        ssh_ctl_path[256];
} SshFilePollCtx;

typedef struct {
    void       *win;
    char        remote_dir[4096];
    char        ssh_host[256];
    char        ssh_user[128];
    int         ssh_port;
    char        ssh_key[1024];
    char        ssh_ctl_path[512];
} SshInotifyCheckCtx;

/* GTask thread functions — return results via g_task_return_int/boolean */
void ssh_dir_poll_thread(GTask *task, gpointer src, gpointer data,
                         GCancellable *cancel);
void ssh_file_poll_thread(GTask *task, gpointer src, gpointer data,
                          GCancellable *cancel);
void ssh_inotify_check_thread(GTask *task, gpointer src, gpointer data,
                              GCancellable *cancel);

#endif
