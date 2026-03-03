# verbose.h

A single-header logging library for C.

---

## Features

- Single-header
- Log levels as bitmask flags — combine freely with `|`
- Optional timestamps
- Optional source location (`__FILE__`, `__LINE__`, `__func__`)
- ANSI colors when writing to a terminal
- Log to file, stderr, or both simultaneously
- Runtime log level changes
- Optional tag/prefix per logger instance
- `VB_FATAL` - log then `exit(1)`
- Thread-safe via `pthread_mutex`

---

## Quick start

Define `VERBOSE_IMPLEMENTATION` before including:

```c
#define VERBOSE_IMPLEMENTATION
#include "verbose.h"
```


## API

### `verbose_init`

```c
int verbose_init(int ts, int level, const char *filename);
```

Initialises the logger. Call once at startup.

| Parameter  | Description |
|------------|-------------|
| `ts`       | `TS_ON` or `TS_OFF` — whether to prefix each line with a timestamp |
| `level`    | Bitmask of `VB_*` flags to enable |
| `filename` | Path to a log file, or `NULL` to write to `stderr` |

Returns `0` on success, `-1` on failure (e.g. file could not be opened).

---

### `verbose_log`

```c
void verbose_log(int level, const char *fmt, ...);
```

Emits a log line. Uses printf-style formatting. The line is only written if
`level` is set in the active level mask.

```c
verbose_log(VB_INFO,  "listening on port %d", port);
verbose_log(VB_WARN,  "retrying in %d seconds", delay);
verbose_log(VB_ERROR, "connection failed: %s", strerror(errno));
verbose_log(VB_FATAL, "cannot continue: %s", reason); /* logs then exits */
```

---

### `verbose_close`

```c
void verbose_close(void);
```

Flushes and closes the log file. Safe to call when logging to `stderr`.

---

### `verbose_set_level`

```c
void verbose_set_level(int level);
```

Changes the active log level mask at runtime. Thread-safe. Useful for
toggling debug output in a running process.

```c
verbose_set_level(VB_WARN | VB_ERROR); /* quiet down */
verbose_set_level(VB_ALL);             /* full verbosity again */
```

---

### `verbose_set_tag`

```c
void verbose_set_tag(const char *tag);
```

Sets an optional tag printed between the level label and the message.
Pass `NULL` to clear.

```c
verbose_set_tag("db");
verbose_log(VB_INFO, "connected");
/* [INFO ] [db] connected */
```

---

### `verbose_also_stderr`

```c
int verbose_also_stderr(int enable);
```

When the logger was initialised with a file, mirrors output to `stderr` as
well. Pass `0` to disable mirroring.

```c
verbose_init(TS_ON, VB_ALL, "/var/log/app.log");
verbose_also_stderr(1); /* also print to terminal */
```

---

## Log levels

| Flag       | Value  | Meaning |
|------------|--------|---------|
| `VB_DEBUG` | `0x01` | Verbose debug information |
| `VB_INFO`  | `0x02` | General informational messages |
| `VB_WARN`  | `0x04` | Warnings that may need attention |
| `VB_ERROR` | `0x08` | Errors that should not happen |
| `VB_FATAL` | `0x10` | Unrecoverable error — logs then calls `exit(1)` |
| `VB_ALL`   | `0x1F` | All of the above |

Levels are bitmask flags, so you can combine them freely:

```c
verbose_init(TS_ON, VB_WARN | VB_ERROR, "/tmp/app.log");
```

---

## Source location

Define `VERBOSE_SOURCE_LOC` before including the header to have
`verbose_log()` automatically inject `file:line (function)` into every line:

```c
#define VERBOSE_SOURCE_LOC
#define VERBOSE_IMPLEMENTATION
#include "verbose.h"
```

Example output:
```
[2024-01-15 12:00:00] [WARN ] src/main.c:42 (main) config not found
```

---

## Output format

```
[TIMESTAMP] [LEVEL] [TAG] file:line (func) message
```

- **TIMESTAMP** - only when `TS_ON` was passed to `verbose_init`
- **TAG** - only when set via `verbose_set_tag`
- **file:line (func)** - only when `VERBOSE_SOURCE_LOC` is defined

Minimal example (no timestamp, no tag, no source location):
```
[WARN ] disk usage above 90%
```

Full example:
```
[2024-01-15 12:00:00] [WARN ] [my_program] src/disk.c:88 (check_disk) disk usage above 90%
```

When writing to a terminal, level labels are colorized:

| Level   | Color        |
|---------|--------------|
| `DEBUG` | Cyan         |
| `INFO`  | Green        |
| `WARN`  | Yellow       |
| `ERROR` | Red          |
| `FATAL` | Bold red     |

---

## Examples

### Log everything to stderr

```c
#define VERBOSE_IMPLEMENTATION
#include "verbose.h"

int main(void) {
    verbose_init(TS_ON, VB_ALL, NULL);

    verbose_log(VB_DEBUG, "starting up");
    verbose_log(VB_INFO,  "ready");
    verbose_log(VB_WARN,  "something looks off");
    verbose_log(VB_ERROR, "something went wrong: %s", "disk full");

    verbose_close();
    return 0;
}
```

### Log warnings and errors to a file

```c
verbose_init(TS_ON, VB_WARN | VB_ERROR, "/var/log/my_program.log");
verbose_log(VB_DEBUG, "not written — filtered out");
verbose_log(VB_WARN,  "written to file");
verbose_close();
```

### File + stderr mirror

```c
verbose_init(TS_ON, VB_ALL, "/var/log/my_program.log");
verbose_also_stderr(1);

verbose_log(VB_INFO, "goes to both file and terminal");
verbose_close();
```

### Tag per subsystem

```c
verbose_init(TS_OFF, VB_ALL, NULL);

verbose_set_tag("http");
verbose_log(VB_INFO, "GET /index.html 200");

verbose_set_tag("db");
verbose_log(VB_WARN, "slow query: %dms", 430);
```

### Fatal error

```c
if (!config) {
    verbose_log(VB_FATAL, "failed to load config — aborting");
    /* exit(1) is called automatically, line below is never reached */
}
```

---

## Man page

A man page is included as `verbose.3`. Install it with:

```sh
sudo make install-man
```

This copies the page to `/usr/local/man/man3`. Override the destination with `MANDIR`:

```sh
sudo make install-man MANDIR=/usr/share/man/man3
```

Once installed, view it with:

```sh
man 3 verbose
```

---

## Compiling

Link against `pthread` (required unless `VERBOSE_NO_THREADS` is defined):

```sh
gcc my_program.c -lpthread -o my_program
```

### Disabling thread safety

If your program is single-threaded and you don't want the `pthread` dependency,
define `VERBOSE_NO_THREADS` before including the header:

```c
#define VERBOSE_NO_THREADS
#define VERBOSE_IMPLEMENTATION
#include "verbose.h"
```

Then no `-lpthread` is needed:

```sh
gcc my_program.c -o my_program
```
