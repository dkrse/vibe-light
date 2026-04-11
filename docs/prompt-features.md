# Prompt Features

## AI Prompt Input

The prompt input area (bottom of the AI tab) supports configurable send behavior and font settings.

### Send Key

Configurable in Settings > Prompt:

- **Enter** -- sends prompt on Enter, Shift+Enter for newline
- **Ctrl+Enter** -- sends prompt on Ctrl+Enter, Enter for newline

Key capture uses `GTK_PHASE_CAPTURE` on the prompt view's key controller for reliable handling.

### Show Terminal After Send

Optional toggle: after sending a prompt, automatically switch to the Terminal tab. Useful when using Claude CLI with tool access that modifies files.

### Prompt Font

Independent font family and size for the prompt input area, configurable in Settings > Prompt.

## AI Model Configuration

Accessible via Menu > AI Model:

### Tool Access

Individual toggles for Claude CLI tools:

| Tool | Description |
|------|-------------|
| Read | Read files from disk |
| Edit | Edit existing files |
| Write | Create new files |
| Glob | Find files by pattern |
| Grep | Search file contents |
| Bash | Execute shell commands |

Tools are passed as `--allowedTools` flags to the `claude` command.

### Auto-Accept & Tool Confirmation Dialogs

Controls how tool-use requests are handled in streaming mode:

| Auto-Accept | Tool Enabled | Behavior |
|-------------|-------------|----------|
| ON | YES | Auto-allow (no dialog) |
| ON | NO | Show confirmation dialog |
| OFF | any | Always show confirmation dialog |

The confirmation dialog is a modal GTK window (`vibe_dialog_new`) showing:
- Tool name (e.g. "Read", "Bash", "Edit")
- Key parameters as compact labels (file_path, command, pattern, etc. -- not raw JSON)
- **Allow** button (suggested-action) -- continues stream reading
- **Deny** button (destructive-action) -- kills the AI process (`g_subprocess_force_exit`)

In streaming mode, `--allowed-tools` always passes all 6 tools to the CLI to prevent it from blocking on stdin. The GUI handles the approval flow instead.

In batch mode (non-streaming), no dialogs are shown -- Claude CLI handles its own confirmation in the terminal.

### Full Disk Access

When disabled, a system prompt restricts Claude to the terminal's current working directory. When enabled, no CWD restriction is applied.

### Streaming Mode

- **Interactive streaming** (default) -- uses `--output-format stream-json`. Responses stream in real-time, line-by-line via `GDataInputStream`. Text deltas appended directly to WebKit DOM via JS (`insertAdjacentText`) for performance. Full cmark-gfm markdown re-render on stream completion.
- **Batch mode** -- uses `--output-format json`. Waits for the complete response before displaying.

### Markdown Rendering

When enabled, AI responses are rendered as full HTML via WebKitWebView + cmark-gfm:

- Tables, code blocks, headings, bold, italic, links
- Blockquotes, lists, strikethrough, horizontal rules
- LaTeX to Unicode conversion (e.g. `$\sum$` -> summation, `$E = mc^2$` -> superscript)
- Theme-aware CSS (dark/light automatically matches app theme)
- HTML sanitized -- `CMARK_OPT_UNSAFE` is not used, raw HTML in AI responses is escaped

When disabled, raw text is displayed.

## Conversation Memory

The conversation buffer (`ai_conversation_md`) is capped at 256KB. When the buffer exceeds this limit, the first half is trimmed at the nearest newline boundary, and a `"(earlier conversation trimmed)"` marker is prepended. This prevents unbounded RAM growth during long AI sessions while preserving the most recent context.

## Session Management

### Session Persistence

- Session ID saved to `settings.conf` on each AI response
- Restored on app restart via `--resume SESSION_ID`
- Start time and turn count also persisted

### Session Auto-Recovery

If a saved session has expired ("No conversation found" error), the app automatically:
1. Clears the expired session ID
2. Retries the prompt with a new session

### Session Info Popover

Click the session label in the AI tab status bar to see:
- Session ID (selectable text)
- Started date/time
- Duration
- Turn count
- Token usage (input/output)
- Mode (streaming/batch)

### New Session / Resume

In the AI Model dialog:
- **New Session** -- clears session ID, next prompt starts fresh
- **Resume** -- enter a session ID to resume a previous conversation

## Conversation Logging

All prompts and responses are logged to `{project}/.LLM/prompts.json`:

```json
[
  {"id": 1, "type": "input", "timestamp": "2026-04-09T14:30:22", "session": "abc123", "model": "claude-sonnet-4-6", "text": "explain this code"},
  {"id": 2, "type": "output", "timestamp": "2026-04-09T14:30:28", "session": "abc123", "model": "claude-sonnet-4-6", "input_tokens": 1250, "output_tokens": 830, "total_tokens": 2080, "elapsed_seconds": 6.84, "text": "This code..."}
]
```

Input entries are logged after the response arrives so that model and session metadata are accurate. Writes are atomic (temp file + `rename()`) to prevent corruption on crash.

### Token Tracking

Displayed in the AI tab header:
- Input tokens / Output tokens / Total tokens
- Dynamic formatting: raw numbers, K (thousands), M (millions)
- Elapsed time for last request (seconds or minutes)

## LaTeX to Unicode

Math expressions in AI output are converted to Unicode:

| LaTeX | Unicode |
|-------|---------|
| `\sum` | summation |
| `\prod` | product |
| `\alpha`, `\beta`, `\gamma` ... | Greek letters |
| `\rightarrow`, `\leftarrow` | Arrows |
| `^2`, `^n` | Superscripts |
| `_{i}`, `_{n}` | Subscripts |
| `\frac{a}{b}` | (a)/(b) |
| `\text{word}` | word (plain text) |
| `\infty` | infinity |
