/***************************************************************************
--          verbose.h - single-header C logging library     
--
--           Copyright (C) 2026 By Ulrik Hørlyk Hjort
--
-- Permission is hereby granted, free of charge, to any person obtaining
-- a copy of this software and associated documentation files (the
-- "Software"), to deal in the Software without restriction, including
-- without limitation the rights to use, copy, modify, merge, publish,
-- distribute, sublicense, and/or sell copies of the Software, and to
-- permit persons to whom the Software is furnished to do so, subject to
-- the following conditions:
--
-- The above copyright notice and this permission notice shall be
-- included in all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
-- EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
-- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
-- NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
-- LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
-- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
-- WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
-- ***************************************************************************/

/*
 * USAGE:
 *   #define VERBOSE_IMPLEMENTATION  (in exactly one .c file)
 *   #include "verbose.h"
 *
 * Optionally define VERBOSE_SOURCE_LOC before including to have
 * verbose_log() automatically inject __FILE__, __LINE__, __func__.
 *
 * API:
 *   int  verbose_init(int ts, int level, const char *filename);
 *   void verbose_set_level(int level);
 *   void verbose_set_tag(const char *tag);
 *   int  verbose_also_stderr(int enable);
 *   void verbose_log(int level, const char *fmt, ...);  // may be a macro
 *   void verbose_close(void);
 */

#ifndef VERBOSE_H
#define VERBOSE_H

/* Timestamp flags */
#define TS_ON  1
#define TS_OFF 0

/* Log level bitmask flags */
#define VB_DEBUG 0x01
#define VB_INFO  0x02
#define VB_WARN  0x04
#define VB_ERROR 0x08
#define VB_FATAL 0x10   /* logs then calls exit(1) */
#define VB_ALL   (VB_DEBUG | VB_INFO | VB_WARN | VB_ERROR | VB_FATAL)

/* Public API */

/*
 * verbose_init - initialise the logger.
 *   ts       : TS_ON or TS_OFF
 *   level    : bitmask of VB_* flags to enable (e.g. VB_WARN | VB_ERROR)
 *   filename : path to log file, or NULL to write to stderr
 *
 * Returns 0 on success, -1 on error.
 */
int verbose_init(int ts, int level, const char *filename);

/* Change the active log level at runtime (thread-safe). */
void verbose_set_level(int level);

/*
 * Set an optional tag printed after the level label, e.g. "[myapp]".
 * Pass NULL to clear it.
 */
void verbose_set_tag(const char *tag);

/*
 * verbose_also_stderr - mirror log output to stderr in addition to a file.
 *   enable : 1 to enable, 0 to disable
 * No-op when the logger was already initialised without a file.
 */
int verbose_also_stderr(int enable);

/* verbose_close - flush and close the log file (if any). */
void verbose_close(void);

/*
 * verbose_log - emit a log entry (printf-style).
 *   level : exactly one VB_* flag
 *   fmt   : printf format string
 *
 * When VERBOSE_SOURCE_LOC is defined this becomes a macro that
 * automatically injects __FILE__, __LINE__, __func__.
 */
#ifdef VERBOSE_SOURCE_LOC
void _verbose_log(int level, const char *file, int line, const char *func,
                  const char *fmt, ...);
#define verbose_log(level, ...) \
    _verbose_log(level, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
void verbose_log(int level, const char *fmt, ...);
#endif

/* ======================================================================= */
#ifdef VERBOSE_IMPLEMENTATION

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifndef VERBOSE_NO_THREADS
#include <pthread.h>
#endif

/* Internal state */
static struct {
    FILE           *fp_file;    /* open log file, or NULL  */
    FILE           *fp_err;     /* stderr mirror, or NULL  */
    int             level;
    int             ts;
    int             use_color;  /* 1 when fp_err is a color tty */
    char            tag[64];
#ifndef VERBOSE_NO_THREADS
    pthread_mutex_t lock;
#endif
} _vb = {
    NULL, NULL, 0, 0, 0, {0},
#ifndef VERBOSE_NO_THREADS
    PTHREAD_MUTEX_INITIALIZER,
#endif
};

/* ANSI color codes */
#define _VB_RESET    "\033[0m"
#define _VB_CYAN     "\033[36m"
#define _VB_GREEN    "\033[32m"
#define _VB_YELLOW   "\033[33m"
#define _VB_RED      "\033[31m"
#define _VB_BOLD_RED "\033[1;31m"

static const char *_vb_level_name(int level) {
    switch (level) {
        case VB_DEBUG: return "DEBUG";
        case VB_INFO:  return "INFO ";
        case VB_WARN:  return "WARN ";
        case VB_ERROR: return "ERROR";
        case VB_FATAL: return "FATAL";
        default:       return "?????";
    }
}

static const char *_vb_level_color(int level) {
    switch (level) {
        case VB_DEBUG: return _VB_CYAN;
        case VB_INFO:  return _VB_GREEN;
        case VB_WARN:  return _VB_YELLOW;
        case VB_ERROR: return _VB_RED;
        case VB_FATAL: return _VB_BOLD_RED;
        default:       return _VB_RESET;
    }
}

/* Internal write helper (caller must hold the lock) */
static void _vb_write(FILE *fp, int color, int level,
                      const char *file, int line, const char *func,
                      const char *fmt, va_list ap) {
    if (_vb.ts == TS_ON) {
        time_t     now = time(NULL);
        struct tm *tm  = localtime(&now);
        char       buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
        fprintf(fp, "[%s] ", buf);
    }

    if (color)
        fprintf(fp, "[%s%s%s] ", _vb_level_color(level), _vb_level_name(level), _VB_RESET);
    else
        fprintf(fp, "[%s] ", _vb_level_name(level));

    if (_vb.tag[0])
        fprintf(fp, "[%s] ", _vb.tag);

    if (file)
        fprintf(fp, "%s:%d (%s) ", file, line, func);

    vfprintf(fp, fmt, ap);
    fputc('\n', fp);
    fflush(fp);
}

/* API implementation */

int verbose_init(int ts, int level, const char *filename) {
    _vb.ts       = ts;
    _vb.level    = level;
    _vb.fp_file  = NULL;
    _vb.fp_err   = NULL;
    _vb.use_color = 0;

    if (filename) {
        _vb.fp_file = fopen(filename, "a");
        if (!_vb.fp_file) {
            fprintf(stderr, "[verbose] failed to open log file: %s\n", filename);
            return -1;
        }
    } else {
        _vb.fp_err    = stderr;
        _vb.use_color = isatty(fileno(stderr));
    }

    return 0;
}

void verbose_set_level(int level) {
#ifndef VERBOSE_NO_THREADS
    pthread_mutex_lock(&_vb.lock);
#endif
    _vb.level = level;
#ifndef VERBOSE_NO_THREADS
    pthread_mutex_unlock(&_vb.lock);
#endif
}

void verbose_set_tag(const char *tag) {
#ifndef VERBOSE_NO_THREADS
    pthread_mutex_lock(&_vb.lock);
#endif
    strncpy(_vb.tag, tag ? tag : "", sizeof(_vb.tag) - 1);
    _vb.tag[sizeof(_vb.tag) - 1] = '\0';
#ifndef VERBOSE_NO_THREADS
    pthread_mutex_unlock(&_vb.lock);
#endif
}

int verbose_also_stderr(int enable) {
#ifndef VERBOSE_NO_THREADS
    pthread_mutex_lock(&_vb.lock);
#endif
    if (enable) {
        _vb.fp_err    = stderr;
        _vb.use_color = isatty(fileno(stderr));
    } else {
        _vb.fp_err    = NULL;
        _vb.use_color = 0;
    }
#ifndef VERBOSE_NO_THREADS
    pthread_mutex_unlock(&_vb.lock);
#endif
    return 0;
}

#ifdef VERBOSE_SOURCE_LOC
void _verbose_log(int level, const char *file, int line, const char *func,
                  const char *fmt, ...)
#else
void verbose_log(int level, const char *fmt, ...)
#endif
{
    if (!(_vb.level & level))
        return;
    if (!_vb.fp_file && !_vb.fp_err)
        return;

#ifndef VERBOSE_NO_THREADS
    pthread_mutex_lock(&_vb.lock);
#endif

    va_list ap;

    if (_vb.fp_file) {
        va_start(ap, fmt);
#ifdef VERBOSE_SOURCE_LOC
        _vb_write(_vb.fp_file, 0, level, file, line, func, fmt, ap);
#else
        _vb_write(_vb.fp_file, 0, level, NULL, 0, NULL, fmt, ap);
#endif
        va_end(ap);
    }

    if (_vb.fp_err) {
        va_start(ap, fmt);
#ifdef VERBOSE_SOURCE_LOC
        _vb_write(_vb.fp_err, _vb.use_color, level, file, line, func, fmt, ap);
#else
        _vb_write(_vb.fp_err, _vb.use_color, level, NULL, 0, NULL, fmt, ap);
#endif
        va_end(ap);
    }

    int is_fatal = (level == VB_FATAL);
#ifndef VERBOSE_NO_THREADS
    pthread_mutex_unlock(&_vb.lock);
#endif

    if (is_fatal) {
        verbose_close();
        exit(1);
    }
}

void verbose_close(void) {
    if (_vb.fp_file) {
        fclose(_vb.fp_file);
        _vb.fp_file = NULL;
    }
    _vb.fp_err = NULL;
}

#endif /* VERBOSE_IMPLEMENTATION */
#endif /* VERBOSE_H */
