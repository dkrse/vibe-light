#include "prompt_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <glib.h>
#include <gio/gio.h>

/* ── JSON escape ── */

char *prompt_log_json_escape(const char *s) {
    if (!s) return g_strdup("");
    GString *out = g_string_new(NULL);
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':  g_string_append(out, "\\\""); break;
            case '\\': g_string_append(out, "\\\\"); break;
            case '\n': g_string_append(out, "\\n");  break;
            case '\r': g_string_append(out, "\\r");  break;
            case '\t': g_string_append(out, "\\t");  break;
            default:
                if ((unsigned char)*p < 0x20)
                    g_string_append_printf(out, "\\u%04x", (unsigned)*p);
                else
                    g_string_append_c(out, *p);
        }
    }
    return g_string_free(out, FALSE);
}

/* ── Internal: get next ID from existing log ── */

static int find_next_id(const char *existing) {
    if (!existing) return 1;
    int max_id = 0;
    const char *p = existing;
    while ((p = strstr(p, "\"id\":")) != NULL) {
        p += 5;
        while (*p == ' ') p++;
        int v = atoi(p);
        if (v > max_id) max_id = v;
    }
    return max_id + 1;
}

/* ── Internal: get log file path ── */

static void get_log_path(char *buf, size_t bufsize, const char *root_dir) {
    snprintf(buf, bufsize, "%s/.LLM/prompts.json", root_dir);
}

static void ensure_llm_dir(const char *root_dir) {
    char dir[2200];
    snprintf(dir, sizeof(dir), "%s/.LLM", root_dir);
    g_mkdir_with_parents(dir, 0755);
}

/* ── Internal: append a JSON entry to the log file ── */

static void append_entry(const char *root_dir, const char *json_entry) {
    ensure_llm_dir(root_dir);

    char jpath[2200];
    get_log_path(jpath, sizeof(jpath), root_dir);

    char *existing = NULL;
    gsize len = 0;
    gboolean have_file = g_file_get_contents(jpath, &existing, &len, NULL);

    FILE *f = fopen(jpath, "w");
    if (!f) { g_free(existing); return; }
    fchmod(fileno(f), 0600);

    if (have_file && len > 2) {
        char *last = strrchr(existing, ']');
        if (last) {
            fwrite(existing, 1, (size_t)(last - existing), f);
            fprintf(f, ",\n  %s\n]\n", json_entry);
        } else {
            fprintf(f, "[\n  %s\n]\n", json_entry);
        }
    } else {
        fprintf(f, "[\n  %s\n]\n", json_entry);
    }

    fclose(f);
    g_free(existing);
}

/* ── Internal: get ISO 8601 timestamp ── */

static char *get_timestamp(void) {
    GDateTime *now = g_date_time_new_now_local();
    char *ts = g_date_time_format(now, "%Y-%m-%dT%H:%M:%S");
    g_date_time_unref(now);
    return ts;
}

/* ── Public API ── */

void prompt_log_input(const char *root_dir,
                      const char *session_id,
                      const char *model,
                      const char *prompt_text) {
    if (!root_dir || !root_dir[0]) return;

    char jpath[2200];
    get_log_path(jpath, sizeof(jpath), root_dir);

    /* Read existing to get next ID */
    char *existing = NULL;
    g_file_get_contents(jpath, &existing, NULL, NULL);
    int id = find_next_id(existing);
    g_free(existing);

    char *ts = get_timestamp();
    char *escaped = prompt_log_json_escape(prompt_text);
    char *esc_session = prompt_log_json_escape(session_id ? session_id : "");
    char *esc_model = prompt_log_json_escape(model ? model : "");

    /* Build JSON entry */
    char *entry = g_strdup_printf(
        "{"
        "\"id\": %d, "
        "\"type\": \"input\", "
        "\"timestamp\": \"%s\", "
        "\"session\": \"%s\", "
        "\"model\": \"%s\", "
        "\"text\": \"%s\""
        "}",
        id, ts, esc_session, esc_model, escaped);

    append_entry(root_dir, entry);

    g_free(entry);
    g_free(ts);
    g_free(escaped);
    g_free(esc_session);
    g_free(esc_model);
}

void prompt_log_output(const char *root_dir,
                       const char *session_id,
                       const char *model,
                       const char *response_text,
                       int input_tokens,
                       int output_tokens,
                       double elapsed_seconds) {
    if (!root_dir || !root_dir[0]) return;

    char jpath[2200];
    get_log_path(jpath, sizeof(jpath), root_dir);

    /* Read existing to get next ID */
    char *existing = NULL;
    g_file_get_contents(jpath, &existing, NULL, NULL);
    int id = find_next_id(existing);
    g_free(existing);

    char *ts = get_timestamp();
    char *escaped = prompt_log_json_escape(response_text);
    char *esc_session = prompt_log_json_escape(session_id ? session_id : "");
    char *esc_model = prompt_log_json_escape(model ? model : "");

    /* Elapsed as locale-safe string */
    char elapsed_str[32];
    snprintf(elapsed_str, sizeof(elapsed_str), "%.2f", elapsed_seconds);
    /* Fix locale comma → dot */
    for (char *p = elapsed_str; *p; p++) if (*p == ',') *p = '.';

    /* Build JSON entry */
    char *entry = g_strdup_printf(
        "{"
        "\"id\": %d, "
        "\"type\": \"output\", "
        "\"timestamp\": \"%s\", "
        "\"session\": \"%s\", "
        "\"model\": \"%s\", "
        "\"input_tokens\": %d, "
        "\"output_tokens\": %d, "
        "\"total_tokens\": %d, "
        "\"elapsed_seconds\": %s, "
        "\"text\": \"%s\""
        "}",
        id, ts, esc_session, esc_model,
        input_tokens, output_tokens,
        input_tokens + output_tokens,
        elapsed_str, escaped);

    append_entry(root_dir, entry);

    g_free(entry);
    g_free(ts);
    g_free(escaped);
    g_free(esc_session);
    g_free(esc_model);
}
