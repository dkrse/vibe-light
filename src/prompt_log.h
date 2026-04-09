#ifndef PROMPT_LOG_H
#define PROMPT_LOG_H

#include <glib.h>

/* Prompt log entry types */
typedef enum {
    PROMPT_ENTRY_INPUT,   /* user prompt */
    PROMPT_ENTRY_OUTPUT   /* AI response */
} PromptEntryType;

/* Log a user prompt (input) */
void prompt_log_input(const char *root_dir,
                      const char *session_id,
                      const char *model,
                      const char *prompt_text);

/* Log an AI response (output) with token usage */
void prompt_log_output(const char *root_dir,
                       const char *session_id,
                       const char *model,
                       const char *response_text,
                       int input_tokens,
                       int output_tokens,
                       double elapsed_seconds);

/* Escape a string for JSON. Caller must g_free(). */
char *prompt_log_json_escape(const char *s);

#endif
