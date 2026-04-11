#include <adwaita.h>
#include <webkit/webkit.h>
#include <string.h>
#include <math.h>
#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>
#include "ai.h"
#include "ssh.h"
#include "prompt_log.h"
#include "theme.h"

extern void update_status_bar(VibeWindow *win);
extern GtkWidget *vibe_dialog_new(VibeWindow *win, const char *title, int width, int height);

/* Convert LaTeX expression to Unicode approximation */
char *latex_to_unicode(const char *latex) {
    GString *out = g_string_new(NULL);
    /* Common LaTeX command → Unicode mappings */
    static const struct { const char *cmd; const char *uni; } syms[] = {
        {"\\sum", "∑"}, {"\\prod", "∏"}, {"\\int", "∫"},
        {"\\infty", "∞"}, {"\\alpha", "α"}, {"\\beta", "β"},
        {"\\gamma", "γ"}, {"\\delta", "δ"}, {"\\epsilon", "ε"},
        {"\\theta", "θ"}, {"\\lambda", "λ"}, {"\\mu", "μ"},
        {"\\pi", "π"}, {"\\sigma", "σ"}, {"\\phi", "φ"}, {"\\omega", "ω"},
        {"\\Delta", "Δ"}, {"\\Sigma", "Σ"}, {"\\Pi", "Π"}, {"\\Omega", "Ω"},
        {"\\pm", "±"}, {"\\times", "×"}, {"\\div", "÷"},
        {"\\neq", "≠"}, {"\\leq", "≤"}, {"\\geq", "≥"},
        {"\\approx", "≈"}, {"\\equiv", "≡"},
        {"\\leftarrow", "←"}, {"\\rightarrow", "→"},
        {"\\Leftarrow", "⇐"}, {"\\Rightarrow", "⇒"},
        {"\\partial", "∂"}, {"\\nabla", "∇"},
        {"\\forall", "∀"}, {"\\exists", "∃"},
        {"\\in", "∈"}, {"\\notin", "∉"},
        {"\\subset", "⊂"}, {"\\subseteq", "⊆"},
        {"\\cup", "∪"}, {"\\cap", "∩"},
        {"\\cdot", "·"}, {"\\ldots", "…"}, {"\\cdots", "⋯"},
        {"\\sqrt", "√"}, {"\\langle", "⟨"}, {"\\rangle", "⟩"},
        {"\\to", "→"},
    };
    /* Superscript/subscript digit maps */
    static const char *sup_digits[] = {"⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹"};
    static const char *sub_digits[] = {"₀","₁","₂","₃","₄","₅","₆","₇","₈","₉"};
    static const char *sup_letters = "ᵃᵇᶜᵈᵉᶠᵍʰⁱʲᵏˡᵐⁿᵒᵖ qʳˢᵗᵘᵛʷˣʸᶻ";
    static const char *sub_letters = "ₐ   ₑ  ₕᵢⱼₖₗₘₙₒₚ ᵣₛₜᵤᵥ ₓ  ";

    const char *p = latex;
    while (*p) {
        /* Skip \left, \right — just decorators */
        if (strncmp(p, "\\left", 5) == 0) { p += 5; continue; }
        if (strncmp(p, "\\right", 6) == 0) { p += 6; continue; }

        /* \frac{num}{den} — render as (num)/(den) */
        if (strncmp(p, "\\frac{", 6) == 0) {
            p += 5; /* skip \frac, land on { */
            /* Extract numerator */
            p++; /* skip { */
            int depth = 1;
            const char *start = p;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                if (depth > 0) p++;
            }
            char *num = g_strndup(start, (gsize)(p - start));
            if (*p == '}') p++;
            /* Extract denominator */
            char *den = NULL;
            if (*p == '{') {
                p++;
                depth = 1;
                start = p;
                while (*p && depth > 0) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                    if (depth > 0) p++;
                }
                den = g_strndup(start, (gsize)(p - start));
                if (*p == '}') p++;
            }
            char *num_r = latex_to_unicode(num);
            g_string_append_c(out, '(');
            g_string_append(out, num_r);
            g_string_append_c(out, ')');
            if (den) {
                char *den_r = latex_to_unicode(den);
                g_string_append_c(out, '/');
                g_string_append_c(out, '(');
                g_string_append(out, den_r);
                g_string_append_c(out, ')');
                g_free(den_r);
                g_free(den);
            }
            g_free(num_r);
            g_free(num);
            continue;
        }
        /* \text{...} and \mathrm{...} — render contents as plain text */
        if ((strncmp(p, "\\text{", 6) == 0) ||
            (strncmp(p, "\\mathrm{", 8) == 0) ||
            (strncmp(p, "\\textbf{", 8) == 0) ||
            (strncmp(p, "\\mathbf{", 8) == 0)) {
            const char *open = strchr(p, '{');
            p = open + 1;
            /* Find matching closing brace (handle nesting) */
            int depth = 1;
            const char *start = p;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                if (depth > 0) p++;
            }
            g_string_append_len(out, start, (gssize)(p - start));
            if (*p == '}') p++;
            continue;
        }
        /* Check LaTeX commands */
        if (*p == '\\' && g_ascii_isalpha(p[1])) {
            gboolean found = FALSE;
            for (size_t i = 0; i < G_N_ELEMENTS(syms); i++) {
                size_t cl = strlen(syms[i].cmd);
                if (strncmp(p, syms[i].cmd, cl) == 0 &&
                    !g_ascii_isalpha(p[cl])) {
                    g_string_append(out, syms[i].uni);
                    p += cl;
                    found = TRUE;
                    break;
                }
            }
            if (!found) {
                /* Unknown command — skip backslash, show name */
                p++;
                while (*p && g_ascii_isalpha(*p))
                    p++;
            }
            continue;
        }
        /* Superscript: ^X or ^{...} */
        if (*p == '^') {
            p++;
            const char *content; size_t clen;
            if (*p == '{') {
                p++;
                /* Find matching '}' with nesting */
                int depth = 1;
                const char *start = p;
                while (*p && depth > 0) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                    if (depth > 0) p++;
                }
                content = start; clen = (size_t)(p - start);
                if (*p == '}') p++;
            } else {
                content = p; clen = 1; p++;
            }
            /* Check for \text{} inside — render as plain text */
            char *inner = g_strndup(content, clen);
            if (strstr(inner, "\\text") || strstr(inner, "\\mathrm")) {
                char *plain = latex_to_unicode(inner);
                g_string_append(out, plain);
                g_free(plain);
            } else {
                for (size_t i = 0; i < clen; i++) {
                    char c = inner[i];
                    if (c >= '0' && c <= '9')
                        g_string_append(out, sup_digits[(int)(c - '0')]);
                    else if (c >= 'a' && c <= 'z') {
                        const char *s = g_utf8_offset_to_pointer(sup_letters, c - 'a');
                        if (s && *s != ' ')
                            g_string_append_len(out, s, (gssize)(g_utf8_next_char(s) - s));
                        else
                            g_string_append_c(out, c);
                    }
                    else if (c == '+') g_string_append(out, "⁺");
                    else if (c == '-') g_string_append(out, "⁻");
                    else if (c == '=') g_string_append(out, "⁼");
                    else if (c == '(') g_string_append(out, "⁽");
                    else if (c == ')') g_string_append(out, "⁾");
                    else if (c == '{' || c == '}') { /* skip braces */ }
                    else g_string_append_c(out, c);
                }
            }
            g_free(inner);
            continue;
        }
        /* Subscript: _X or _{...} */
        if (*p == '_') {
            p++;
            const char *content; size_t clen;
            if (*p == '{') {
                p++;
                /* Find matching '}' with nesting */
                int depth = 1;
                const char *start = p;
                while (*p && depth > 0) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                    if (depth > 0) p++;
                }
                content = start; clen = (size_t)(p - start);
                if (*p == '}') p++;
            } else {
                content = p; clen = 1; p++;
            }
            /* Check for \text{} inside — render as plain text */
            char *inner = g_strndup(content, clen);
            if (strstr(inner, "\\text") || strstr(inner, "\\mathrm")) {
                char *plain = latex_to_unicode(inner);
                g_string_append(out, plain);
                g_free(plain);
            } else {
                for (size_t i = 0; i < clen; i++) {
                    char c = inner[i];
                    if (c >= '0' && c <= '9')
                        g_string_append(out, sub_digits[(int)(c - '0')]);
                    else if (c >= 'a' && c <= 'z') {
                        const char *s = g_utf8_offset_to_pointer(sub_letters, c - 'a');
                        if (s && *s != ' ')
                            g_string_append_len(out, s, (gssize)(g_utf8_next_char(s) - s));
                        else
                            g_string_append_c(out, c);
                    }
                    else if (c == '+') g_string_append(out, "₊");
                    else if (c == '-') g_string_append(out, "₋");
                    else if (c == '=') g_string_append(out, "₌");
                    else if (c == '(') g_string_append(out, "₍");
                    else if (c == ')') g_string_append(out, "₎");
                    else if (c == '{' || c == '}') { /* skip braces */ }
                    else g_string_append_c(out, c);
                }
            }
            g_free(inner);
            continue;
        }
        /* Skip braces, spaces pass through */
        if (*p == '{' || *p == '}') { p++; continue; }
        /* Regular character */
        const char *next = g_utf8_next_char(p);
        g_string_append_len(out, p, (gssize)(next - p));
        p = next;
    }
    return g_string_free(out, FALSE);
}

/* CSS for the AI markdown webview — dark and light variants */
static const char *ai_webview_css_dark =
    "body { font-family: 'Inter', 'Cantarell', sans-serif; font-size: 14px;"
    "  color: #d4d4d4; background: #1e1e1e; margin: 12px 16px; line-height: 1.6; }"
    "h1,h2,h3,h4,h5,h6 { color: #e5c07b; margin-top: 1em; margin-bottom: 0.4em; }"
    "h1 { font-size: 1.4em; } h2 { font-size: 1.25em; } h3 { font-size: 1.1em; }"
    "strong { color: #e8e8e8; } em { color: #cccccc; }"
    "code { font-family: 'JetBrains Mono', 'Fira Code', monospace; font-size: 0.9em;"
    "  background: #2d2d2d; color: #ce9178; padding: 2px 5px; border-radius: 3px; }"
    "pre { background: #161616; border-radius: 6px; padding: 12px 14px;"
    "  overflow-x: auto; margin: 0.8em 0; }"
    "pre code { background: none; color: #d4d4d4; padding: 0; }"
    "a { color: #61afef; } blockquote { border-left: 3px solid #555; padding-left: 12px; color: #999; margin: 0.5em 0; }"
    "table { border-collapse: collapse; margin: 0.8em 0; width: auto; }"
    "th, td { border: 1px solid #444; padding: 6px 12px; text-align: left; }"
    "th { background: #2a2a2a; color: #e5c07b; font-weight: bold; }"
    "tr:nth-child(even) { background: #252525; }"
    "hr { border: none; border-top: 1px solid #444; margin: 1.2em 0; }"
    "ul, ol { padding-left: 1.6em; } li { margin: 0.2em 0; }"
    "del { color: #888; }"
    ".math { font-family: 'JetBrains Mono', monospace; font-style: italic; color: #c586c0; }"
    "img { max-width: 100%; }"
    "::-webkit-scrollbar { width: 8px; }"
    "::-webkit-scrollbar-thumb { background: #555; border-radius: 4px; }"
    "::-webkit-scrollbar-track { background: #1e1e1e; }";

static const char *ai_webview_css_light =
    "body { font-family: 'Inter', 'Cantarell', sans-serif; font-size: 14px;"
    "  color: #1e1e1e; background: #ffffff; margin: 12px 16px; line-height: 1.6; }"
    "h1,h2,h3,h4,h5,h6 { color: #986801; margin-top: 1em; margin-bottom: 0.4em; }"
    "h1 { font-size: 1.4em; } h2 { font-size: 1.25em; } h3 { font-size: 1.1em; }"
    "strong { color: #1a1a1a; } em { color: #333; }"
    "code { font-family: 'JetBrains Mono', 'Fira Code', monospace; font-size: 0.9em;"
    "  background: #f0f0f0; color: #c7254e; padding: 2px 5px; border-radius: 3px; }"
    "pre { background: #f6f8fa; border-radius: 6px; padding: 12px 14px;"
    "  overflow-x: auto; margin: 0.8em 0; border: 1px solid #e1e4e8; }"
    "pre code { background: none; color: #24292e; padding: 0; }"
    "a { color: #0366d6; } blockquote { border-left: 3px solid #ccc; padding-left: 12px; color: #666; margin: 0.5em 0; }"
    "table { border-collapse: collapse; margin: 0.8em 0; width: auto; }"
    "th, td { border: 1px solid #d0d7de; padding: 6px 12px; text-align: left; }"
    "th { background: #f0f3f6; color: #1a1a1a; font-weight: bold; }"
    "tr:nth-child(even) { background: #f6f8fa; }"
    "hr { border: none; border-top: 1px solid #d0d7de; margin: 1.2em 0; }"
    "ul, ol { padding-left: 1.6em; } li { margin: 0.2em 0; }"
    "del { color: #999; }"
    ".math { font-family: 'JetBrains Mono', monospace; font-style: italic; color: #9c27b0; }"
    "img { max-width: 100%; }"
    "::-webkit-scrollbar { width: 8px; }"
    "::-webkit-scrollbar-thumb { background: #c0c0c0; border-radius: 4px; }"
    "::-webkit-scrollbar-track { background: #f0f0f0; }";

/* Refresh AI output — render markdown to HTML and load in WebView */
void ai_refresh_output(VibeWindow *win) {
    if (!win->ai_conversation_md) return;

    /* Pick CSS based on current theme (dark vs light) */
    AdwStyleManager *sm = adw_style_manager_get_default();
    gboolean is_dark = adw_style_manager_get_dark(sm);

    GString *html = g_string_new(
        "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<style>");
    g_string_append(html, is_dark ? ai_webview_css_dark : ai_webview_css_light);
    /* Override font-size and opacity from settings */
    g_string_append_printf(html, " body { font-size: %dpx !important; opacity: %.2f !important; }",
                           win->settings.ai_font_size, win->settings.ai_font_intensity);
    g_string_append(html, "</style></head><body>");

    if (win->ai_conversation_md->len > 0) {
        if (win->settings.ai_markdown) {
            /* Step 1: Extract LaTeX, replace with placeholders */
            GPtrArray *math_exprs = g_ptr_array_new_with_free_func(g_free);
            GString *safe = g_string_new(NULL);
            const char *p = win->ai_conversation_md->str;
            while (*p) {
                if (p[0] == '$' && p[1] == '$') {
                    const char *close = strstr(p + 2, "$$");
                    if (close) {
                        char *expr = g_strndup(p + 2, (gsize)(close - p - 2));
                        g_string_append_printf(safe, "<span class='math'>MATHPH%u</span>",
                                               math_exprs->len);
                        g_ptr_array_add(math_exprs, expr);
                        p = close + 2;
                        continue;
                    }
                }
                if (p[0] == '$' && p[1] != '$') {
                    const char *close = strchr(p + 1, '$');
                    if (close && close > p + 1 && !memchr(p + 1, '\n', (size_t)(close - p - 1))) {
                        char *expr = g_strndup(p + 1, (gsize)(close - p - 1));
                        g_string_append_printf(safe, "<span class='math'>MATHPH%u</span>",
                                               math_exprs->len);
                        g_ptr_array_add(math_exprs, expr);
                        p = close + 1;
                        continue;
                    }
                }
                const char *next = g_utf8_next_char(p);
                g_string_append_len(safe, p, (gssize)(next - p));
                p = next;
            }

            /* Step 2: Parse with cmark-gfm to HTML */
            cmark_gfm_core_extensions_ensure_registered();
            cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);
            const char *ext_names[] = {"table", "strikethrough", "autolink", NULL};
            for (int i = 0; ext_names[i]; i++) {
                cmark_syntax_extension *ext = cmark_find_syntax_extension(ext_names[i]);
                if (ext) cmark_parser_attach_syntax_extension(parser, ext);
            }
            cmark_parser_feed(parser, safe->str, safe->len);
            cmark_node *doc = cmark_parser_finish(parser);
            char *rendered = cmark_render_html(doc, CMARK_OPT_DEFAULT,
                                              cmark_parser_get_syntax_extensions(parser));

            /* Step 3: Replace MATHPH placeholders with Unicode-rendered LaTeX (single pass) */
            GString *final_html = g_string_new(NULL);
            const char *src = rendered;
            while (*src) {
                if (strncmp(src, "MATHPH", 6) == 0) {
                    /* Parse the placeholder index */
                    const char *num_start = src + 6;
                    char *num_end;
                    unsigned long idx = strtoul(num_start, &num_end, 10);
                    if (num_end > num_start && idx < math_exprs->len) {
                        const char *expr = g_ptr_array_index(math_exprs, idx);
                        char *uni = latex_to_unicode(expr);
                        g_string_append(final_html, uni);
                        g_free(uni);
                        src = num_end;
                        continue;
                    }
                }
                g_string_append_c(final_html, *src);
                src++;
            }

            g_string_append(html, final_html->str);
            g_string_free(final_html, TRUE);
            free(rendered);
            cmark_node_free(doc);
            cmark_parser_free(parser);
            g_string_free(safe, TRUE);
            g_ptr_array_unref(math_exprs);
        } else {
            /* Raw text mode */
            char *escaped = g_markup_escape_text(win->ai_conversation_md->str, -1);
            g_string_append(html, "<pre>");
            g_string_append(html, escaped);
            g_string_append(html, "</pre>");
            g_free(escaped);
        }
    }

    g_string_append(html, "<script>window.scrollTo(0, document.body.scrollHeight);</script>");
    g_string_append(html, "</body></html>");

    webkit_web_view_load_html(win->ai_webview, html->str, NULL);
    g_string_free(html, TRUE);
}

void ai_parse_and_display(VibeWindow *win) {
    const char *json = win->ai_response_buf->str;

    /* Extract "result" field value */
    const char *result_key = "\"result\":\"";
    const char *rp = strstr(json, result_key);
    GString *result_text = g_string_new(NULL);
    if (rp) {
        rp += strlen(result_key);
        while (*rp && *rp != '"') {
            if (rp[0] == '\\' && rp[1]) {
                switch (rp[1]) {
                    case 'n': g_string_append_c(result_text, '\n'); break;
                    case 't': g_string_append_c(result_text, '\t'); break;
                    case 'r': g_string_append_c(result_text, '\r'); break;
                    case 'b': g_string_append_c(result_text, '\b'); break;
                    case 'f': g_string_append_c(result_text, '\f'); break;
                    case '"': g_string_append_c(result_text, '"'); rp += 2; continue;
                    case '\\': g_string_append_c(result_text, '\\'); rp += 2; continue;
                    case '/': g_string_append_c(result_text, '/'); break;
                    case 'u': {
                        /* \uXXXX Unicode escape */
                        if (rp[2] && rp[3] && rp[4] && rp[5]) {
                            char hex[5] = { rp[2], rp[3], rp[4], rp[5], 0 };
                            gunichar cp = (gunichar)strtoul(hex, NULL, 16);
                            /* Handle UTF-16 surrogate pairs */
                            if (cp >= 0xD800 && cp <= 0xDBFF &&
                                rp[6] == '\\' && rp[7] == 'u') {
                                char hex2[5] = { rp[8], rp[9], rp[10], rp[11], 0 };
                                gunichar lo = (gunichar)strtoul(hex2, NULL, 16);
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                rp += 6; /* extra 6 for second \uXXXX */
                            }
                            char utf8[7];
                            int len = g_unichar_to_utf8(cp, utf8);
                            g_string_append_len(result_text, utf8, len);
                            rp += 6; /* skip \uXXXX */
                            continue;
                        }
                        g_string_append_c(result_text, rp[1]);
                        break;
                    }
                    default: g_string_append_c(result_text, rp[1]); break;
                }
                rp += 2;
            } else {
                g_string_append_c(result_text, *rp);
                rp++;
            }
        }
    } else {
        /* Fallback: show raw output */
        g_string_append(result_text, json);
    }

    /* Append to conversation markdown and refresh webview */
    if (!win->ai_conversation_md)
        win->ai_conversation_md = g_string_new(NULL);
    g_string_append(win->ai_conversation_md, result_text->str);
    g_string_append(win->ai_conversation_md, "\n\n");

    char *result_for_log = g_string_free(result_text, FALSE);

    ai_refresh_output(win);

    /* Extract session_id for --resume */
    const char *sid_key = "\"session_id\":\"";
    const char *sp = strstr(json, sid_key);
    if (sp) {
        sp += strlen(sid_key);
        const char *se = strchr(sp, '"');
        if (se && (se - sp) < 127) {
            char old_sid[128];
            g_strlcpy(old_sid, win->ai_session_id, sizeof(old_sid));
            memcpy(win->ai_session_id, sp, se - sp);
            win->ai_session_id[se - sp] = '\0';
            /* Track new session start */
            if (strcmp(old_sid, win->ai_session_id) != 0) {
                win->ai_session_start = g_get_real_time();
                win->ai_session_turns = 0;
            }
            win->ai_session_turns++;
            /* Persist session ID + stats for resume after restart */
            g_strlcpy(win->settings.ai_last_session, win->ai_session_id,
                       sizeof(win->settings.ai_last_session));
            win->settings.ai_session_start = win->ai_session_start;
            win->settings.ai_session_turns = win->ai_session_turns;
            settings_save(&win->settings);
        }
    }

    /* Extract model name for status — keep previous if not found */
    const char *mu = strstr(json, "\"modelUsage\":{\"");
    if (mu) {
        mu += strlen("\"modelUsage\":{\"");
        const char *me = strchr(mu, '"');
        char model[128];
        if (me && (me - mu) < 127) {
            memcpy(model, mu, me - mu);
            model[me - mu] = '\0';
            gtk_label_set_text(win->ai_status_label, model);
        }
    }
    update_status_bar(win);

    /* Compute elapsed time */
    gint64 now = g_get_monotonic_time();
    win->ai_last_elapsed = (now - win->ai_start_time) / 1e6;

    /* Extract token usage: "inputTokens":N and "outputTokens":N */
    const char *it = strstr(json, "\"inputTokens\":");
    if (it) {
        it += strlen("\"inputTokens\":");
        win->ai_input_tokens += atoi(it);
    }
    const char *ot = strstr(json, "\"outputTokens\":");
    if (ot) {
        ot += strlen("\"outputTokens\":");
        win->ai_output_tokens += atoi(ot);
    }


    /* Update token label with dynamic formatting */
    {
        char in_s[16], out_s[16], tot_s[16], time_s[32];
        int total = win->ai_input_tokens + win->ai_output_tokens;

        #define FMT_TOK(buf, n) do { \
            if ((n) >= 1000000) snprintf(buf, sizeof(buf), "%.1fM", (n)/1e6); \
            else if ((n) >= 1000) snprintf(buf, sizeof(buf), "%.1fk", (n)/1e3); \
            else snprintf(buf, sizeof(buf), "%d", (n)); \
        } while(0)

        FMT_TOK(in_s, win->ai_input_tokens);
        FMT_TOK(out_s, win->ai_output_tokens);
        FMT_TOK(tot_s, total);
        #undef FMT_TOK

        if (win->ai_last_elapsed >= 60.0)
            snprintf(time_s, sizeof(time_s), "%.0fm%.0fs",
                     win->ai_last_elapsed / 60.0,
                     fmod(win->ai_last_elapsed, 60.0));
        else
            snprintf(time_s, sizeof(time_s), "%.1fs", win->ai_last_elapsed);

        char tok_str[256];
        snprintf(tok_str, sizeof(tok_str), "%s  |  in: %s  out: %s  total: %s",
                 time_s, in_s, out_s, tot_s);
        gtk_label_set_text(win->ai_token_label, tok_str);
    }

    /* Log prompt (input) + response (output) to JSON — both with correct model/session */
    const char *log_session = win->ai_session_id[0] ? win->ai_session_id : NULL;
    const char *log_model = gtk_label_get_text(win->ai_status_label);
    if (win->ai_last_prompt) {
        prompt_log_input(win->root_dir, log_session, log_model, win->ai_last_prompt);
        g_free(win->ai_last_prompt);
        win->ai_last_prompt = NULL;
    }

    int req_in = 0, req_out = 0;
    const char *it2 = strstr(json, "\"inputTokens\":");
    const char *ot2 = strstr(json, "\"outputTokens\":");
    if (it2) req_in = atoi(it2 + 14);
    if (ot2) req_out = atoi(ot2 + 15);
    prompt_log_output(win->root_dir, log_session, log_model,
                      result_for_log, req_in, req_out,
                      win->ai_last_elapsed);
    g_free(result_for_log);
}

/* Called when claude process finishes and all stdout is available */
void on_ai_communicate_done(GObject *src, GAsyncResult *res, gpointer data) {
    VibeWindow *win = data;
    GBytes *stdout_bytes = NULL;
    GBytes *stderr_bytes = NULL;
    GError *err = NULL;

    g_subprocess_communicate_finish(G_SUBPROCESS(src), res, &stdout_bytes, &stderr_bytes, &err);

    /* Stop elapsed timer */
    if (win->ai_timer_id) {
        g_source_remove(win->ai_timer_id);
        win->ai_timer_id = 0;
    }

    if (!win->ai_conversation_md)
        win->ai_conversation_md = g_string_new(NULL);

    if (err) {
        /* Communication error (cancelled, pipe broken, etc.) */
        if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_string_append(win->ai_conversation_md, "\n\n**Cancelled.**\n\n");
        else
            g_string_append_printf(win->ai_conversation_md, "\n\n**Error:** %s\n\n", err->message);
        ai_refresh_output(win);
        gtk_label_set_text(win->ai_status_label, "error");
        update_status_bar(win);
        g_error_free(err);
        if (stdout_bytes) g_bytes_unref(stdout_bytes);
        if (stderr_bytes) g_bytes_unref(stderr_bytes);
        g_clear_object(&win->ai_proc);
        return;
    }

    /* Check exit status */
    gboolean exited_ok = g_subprocess_get_successful(G_SUBPROCESS(src));

    /* Detect invalid session — clear session and retry with new one */
    if (!exited_ok && win->ai_session_id[0]) {
        gboolean session_invalid = FALSE;
        if (stdout_bytes) {
            gsize slen;
            const char *sdata = g_bytes_get_data(stdout_bytes, &slen);
            if (slen > 0 && strstr(sdata, "No conversation found"))
                session_invalid = TRUE;
        }
        if (!session_invalid && stderr_bytes) {
            gsize slen;
            const char *sdata = g_bytes_get_data(stderr_bytes, &slen);
            if (slen > 0 && strstr(sdata, "No conversation found"))
                session_invalid = TRUE;
        }
        if (session_invalid) {
            /* Clear stale session */
            win->ai_session_id[0] = '\0';
            win->settings.ai_last_session[0] = '\0';
            settings_save(&win->settings);
            if (stdout_bytes) g_bytes_unref(stdout_bytes);
            if (stderr_bytes) g_bytes_unref(stderr_bytes);
            g_clear_object(&win->ai_proc);
            /* Retry: re-send the last prompt without --resume */
            if (win->ai_last_prompt) {
                g_string_append(win->ai_conversation_md,
                    "\n\n*Session expired, starting new session...*\n\n");
                ai_refresh_output(win);
                /* Put prompt back into buffer so send_prompt_to_ai picks it up */
                gtk_text_buffer_set_text(win->prompt_buffer, win->ai_last_prompt, -1);
                g_free(win->ai_last_prompt);
                win->ai_last_prompt = NULL;
                send_prompt_to_ai(win);
            }
            return;
        }
    }

    if (stdout_bytes) {
        gsize len;
        const char *json = g_bytes_get_data(stdout_bytes, &len);

        if (len > 0 && strstr(json, "\"result\"")) {
            /* Valid response */
            g_string_truncate(win->ai_response_buf, 0);
            g_string_append_len(win->ai_response_buf, json, len);
            g_bytes_unref(stdout_bytes);
            if (stderr_bytes) g_bytes_unref(stderr_bytes);
            ai_parse_and_display(win);
            g_clear_object(&win->ai_proc);
            return;
        }

        /* Got stdout but no valid JSON result */
        if (!exited_ok) {
            /* Try to extract error from stderr or stdout */
            const char *errmsg = NULL;
            gsize errlen = 0;
            if (stderr_bytes) {
                errmsg = g_bytes_get_data(stderr_bytes, &errlen);
            }
            if (errlen > 0 && errmsg) {
                char *msg = g_strndup(errmsg, errlen < 500 ? errlen : 500);
                g_string_append_printf(win->ai_conversation_md,
                    "\n\n**Error (process failed):**\n```\n%s\n```\n\n", msg);
                g_free(msg);
            } else if (len > 0) {
                char *msg = g_strndup(json, len < 500 ? len : 500);
                g_string_append_printf(win->ai_conversation_md,
                    "\n\n**Error (unexpected output):**\n```\n%s\n```\n\n", msg);
                g_free(msg);
            } else {
                g_string_append(win->ai_conversation_md,
                    "\n\n**Error:** Process failed with no output.\n\n");
            }
        } else {
            /* Exited OK but no result field — empty or malformed response */
            if (len == 0) {
                g_string_append(win->ai_conversation_md,
                    "\n\n**Error:** Empty response from model.\n\n");
            } else {
                char *msg = g_strndup(json, len < 500 ? len : 500);
                g_string_append_printf(win->ai_conversation_md,
                    "\n\n**Error (no result):**\n```\n%s\n```\n\n", msg);
                g_free(msg);
            }
        }
        g_bytes_unref(stdout_bytes);
    } else {
        /* No stdout at all */
        g_string_append(win->ai_conversation_md,
            "\n\n**Error:** No response from model. Check your connection and API key.\n\n");
    }

    ai_refresh_output(win);
    gtk_label_set_text(win->ai_status_label, "error");
    update_status_bar(win);
    if (stderr_bytes) g_bytes_unref(stderr_bytes);
    g_clear_object(&win->ai_proc);
}

/* ── AI elapsed time timer ── */

gboolean ai_timer_tick(gpointer data) {
    VibeWindow *win = data;
    if (!win->ai_proc) {
        win->ai_timer_id = 0;
        return G_SOURCE_REMOVE;
    }
    gint64 now = g_get_monotonic_time();
    double elapsed = (now - win->ai_start_time) / 1e6;
    char buf[64];
    if (elapsed >= 60.0)
        snprintf(buf, sizeof(buf), "thinking… %.0fm%.0fs",
                 elapsed / 60.0, fmod(elapsed, 60.0));
    else
        snprintf(buf, sizeof(buf), "thinking… %.1fs", elapsed);
    gtk_label_set_text(win->ai_status_label, buf);
    return G_SOURCE_CONTINUE;
}

/* ── Streaming JSON helpers ── */

/* Extract a JSON string value for a given key from a JSON line.
 * Returns newly allocated string or NULL. Simple parser for flat objects. */
static char *json_extract_string(const char *json, const char *key) {
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);
    const char *p = strstr(json, needle);
    if (!p) return NULL;
    p += strlen(needle);
    GString *val = g_string_new(NULL);
    while (*p && *p != '"') {
        if (p[0] == '\\' && p[1]) {
            switch (p[1]) {
                case 'n': g_string_append_c(val, '\n'); break;
                case 't': g_string_append_c(val, '\t'); break;
                case 'r': g_string_append_c(val, '\r'); break;
                case '"': g_string_append_c(val, '"'); break;
                case '\\': g_string_append_c(val, '\\'); break;
                case '/': g_string_append_c(val, '/'); break;
                case 'u': {
                    if (p[2] && p[3] && p[4] && p[5]) {
                        char hex[5] = { p[2], p[3], p[4], p[5], 0 };
                        gunichar cp = (gunichar)strtoul(hex, NULL, 16);
                        if (cp >= 0xD800 && cp <= 0xDBFF &&
                            p[6] == '\\' && p[7] == 'u') {
                            char hex2[5] = { p[8], p[9], p[10], p[11], 0 };
                            gunichar lo = (gunichar)strtoul(hex2, NULL, 16);
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            p += 6;
                        }
                        char utf8[7];
                        int len = g_unichar_to_utf8(cp, utf8);
                        g_string_append_len(val, utf8, len);
                        p += 6;
                        continue;
                    }
                    g_string_append_c(val, p[1]);
                    break;
                }
                default: g_string_append_c(val, p[1]); break;
            }
            p += 2;
        } else {
            g_string_append_c(val, *p);
            p++;
        }
    }
    return g_string_free(val, FALSE);
}

static int json_extract_int(const char *json, const char *key) {
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char *p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);
    return atoi(p);
}

/* Extract raw JSON object value for a given key — returns the substring
 * from the opening '{' to the matching '}', or NULL. Caller must g_free(). */
static char *json_extract_raw_object(const char *json, const char *key) {
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char *p = strstr(json, needle);
    if (!p) return NULL;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '{') return NULL;
    int depth = 0;
    const char *start = p;
    gboolean in_string = FALSE;
    for (; *p; p++) {
        if (in_string) {
            if (*p == '\\' && p[1]) { p++; continue; }
            if (*p == '"') in_string = FALSE;
            continue;
        }
        if (*p == '"') { in_string = TRUE; continue; }
        if (*p == '{') depth++;
        else if (*p == '}') { depth--; if (depth == 0) return g_strndup(start, (gsize)(p - start + 1)); }
    }
    return NULL;
}

/* ── Tool-use confirmation dialog (streaming mode, ai_auto_accept == FALSE) ── */

typedef struct {
    VibeWindow *win;
    gboolean    allowed;
    GMainLoop  *loop;
} ToolConfirmCtx;

static void on_tool_allow(GtkButton *btn, gpointer data) {
    (void)btn;
    ToolConfirmCtx *ctx = data;
    ctx->allowed = TRUE;
    g_main_loop_quit(ctx->loop);
}

static void on_tool_deny(GtkButton *btn, gpointer data) {
    (void)btn;
    ToolConfirmCtx *ctx = data;
    ctx->allowed = FALSE;
    g_main_loop_quit(ctx->loop);
}

static void on_tool_dialog_close(GtkWindow *dialog, gpointer data) {
    (void)dialog;
    ToolConfirmCtx *ctx = data;
    /* Treat close as deny */
    if (g_main_loop_is_running(ctx->loop))
        g_main_loop_quit(ctx->loop);
}

/* Extract key parameters from tool input JSON and build a human-readable
 * summary.  Returns a newly-allocated string or NULL if nothing useful. */
static char *tool_params_summary(const char *tool_name, const char *input_json) {
    if (!input_json || g_strcmp0(input_json, "{}") == 0)
        return NULL;

    GString *s = g_string_new(NULL);

    /* Pick the most relevant fields per tool */
    const char *keys[][4] = {
        { "Read",  "file_path", NULL },
        { "Edit",  "file_path", "old_string", "new_string" },
        { "Write", "file_path", NULL },
        { "Bash",  "command", NULL },
        { "Glob",  "pattern", "path", NULL },
        { "Grep",  "pattern", "path", "glob" },
        { NULL }
    };

    const char **fields = NULL;
    for (int i = 0; keys[i][0]; i++) {
        if (g_strcmp0(tool_name, keys[i][0]) == 0) { fields = &keys[i][1]; break; }
    }

    if (fields) {
        for (int i = 0; fields[i]; i++) {
            char *val = json_extract_string(input_json, fields[i]);
            if (val) {
                /* Truncate very long values (e.g. file content) */
                if (strlen(val) > 200) {
                    val[197] = '.'; val[198] = '.'; val[199] = '.'; val[200] = '\0';
                }
                g_string_append_printf(s, "<b>%s:</b> %s\n", fields[i], val);
                g_free(val);
            }
        }
    }

    if (s->len == 0) { g_string_free(s, TRUE); return NULL; }
    /* Remove trailing newline */
    if (s->str[s->len - 1] == '\n') g_string_truncate(s, s->len - 1);
    return g_string_free(s, FALSE);
}

/* Show a modal tool-use confirmation dialog. Returns TRUE if allowed. */
static gboolean ai_tool_use_confirm(VibeWindow *win, const char *tool_name,
                                     const char *params_json) {
    char title[128];
    snprintf(title, sizeof(title), "Tool: %s", tool_name);

    char *summary = tool_params_summary(tool_name, params_json);
    gboolean has_params = summary != NULL;

    GtkWidget *dialog = vibe_dialog_new(win, title, 350, -1);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    /* Label */
    char *label_text = g_strdup_printf("Claude wants to use <b>%s</b>", tool_name);
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), label_text);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_box_append(GTK_BOX(box), label);
    g_free(label_text);

    /* Parameters as markup labels — compact, no textview for simple info */
    if (has_params) {
        GtkWidget *params_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(params_label), summary);
        gtk_label_set_xalign(GTK_LABEL(params_label), 0);
        gtk_label_set_wrap(GTK_LABEL(params_label), TRUE);
        gtk_label_set_selectable(GTK_LABEL(params_label), TRUE);
        PangoAttrList *attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, pango_attr_family_new("monospace"));
        pango_attr_list_insert(attrs, pango_attr_scale_new(0.9));
        gtk_label_set_attributes(GTK_LABEL(params_label), attrs);
        pango_attr_list_unref(attrs);
        gtk_box_append(GTK_BOX(box), params_label);
        g_free(summary);
    }

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(box), btn_box);

    GtkWidget *deny_btn = gtk_button_new_with_label("Deny");
    gtk_widget_add_css_class(deny_btn, "destructive-action");
    gtk_box_append(GTK_BOX(btn_box), deny_btn);

    GtkWidget *allow_btn = gtk_button_new_with_label("Allow");
    gtk_widget_add_css_class(allow_btn, "suggested-action");
    gtk_box_append(GTK_BOX(btn_box), allow_btn);

    ToolConfirmCtx ctx = { .win = win, .allowed = FALSE,
                           .loop = g_main_loop_new(NULL, FALSE) };

    g_signal_connect(allow_btn, "clicked", G_CALLBACK(on_tool_allow), &ctx);
    g_signal_connect(deny_btn, "clicked", G_CALLBACK(on_tool_deny), &ctx);
    g_signal_connect(dialog, "close-request", G_CALLBACK(on_tool_dialog_close), &ctx);

    gtk_window_present(GTK_WINDOW(dialog));
    g_main_loop_run(ctx.loop);
    g_main_loop_unref(ctx.loop);
    gtk_window_destroy(GTK_WINDOW(dialog));

    return ctx.allowed;
}

/* Finalize streaming response — extract metadata from accumulated buffer */
static void ai_stream_finalize(VibeWindow *win) {
    /* Stop elapsed timer */
    if (win->ai_timer_id) {
        g_source_remove(win->ai_timer_id);
        win->ai_timer_id = 0;
    }

    g_string_append(win->ai_conversation_md, "\n\n");
    ai_refresh_output(win);

    /* The response_buf accumulated the last "result" line's JSON */
    const char *json = win->ai_response_buf->str;

    /* Extract session_id */
    char *sid = json_extract_string(json, "session_id");
    if (sid) {
        if (strcmp(win->ai_session_id, sid) != 0) {
            win->ai_session_start = g_get_real_time();
            win->ai_session_turns = 0;
        }
        win->ai_session_turns++;
        g_strlcpy(win->ai_session_id, sid, sizeof(win->ai_session_id));
        g_strlcpy(win->settings.ai_last_session, sid, sizeof(win->settings.ai_last_session));
        win->settings.ai_session_start = win->ai_session_start;
        win->settings.ai_session_turns = win->ai_session_turns;
        settings_save(&win->settings);
        g_free(sid);
    }

    /* Extract model name from subkey pattern */
    const char *mu = strstr(json, "\"model\":\"");
    if (mu) {
        mu += 9;
        const char *me = strchr(mu, '"');
        if (me && (me - mu) < 127) {
            char model[128];
            memcpy(model, mu, me - mu);
            model[me - mu] = '\0';
            gtk_label_set_text(win->ai_status_label, model);
        }
    }
    update_status_bar(win);

    /* Elapsed time */
    gint64 now = g_get_monotonic_time();
    win->ai_last_elapsed = (now - win->ai_start_time) / 1e6;

    /* Token usage */
    int req_in = json_extract_int(json, "inputTokens");
    int req_out = json_extract_int(json, "outputTokens");
    if (req_in) win->ai_input_tokens += req_in;
    if (req_out) win->ai_output_tokens += req_out;

    /* Update token label */
    {
        char in_s[16], out_s[16], tot_s[16], time_s[32];
        int total = win->ai_input_tokens + win->ai_output_tokens;

        #define FMT_TOK(buf, n) do { \
            if ((n) >= 1000000) snprintf(buf, sizeof(buf), "%.1fM", (n)/1e6); \
            else if ((n) >= 1000) snprintf(buf, sizeof(buf), "%.1fk", (n)/1e3); \
            else snprintf(buf, sizeof(buf), "%d", (n)); \
        } while(0)

        FMT_TOK(in_s, win->ai_input_tokens);
        FMT_TOK(out_s, win->ai_output_tokens);
        FMT_TOK(tot_s, total);
        #undef FMT_TOK

        if (win->ai_last_elapsed >= 60.0)
            snprintf(time_s, sizeof(time_s), "%.0fm%.0fs",
                     win->ai_last_elapsed / 60.0,
                     fmod(win->ai_last_elapsed, 60.0));
        else
            snprintf(time_s, sizeof(time_s), "%.1fs", win->ai_last_elapsed);

        char tok_str[256];
        snprintf(tok_str, sizeof(tok_str), "%s  |  in: %s  out: %s  total: %s",
                 time_s, in_s, out_s, tot_s);
        gtk_label_set_text(win->ai_token_label, tok_str);
    }

    /* Log prompt + response */
    const char *log_session = win->ai_session_id[0] ? win->ai_session_id : NULL;
    const char *log_model = gtk_label_get_text(win->ai_status_label);
    if (win->ai_last_prompt) {
        prompt_log_input(win->root_dir, log_session, log_model, win->ai_last_prompt);
        g_free(win->ai_last_prompt);
        win->ai_last_prompt = NULL;
    }
    /* Log the full result text that was streamed */
    char *result_for_log = json_extract_string(json, "result");
    if (result_for_log) {
        prompt_log_output(win->root_dir, log_session, log_model,
                          result_for_log, req_in, req_out, win->ai_last_elapsed);
        g_free(result_for_log);
    }

    g_clear_object(&win->ai_stream);
    g_clear_object(&win->ai_proc);
}

/* Guard for pending stream refresh timer (cleaned up on result event) */
static guint ai_stream_refresh_id = 0;

void on_ai_stream_line_ready(GObject *src, GAsyncResult *res, gpointer data) {
    VibeWindow *win = data;
    gsize len = 0;
    GError *err = NULL;
    char *line = g_data_input_stream_read_line_finish(
        G_DATA_INPUT_STREAM(src), res, &len, &err);

    if (err) {
        if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_string_append_printf(win->ai_conversation_md,
                "\n\n**Error:** %s\n\n", err->message);
            ai_refresh_output(win);
        }
        g_error_free(err);
        if (win->ai_timer_id) { g_source_remove(win->ai_timer_id); win->ai_timer_id = 0; }
        gtk_label_set_text(win->ai_status_label, "error");
        g_clear_object(&win->ai_stream);
        g_clear_object(&win->ai_proc);
        return;
    }

    if (!line) {
        /* EOF — process exited */
        /* Check if we got a result event; if not, check for session expiry */
        if (win->ai_response_buf->len == 0 && win->ai_session_id[0]) {
            /* Possibly expired session — retry */
            win->ai_session_id[0] = '\0';
            win->settings.ai_last_session[0] = '\0';
            settings_save(&win->settings);
            g_clear_object(&win->ai_stream);
            g_clear_object(&win->ai_proc);
            if (win->ai_last_prompt) {
                g_string_append(win->ai_conversation_md,
                    "\n\n*Session expired, starting new session...*\n\n");
                ai_refresh_output(win);
                gtk_text_buffer_set_text(win->prompt_buffer, win->ai_last_prompt, -1);
                g_free(win->ai_last_prompt);
                win->ai_last_prompt = NULL;
                send_prompt_to_ai(win);
            }
            return;
        }
        ai_stream_finalize(win);
        return;
    }

    /* Process stream-json event line */
    if (!win->ai_conversation_md)
        win->ai_conversation_md = g_string_new(NULL);

    /* ── Tool-use confirmation dialog ──
     * auto_accept ON  + tool enabled  → auto-allow (no dialog)
     * auto_accept ON  + tool disabled → show dialog
     * auto_accept OFF                 → always show dialog */
    if (strstr(line, "\"type\":\"tool_use\"") && strstr(line, "\"name\":\"")) {
        char *tool_name = json_extract_string(line, "name");
        char *input_json = json_extract_raw_object(line, "input");
        if (tool_name) {
            /* Check if this specific tool is enabled in settings */
            gboolean tool_enabled = FALSE;
            if (g_strcmp0(tool_name, "Read") == 0)  tool_enabled = win->settings.ai_tool_read;
            else if (g_strcmp0(tool_name, "Edit") == 0)  tool_enabled = win->settings.ai_tool_edit;
            else if (g_strcmp0(tool_name, "Write") == 0) tool_enabled = win->settings.ai_tool_write;
            else if (g_strcmp0(tool_name, "Glob") == 0)  tool_enabled = win->settings.ai_tool_glob;
            else if (g_strcmp0(tool_name, "Grep") == 0)  tool_enabled = win->settings.ai_tool_grep;
            else if (g_strcmp0(tool_name, "Bash") == 0)  tool_enabled = win->settings.ai_tool_bash;

            gboolean allowed = win->settings.ai_auto_accept && tool_enabled;

            if (!allowed) {
                /* Show in conversation */
                g_string_append_printf(win->ai_conversation_md,
                    "\n\n> **Tool call:** `%s`\n\n", tool_name);
                ai_refresh_output(win);

                allowed = ai_tool_use_confirm(win, tool_name, input_json);
            }

            if (!allowed) {
                g_string_append(win->ai_conversation_md,
                    "\n\n*Tool denied — stopping AI.*\n\n");
                ai_refresh_output(win);
                g_subprocess_force_exit(win->ai_proc);
                if (win->ai_timer_id) { g_source_remove(win->ai_timer_id); win->ai_timer_id = 0; }
                gtk_label_set_text(win->ai_status_label, "denied");
                g_free(tool_name);
                g_free(input_json);
                g_free(line);
                g_clear_object(&win->ai_stream);
                g_clear_object(&win->ai_proc);
                return;
            }
        }
        g_free(tool_name);
        g_free(input_json);
    }

    /* Detect event type — stream_event wraps content_block_delta with nested delta.text */
    if (strstr(line, "\"text_delta\"")) {
        /* Extract text from: ..."delta":{"type":"text_delta","text":"XXX"} */
        const char *marker = strstr(line, "\"text_delta\",\"text\":\"");
        if (marker) {
            marker += strlen("\"text_delta\",\"text\":\"");
            /* Parse the JSON string value in-place */
            GString *delta = g_string_new(NULL);
            const char *p = marker;
            while (*p && *p != '"') {
                if (p[0] == '\\' && p[1]) {
                    switch (p[1]) {
                        case 'n': g_string_append_c(delta, '\n'); break;
                        case 't': g_string_append_c(delta, '\t'); break;
                        case '"': g_string_append_c(delta, '"'); break;
                        case '\\': g_string_append_c(delta, '\\'); break;
                        case '/': g_string_append_c(delta, '/'); break;
                        case 'u': {
                            if (p[2] && p[3] && p[4] && p[5]) {
                                char hex[5] = { p[2], p[3], p[4], p[5], 0 };
                                gunichar cp = (gunichar)strtoul(hex, NULL, 16);
                                if (cp >= 0xD800 && cp <= 0xDBFF &&
                                    p[6] == '\\' && p[7] == 'u') {
                                    char hex2[5] = { p[8], p[9], p[10], p[11], 0 };
                                    gunichar lo = (gunichar)strtoul(hex2, NULL, 16);
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                    p += 6;
                                }
                                char utf8[7];
                                int ulen = g_unichar_to_utf8(cp, utf8);
                                g_string_append_len(delta, utf8, ulen);
                                p += 6;
                                continue;
                            }
                            g_string_append_c(delta, p[1]);
                            break;
                        }
                        default: g_string_append_c(delta, p[1]); break;
                    }
                    p += 2;
                } else {
                    g_string_append_c(delta, *p);
                    p++;
                }
            }
            if (delta->len > 0) {
                /* Cap conversation at 256KB — trim first half when exceeded */
                #define AI_CONVERSATION_MAX (256 * 1024)
                if (win->ai_conversation_md->len + delta->len > AI_CONVERSATION_MAX) {
                    gsize half = win->ai_conversation_md->len / 2;
                    /* Find a newline near the midpoint to trim cleanly */
                    const char *nl = memchr(win->ai_conversation_md->str + half, '\n',
                                            win->ai_conversation_md->len - half);
                    gsize trim_at = nl ? (gsize)(nl - win->ai_conversation_md->str + 1) : half;
                    g_string_erase(win->ai_conversation_md, 0, trim_at);
                    g_string_prepend(win->ai_conversation_md, "*… (earlier conversation trimmed) …*\n\n");
                }
                g_string_append(win->ai_conversation_md, delta->str);
                /* Streaming: append delta text via JS for performance instead of
                 * full cmark re-render.  Full render happens on stream finalize. */
                char *escaped_js = g_markup_escape_text(delta->str, -1);
                /* Also escape backslashes and quotes for JS string literal */
                GString *js = g_string_new("(function(){var t=document.getElementById('_stream');if(!t){t=document.createElement('span');t.id='_stream';document.body.appendChild(t);}t.insertAdjacentText('beforeend','");
                for (const char *c = escaped_js; *c; c++) {
                    if (*c == '\\') g_string_append(js, "\\\\");
                    else if (*c == '\'') g_string_append(js, "\\'");
                    else if (*c == '\n') g_string_append(js, "\\n");
                    else if (*c == '\r') g_string_append(js, "\\r");
                    else g_string_append_c(js, *c);
                }
                g_string_append(js, "');window.scrollTo(0,document.body.scrollHeight);})()");
                webkit_web_view_evaluate_javascript(win->ai_webview, js->str, -1,
                                                     NULL, NULL, NULL, NULL, NULL);
                g_string_free(js, TRUE);
                g_free(escaped_js);
            }
            g_string_free(delta, TRUE);
        }
    } else if (strstr(line, "\"type\":\"result\"")) {
        /* Final event — store full JSON for metadata extraction */
        g_string_truncate(win->ai_response_buf, 0);
        g_string_append_len(win->ai_response_buf, line, len);
        /* Force final refresh */
        if (ai_stream_refresh_id) {
            g_source_remove(ai_stream_refresh_id);
            ai_stream_refresh_id = 0;
        }

        /* Check for error result — e.g. invalid session */
        if (strstr(line, "\"is_error\":true")) {
            /* Check if session expired */
            if (strstr(line, "No conversation found") && win->ai_session_id[0]) {
                win->ai_session_id[0] = '\0';
                win->settings.ai_last_session[0] = '\0';
                settings_save(&win->settings);
                g_free(line);
                g_clear_object(&win->ai_stream);
                g_clear_object(&win->ai_proc);
                if (win->ai_timer_id) { g_source_remove(win->ai_timer_id); win->ai_timer_id = 0; }
                if (win->ai_last_prompt) {
                    g_string_append(win->ai_conversation_md,
                        "\n\n*Session expired, starting new session...*\n\n");
                    ai_refresh_output(win);
                    gtk_text_buffer_set_text(win->prompt_buffer, win->ai_last_prompt, -1);
                    g_free(win->ai_last_prompt);
                    win->ai_last_prompt = NULL;
                    send_prompt_to_ai(win);
                }
                return;
            }
            /* Generic error */
            char *errmsg = json_extract_string(line, "result");
            g_string_append_printf(win->ai_conversation_md,
                "\n\n**Error:** %s\n\n", errmsg ? errmsg : "Unknown error");
            g_free(errmsg);
            ai_refresh_output(win);
            if (win->ai_timer_id) { g_source_remove(win->ai_timer_id); win->ai_timer_id = 0; }
            gtk_label_set_text(win->ai_status_label, "error");
            g_free(line);
            g_clear_object(&win->ai_stream);
            g_clear_object(&win->ai_proc);
            return;
        }
    }
    /* Ignore other event types (system, message_start, content_block_start, etc.) */

    g_free(line);

    /* Request next line */
    g_data_input_stream_read_line_async(win->ai_stream,
        G_PRIORITY_DEFAULT, win->cancellable, on_ai_stream_line_ready, win);
}

void send_prompt_to_ai(VibeWindow *win) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(win->prompt_buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(win->prompt_buffer, &start, &end, FALSE);

    if (!text || !text[0]) { g_free(text); return; }

    /* Don't send if a process is already running */
    if (win->ai_proc) {
        g_free(text);
        return;
    }

    /* Store prompt for deferred logging (logged after response with correct model/session) */
    g_free(win->ai_last_prompt);
    win->ai_last_prompt = g_strdup(text);

    /* Append prompt to conversation and refresh webview */
    if (!win->ai_conversation_md)
        win->ai_conversation_md = g_string_new(NULL);
    char *prompt_md = g_strdup_printf("*>>> %s*\n\n---\n\n", text);
    g_string_append(win->ai_conversation_md, prompt_md);
    g_free(prompt_md);
    ai_refresh_output(win);

    /* Update status + start elapsed timer */
    win->ai_start_time = g_get_monotonic_time();
    gtk_label_set_text(win->ai_status_label, "thinking… 0.0s");
    if (win->ai_timer_id) g_source_remove(win->ai_timer_id);
    win->ai_timer_id = g_timeout_add(100, ai_timer_tick, win);

    /* Spawn: claude -p "text" --output-format json [--resume SESSION_ID]
     * Model is NOT passed — uses whatever the user configured via
     * "claude config set model ..." in the terminal.                  */
    GError *err = NULL;

    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, (gpointer)"claude");
    g_ptr_array_add(argv, (gpointer)"-p");
    g_ptr_array_add(argv, (gpointer)text);
    if (win->ai_session_id[0]) {
        g_ptr_array_add(argv, (gpointer)"--resume");
        g_ptr_array_add(argv, (gpointer)win->ai_session_id);
    }
    /* Restrict to CWD unless full disk access is enabled */
    static char restrict_prompt[4096];
    if (!win->settings.ai_full_disk_access) {
        const char *cwd = win->ai_cwd[0] ? win->ai_cwd : win->root_dir;
        if (cwd[0]) {
            snprintf(restrict_prompt, sizeof(restrict_prompt),
                "IMPORTANT: You must ONLY read, write, and modify files within "
                "the directory '%s' and its subdirectories. "
                "Do NOT access any files or directories outside of it.", cwd);
            g_ptr_array_add(argv, (gpointer)"--system-prompt");
            g_ptr_array_add(argv, (gpointer)restrict_prompt);
        }
    }
    g_ptr_array_add(argv, (gpointer)"--output-format");
    if (win->settings.ai_streaming) {
        g_ptr_array_add(argv, (gpointer)"stream-json");
        g_ptr_array_add(argv, (gpointer)"--verbose");
        g_ptr_array_add(argv, (gpointer)"--include-partial-messages");
    } else {
        g_ptr_array_add(argv, (gpointer)"json");
    }
    /* Add allowed tools.
     * In streaming mode: always pass ALL tools so the CLI never blocks on
     * stdin — the GUI intercepts tool_use events and shows a confirmation
     * dialog for tools that aren't auto-accepted.
     * In batch mode: only pass enabled tools when auto_accept is on. */
    if (win->settings.ai_streaming) {
        g_ptr_array_add(argv, (gpointer)"--allowed-tools");
        g_ptr_array_add(argv, (gpointer)"Edit");
        g_ptr_array_add(argv, (gpointer)"Write");
        g_ptr_array_add(argv, (gpointer)"Read");
        g_ptr_array_add(argv, (gpointer)"Glob");
        g_ptr_array_add(argv, (gpointer)"Grep");
        g_ptr_array_add(argv, (gpointer)"Bash");
    } else if (win->settings.ai_auto_accept) {
        gboolean any_tool = win->settings.ai_tool_read || win->settings.ai_tool_edit ||
                             win->settings.ai_tool_write || win->settings.ai_tool_glob ||
                             win->settings.ai_tool_grep || win->settings.ai_tool_bash;
        if (any_tool) {
            g_ptr_array_add(argv, (gpointer)"--allowed-tools");
            if (win->settings.ai_tool_edit)  g_ptr_array_add(argv, (gpointer)"Edit");
            if (win->settings.ai_tool_write) g_ptr_array_add(argv, (gpointer)"Write");
            if (win->settings.ai_tool_read)  g_ptr_array_add(argv, (gpointer)"Read");
            if (win->settings.ai_tool_glob)  g_ptr_array_add(argv, (gpointer)"Glob");
            if (win->settings.ai_tool_grep)  g_ptr_array_add(argv, (gpointer)"Grep");
            if (win->settings.ai_tool_bash)  g_ptr_array_add(argv, (gpointer)"Bash");
        }
    }
    g_ptr_array_add(argv, NULL);

    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);

    /* Use terminal's CWD as working directory for claude */
    const char *term_uri = vte_terminal_get_termprop_string(win->terminal, VTE_TERMPROP_CURRENT_DIRECTORY_URI, NULL);
    if (term_uri) {
        /* URI is file:///path — extract path */
        GFile *f = g_file_new_for_uri(term_uri);
        char *path = g_file_get_path(f);
        if (path) {
            g_strlcpy(win->ai_cwd, path, sizeof(win->ai_cwd));
            g_subprocess_launcher_set_cwd(launcher, path);
            g_free(path);
        }
        g_object_unref(f);
    } else if (win->root_dir[0]) {
        /* Fallback to opened folder */
        g_strlcpy(win->ai_cwd, win->root_dir, sizeof(win->ai_cwd));
        g_subprocess_launcher_set_cwd(launcher, win->root_dir);
    }

    win->ai_proc = g_subprocess_launcher_spawnv(launcher,
        (const gchar * const *)argv->pdata, &err);
    g_object_unref(launcher);
    g_ptr_array_unref(argv);

    if (!win->ai_proc) {
        g_string_append_printf(win->ai_conversation_md,
            "\n\n**Error:** %s\n\n", err ? err->message : "unknown");
        ai_refresh_output(win);
        if (err) g_error_free(err);
        g_free(text);
        gtk_label_set_text(win->ai_status_label, "error");
        return;
    }

    /* Init response buffer */
    if (!win->ai_response_buf)
        win->ai_response_buf = g_string_new(NULL);
    g_string_truncate(win->ai_response_buf, 0);

    if (win->settings.ai_streaming) {
        /* Streaming mode: read stdout line-by-line as events arrive */
        GInputStream *stdout_pipe = g_subprocess_get_stdout_pipe(win->ai_proc);
        win->ai_stream = g_data_input_stream_new(stdout_pipe);
        g_data_input_stream_read_line_async(win->ai_stream,
            G_PRIORITY_DEFAULT, win->cancellable, on_ai_stream_line_ready, win);
    } else {
        /* Batch mode: read all stdout when process exits */
        g_subprocess_communicate_async(win->ai_proc, NULL, win->cancellable,
                                        on_ai_communicate_done, win);
    }

    gtk_text_buffer_set_text(win->prompt_buffer, "", -1);
    g_free(text);
}

gboolean on_prompt_key(GtkEventControllerKey *ctrl, guint keyval,
                               guint keycode, GdkModifierType state, gpointer data) {
    (void)ctrl; (void)keycode;
    VibeWindow *win = data;

    if (keyval == GDK_KEY_Return) {
        if (win->settings.prompt_send_enter && !(state & GDK_CONTROL_MASK)) {
            send_prompt_to_ai(win);
            return TRUE;
        } else if (!win->settings.prompt_send_enter && (state & GDK_CONTROL_MASK)) {
            send_prompt_to_ai(win);
            return TRUE;
        }
    }
    return FALSE;
}
