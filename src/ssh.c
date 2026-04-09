#define _DEFAULT_SOURCE
#include "ssh.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* ── Path helpers ── */

gboolean ssh_path_is_remote(const char *path) {
    return (strstr(path, "/vibe-light-sftp-") != NULL ||
            strstr(path, "/vibe-sftp-") != NULL);
}

const char *ssh_to_remote_path(const char *ssh_mount, const char *ssh_remote_path,
                               const char *local_path, char *buf, size_t buflen) {
    size_t mlen = strlen(ssh_mount);
    const char *suffix = local_path + mlen;
    snprintf(buf, buflen, "%s%s", ssh_remote_path, suffix);
    return buf;
}

/* ── SSH argv builders ── */

GPtrArray *ssh_argv_from_params(const char *host, const char *user,
                                int port, const char *key,
                                const char *ctl_path) {
    GPtrArray *av = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(av, g_strdup("ssh"));
    g_ptr_array_add(av, g_strdup("-p"));
    g_ptr_array_add(av, g_strdup_printf("%d", port));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("StrictHostKeyChecking=accept-new"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("BatchMode=yes"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ConnectTimeout=5"));
    if (ctl_path && ctl_path[0]) {
        g_ptr_array_add(av, g_strdup("-o"));
        g_ptr_array_add(av, g_strdup_printf("ControlPath=%s", ctl_path));
    }
    if (key && key[0]) {
        g_ptr_array_add(av, g_strdup("-i"));
        g_ptr_array_add(av, g_strdup(key));
    }
    g_ptr_array_add(av, g_strdup_printf("%s@%s", user, host));
    return av;
}

GPtrArray *ssh_argv_new(const char *host, const char *user, int port,
                        const char *key, const char *ctl_path) {
    GPtrArray *av = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(av, g_strdup("ssh"));
    g_ptr_array_add(av, g_strdup("-p"));
    g_ptr_array_add(av, g_strdup_printf("%d", port));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("StrictHostKeyChecking=accept-new"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("BatchMode=yes"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ConnectTimeout=10"));
    if (ctl_path && ctl_path[0]) {
        g_ptr_array_add(av, g_strdup("-o"));
        g_ptr_array_add(av, g_strdup_printf("ControlPath=%s", ctl_path));
    }
    if (key && key[0]) {
        g_ptr_array_add(av, g_strdup("-i"));
        g_ptr_array_add(av, g_strdup(key));
    }
    g_ptr_array_add(av, g_strdup_printf("%s@%s", user, host));
    return av;
}

/* ── SSH ControlMaster lifecycle ── */

void ssh_ctl_start(char *ctl_dir, size_t ctl_dir_size,
                   char *ctl_path, size_t ctl_path_size,
                   const char *host, const char *user, int port, const char *key) {
    const char *runtime = g_get_user_runtime_dir();
    snprintf(ctl_dir, ctl_dir_size, "%s/vibe-ssh-XXXXXX", runtime);
    if (!mkdtemp(ctl_dir)) {
        ctl_dir[0] = '\0';
        ctl_path[0] = '\0';
        return;
    }
    snprintf(ctl_path, ctl_path_size, "%s/ctl", ctl_dir);

    GPtrArray *av = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(av, g_strdup("ssh"));
    g_ptr_array_add(av, g_strdup("-p"));
    g_ptr_array_add(av, g_strdup_printf("%d", port));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("StrictHostKeyChecking=accept-new"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("BatchMode=yes"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ConnectTimeout=10"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ControlMaster=yes"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup_printf("ControlPath=%s", ctl_path));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ControlPersist=60"));
    if (key && key[0]) {
        g_ptr_array_add(av, g_strdup("-i"));
        g_ptr_array_add(av, g_strdup(key));
    }
    g_ptr_array_add(av, g_strdup_printf("%s@%s", user, host));
    g_ptr_array_add(av, g_strdup("-fN"));
    g_ptr_array_add(av, NULL);

    g_spawn_sync(NULL, (char **)av->pdata, NULL,
                 G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    g_ptr_array_unref(av);
}

void ssh_ctl_stop(char *ctl_path, char *ctl_dir,
                  const char *host, const char *user) {
    if (!ctl_path[0]) return;

    GPtrArray *av = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(av, g_strdup("ssh"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup_printf("ControlPath=%s", ctl_path));
    g_ptr_array_add(av, g_strdup("-O"));
    g_ptr_array_add(av, g_strdup("exit"));
    g_ptr_array_add(av, g_strdup_printf("%s@%s", user, host));
    g_ptr_array_add(av, NULL);

    g_spawn_sync(NULL, (char **)av->pdata, NULL,
                 G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    g_ptr_array_unref(av);

    unlink(ctl_path);
    ctl_path[0] = '\0';
    if (ctl_dir[0]) {
        rmdir(ctl_dir);
        ctl_dir[0] = '\0';
    }
}

/* ── SSH command execution ── */

gboolean ssh_spawn_sync(GPtrArray *argv, char **out_stdout, gsize *out_len) {
    g_ptr_array_add(argv, NULL);

    char *stdout_buf = NULL;
    GError *err = NULL;
    gint status = 0;

    gboolean ok = g_spawn_sync(
        NULL, (char **)argv->pdata, NULL,
        G_SPAWN_SEARCH_PATH,
        NULL, NULL, &stdout_buf, NULL, &status, &err);

    /* remove trailing NULL so caller can reuse argv */
    g_ptr_array_set_size(argv, argv->len - 1);

    if (!ok) {
        if (err) g_error_free(err);
        return FALSE;
    }
    if (!g_spawn_check_wait_status(status, NULL)) {
        g_free(stdout_buf);
        return FALSE;
    }

    if (out_len)
        *out_len = stdout_buf ? strlen(stdout_buf) : 0;

    if (out_stdout)
        *out_stdout = stdout_buf;
    else
        g_free(stdout_buf);

    return TRUE;
}

/* ── Utility ── */

guint32 ssh_djb2_hash(const char *str) {
    guint32 h = 5381;
    for (; *str; str++)
        h = ((h << 5) + h) + (unsigned char)*str;
    return h;
}

/* ── Remote file reading ── */

gboolean ssh_cat_file(const char *host, const char *user, int port,
                      const char *key, const char *ctl_path,
                      const char *ssh_mount, const char *ssh_remote_path,
                      const char *local_path,
                      char **out_contents, gsize *out_len,
                      gsize max_file_size) {
    char remote[4096];
    ssh_to_remote_path(ssh_mount, ssh_remote_path, local_path, remote, sizeof(remote));

    GPtrArray *av = ssh_argv_new(host, user, port, key, ctl_path);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("cat"));
    g_ptr_array_add(av, g_strdup(remote));
    g_ptr_array_add(av, NULL);

    GError *err = NULL;
    GSubprocess *proc = g_subprocess_newv(
        (const char * const *)av->pdata,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &err);
    g_ptr_array_unref(av);

    if (!proc) {
        if (err) g_error_free(err);
        return FALSE;
    }

    GBytes *stdout_bytes = NULL;
    gboolean ok = g_subprocess_communicate(proc, NULL, NULL, &stdout_bytes, NULL, &err);

    if (!ok || !g_subprocess_get_successful(proc)) {
        if (stdout_bytes) g_bytes_unref(stdout_bytes);
        g_object_unref(proc);
        if (err) g_error_free(err);
        return FALSE;
    }

    gsize len = 0;
    const char *data = g_bytes_get_data(stdout_bytes, &len);

    if (len > max_file_size) {
        g_bytes_unref(stdout_bytes);
        g_object_unref(proc);
        *out_contents = g_strdup("(file too large)");
        *out_len = strlen(*out_contents);
        return TRUE;
    }

    *out_contents = g_malloc(len + 1);
    if (data && len) memcpy(*out_contents, data, len);
    (*out_contents)[len] = '\0';
    *out_len = len;

    g_bytes_unref(stdout_bytes);
    g_object_unref(proc);
    return TRUE;
}

/* ── GTask thread functions for remote polling ── */

void ssh_dir_poll_thread(GTask *task, gpointer src, gpointer data,
                         GCancellable *cancel) {
    (void)src; (void)cancel;
    SshDirPollCtx *ctx = data;

    GPtrArray *av = ssh_argv_from_params(ctx->ssh_host, ctx->ssh_user,
                                          ctx->ssh_port, ctx->ssh_key,
                                          ctx->ssh_ctl_path);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("ls"));
    g_ptr_array_add(av, g_strdup("-1pA"));
    g_ptr_array_add(av, g_strdup(ctx->remote_path));

    char *stdout_buf = NULL;
    if (ssh_spawn_sync(av, &stdout_buf, NULL) && stdout_buf) {
        guint32 h = ssh_djb2_hash(stdout_buf);
        g_task_return_int(task, (gint64)h);
        g_free(stdout_buf);
    } else {
        g_task_return_int(task, 0);
    }
    g_ptr_array_unref(av);
}

void ssh_file_poll_thread(GTask *task, gpointer src, gpointer data,
                          GCancellable *cancel) {
    (void)src; (void)cancel;
    SshFilePollCtx *ctx = data;

    GPtrArray *av = ssh_argv_from_params(ctx->ssh_host, ctx->ssh_user,
                                          ctx->ssh_port, ctx->ssh_key,
                                          ctx->ssh_ctl_path);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("stat"));
    g_ptr_array_add(av, g_strdup("-c"));
    g_ptr_array_add(av, g_strdup("%Y"));
    g_ptr_array_add(av, g_strdup(ctx->remote_path));

    char *stdout_buf = NULL;
    if (ssh_spawn_sync(av, &stdout_buf, NULL) && stdout_buf) {
        gint64 mtime = g_ascii_strtoll(stdout_buf, NULL, 10);
        g_task_return_int(task, mtime);
        g_free(stdout_buf);
    } else {
        g_task_return_int(task, 0);
    }
    g_ptr_array_unref(av);
}

void ssh_inotify_check_thread(GTask *task, gpointer src, gpointer data,
                               GCancellable *cancel) {
    (void)src; (void)cancel;
    SshInotifyCheckCtx *ctx = data;

    GPtrArray *av = ssh_argv_from_params(ctx->ssh_host, ctx->ssh_user,
                                          ctx->ssh_port, ctx->ssh_key,
                                          ctx->ssh_ctl_path);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("which"));
    g_ptr_array_add(av, g_strdup("inotifywait"));

    gboolean has_it = ssh_spawn_sync(av, NULL, NULL);
    g_ptr_array_unref(av);

    g_task_return_boolean(task, has_it);
}
