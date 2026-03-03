/***************************************************************************
--      verbose.h - single-header C logging library example of usage
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

/* Optionally enable source location injection (__FILE__/__LINE__/__func__) */
#define VERBOSE_SOURCE_LOC

#define VERBOSE_IMPLEMENTATION
#include "verbose.h"

#include <stdio.h>

int main(void) {
    /* 1. Basic usage: all levels to stderr (colors if tty) */
    verbose_init(TS_ON, VB_ALL, NULL);

    verbose_log(VB_DEBUG, "starting up, pid=%d", 42);
    verbose_log(VB_INFO,  "server listening on port %d", 8080);
    verbose_log(VB_WARN,  "config file not found, using defaults");
    verbose_log(VB_ERROR, "failed to connect to database: %s", "timeout");

    verbose_close();

    /* 2. Tag + runtime level change */
    verbose_init(TS_OFF, VB_ALL, NULL);
    verbose_set_tag("my_program");

    verbose_log(VB_INFO, "tag is shown before the message");

    /* Silence debug at runtime */
    verbose_set_level(VB_INFO | VB_WARN | VB_ERROR | VB_FATAL);
    verbose_log(VB_DEBUG, "this will NOT appear (level changed at runtime)");
    verbose_log(VB_INFO,  "this will appear");

    verbose_set_tag(NULL);
    verbose_close();

    /* 3. File + mirrored stderr (dual output) */
    if (verbose_init(TS_ON, VB_WARN | VB_ERROR, "/tmp/verbose_example.log") == 0) {
        verbose_also_stderr(1);   /* mirror to stderr as well */

        verbose_log(VB_DEBUG, "filtered out — not in file or stderr");
        verbose_log(VB_WARN,  "disk usage above 90%%");
        verbose_log(VB_ERROR, "out of memory");

        verbose_close();

        printf("\n--- /tmp/verbose_example.log ---\n");
        FILE *f = fopen("/tmp/verbose_example.log", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f))
                fputs(line, stdout);
            fclose(f);
        }
    }

    /* 4. VB_FATAL: logs then exits  */
    verbose_init(TS_OFF, VB_ALL, NULL);
    verbose_log(VB_FATAL, "unrecoverable error — goodbye");

    return 0;
}
