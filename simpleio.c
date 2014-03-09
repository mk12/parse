// Copyright 2012 Mitchell Kember. Subject to the MIT License.

#include "simpleio.h"

#include <assert.h>   // assertions
#include <ctype.h>    // isspace
#include <errno.h>    // errno
#include <float.h>    // HUGE_VAL
#include <limits.h>   // INT_MAX, etc.
#include <math.h>     // isinf, isnan
#include <stdarg.h>   // varargs
#include <stdbool.h>  // bool
#include <stdio.h>    // input/output
#include <stdlib.h>   // memory management
#include <string.h>   // strlen, strcmp

// isatty (portable to POSIX and Windows)
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#define isatty _isatty
#endif

// -----------------------------------------------------------------------------
//                 Macros
// -----------------------------------------------------------------------------

// Portable eXit Status: replace the conventional 0/1 with more portable macros.
#define PXS(s) ((s) == 0 ? EXIT_SUCCESS : EXIT_FAILURE)

// A maximum buffer size (in bytes) to limit growth of buffers.
#define MAX_BUFSIZE 1024

// -----------------------------------------------------------------------------
//                 Constants
// -----------------------------------------------------------------------------

/*
 * A common error message for use in parsers. When a string argument 'str'
 * cannot be parsed because it contains invalid characters, e.g. parsing "12a3"
 * as an int, use 'sio_erroc(str, SIO_PARSE_BADINPUT)' to report the error.
 */
const char *const SIO_PARSE_BADINPUT = "bad input";

// -----------------------------------------------------------------------------
//                 Globals
// -----------------------------------------------------------------------------

static const char *g_progname;        // program name
static enum { STDIN, ARGS } g_input;  // method of input

// -----------------------------------------------------------------------------
//                 Private prototypes
// -----------------------------------------------------------------------------

static int foreach_line(FILE *stream, int (*func)(int, char **), int n);
static int split_words(char ***wordsptr, int *len, char *str);
static size_t simple_fgetline(char **lineptr, size_t *size, FILE *stream);

// =============================================================================
//                 Main
// =============================================================================

/*
 * TODO: document this
 */
int sio_main(int argc, char **argv, int (*func)(int, char **), int n,
             const char *usage_args) {
    assert(argv);
    assert(func);
    assert(n >= 0);
    assert(usage_args);

    // Set the program name to the basename of the invocation path.
    g_progname = strrchr(argv[0], '/');
    // If there's no slash (POSIX path separator), or if it's the last
    // character, just use the whole invocation path.
    if (!g_progname || !*++g_progname)
        g_progname = argv[0];

    // Create the usage message. Note that 'usagemsg' is a VLA (C99).
    char usagemsg[10+strlen(g_progname)+strlen(usage_args)];
    sprintf(usagemsg, "usage: %s %s\n", g_progname, usage_args); 

    // Display the usage message when invoked with "-h" or "--help".
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 ||
                      strcmp(argv[1], "--help") == 0)) {
        fputs(usagemsg, stdout);
        return PXS(0);
    }
    
    // Read from stdin when input is not interactive, or when invoked with "-".
    if ((argc == 1 && !isatty(0)) || (argc == 2 && strcmp(argv[1], "-") == 0)) {
        g_input = STDIN;
        return PXS(foreach_line(stdin, func, n));
    }

    // Use the command line arguments if they are correct in number.
    if ((n == 0 && argc > 1) || (n > 0 && argc - 1 == n)) {
        g_input = ARGS;
        return PXS(func(argc - 1, argv + 1));
    }

    // Wrong number of arguments
    fputs(usagemsg, stderr);
    return PXS(1);
}

// =============================================================================
//                 Parsers
// =============================================================================

/*
 * Parses the string 's' as an int and stores the result in '*ptr'. Returns 0 on
 * success and 1 on failure. Note that when this function fails, it prints an
 * error message via 'sio_errorc' before returning.
 */
int sio_parse_int(int *ptr, const char *s) {
    assert(s);
    char *end;
    long val;
    errno = 0;
    val = strtol(s, &end, 0);
    if (*s == '\0' || *end != '\0')
        return sio_errorc(s, SIO_PARSE_BADINPUT);
    if ((errno == ERANGE && val == LONG_MAX) || val > INT_MAX)
        return sio_errorc(s, "int overflow");
    if ((errno == ERANGE && val == LONG_MIN) || val < INT_MIN)
        return sio_errorc(s, "int underflow");
    *ptr = (int)val;
    return 0;
}

/*
 * Parses the string 's' as a long and stores the result in '*ptr'. Returns 0 on
 * success and 1 on failure. Note that when this function fails, it prints an
 * error message via 'sio_errorc' before returning.
 */
int sio_parse_long(long *ptr, const char *s) {
    assert(s);
    char *end;
    long val;
    errno = 0;
    val = strtol(s, &end, 0);
    if (*s == '\0' || *end != '\0')
        return sio_errorc(s, SIO_PARSE_BADINPUT);
    if (errno == ERANGE) {
        if (val == LONG_MAX) return sio_errorc(s, "long overflow");
        if (val == LONG_MIN) return sio_errorc(s, "long underflow");
    }
    *ptr = val;
    return 0;
}

/*
 * Parses the string 's' as a float and stores the result in '*ptr'. Returns 0
 * on success and 1 on failure. Note that when this function fails, it prints an
 * error message via 'sio_errorc' before returning.
 */
int sio_parse_float(float *ptr, const char *s) {
    assert(s);
    char *end;
    float val;
    errno = 0;
    val = strtof(s, &end);
    if (*s == '\0' || *end != '\0' || isinf(val) || isnan(val))
        return sio_errorc(s, SIO_PARSE_BADINPUT);
    if (errno == ERANGE) {
        if (val >= +HUGE_VALF) return sio_errorc(s, "float overflow");
        if (val <= -HUGE_VALF) return sio_errorc(s, "float underflow");
    }
    *ptr = val;
    return 0;
}

/*
 * Parses the string 's' as a double and stores the result in '*ptr'. Returns 0
 * on success and 1 on failure. Note that when this function fails, it prints an
 * error message via 'sio_errorc' before returning.
 */
int sio_parse_double(double *ptr, const char *s) {
    assert(s);
    char *end;
    double val;
    errno = 0;
    val = strtod(s, &end);
    if (*s == '\0' || *end != '\0' || isinf(val) || isnan(val))
        return sio_errorc(s, SIO_PARSE_BADINPUT);
    if (errno == ERANGE) {
        if (val >= +HUGE_VAL) return sio_errorc(s, "double overflow");
        if (val <= -HUGE_VAL) return sio_errorc(s, "double underflow");
    }
    *ptr = val;
    return 0;
}

// =============================================================================
//                 Error reporting
// =============================================================================

/*
 * Prints an error message to stderr using the appropriate format, and returns 1
 * for convenience (i.e. 'return error("message")' in the main function both
 * prints the error message and returns exit failure, which is usually what you
 * want).
 */
int sio_error(const char *errmsg) {
    const char *prefix = (g_input == STDIN) ? "error" : g_progname;
    fprintf(stderr, "%s: %s\n", prefix, errmsg);
    return 1;
}

/*
 * Prints an error message to stderr with context. The 'context' string,
 * followed by a colon and a space, is printed immediately before the 'errmsg'
 * string. See the 'sio_error' function.
 */
int sio_errorc(const char *context, const char *errmsg) {
    return sio_errorf("%s: %s", context, errmsg);
}

/*
 * Prints a formatted error message. Use it as you would use 'printf'. See the
 * 'sio_error' function.
 */
int sio_errorf(const char *format, ...) {
    if (g_input == STDIN) fputs("error: ", stderr);
    else fprintf(stderr, "%s: ", g_progname);

    // Pass on the varargs on to 'vfprintf'.
    va_list arglist;
    va_start(arglist, format);
    vfprintf(stderr, format, arglist);
    va_end(arglist);
    putc('\n', stderr);

    return 1;
}

// =============================================================================
//                 Memory management
// =============================================================================

/*
 * A wrapper around 'malloc' which does error reporting. Always use this
 * function instead of 'malloc'. You can safely assume that this function will
 * always succeed, because if it does not, it will exit the program rather than
 * returning.
 */
void *sio_malloc(size_t size) {
    assert(size > 0);
    void *ptr = malloc(size);
    if (!ptr) exit(PXS(sio_errorf("malloc: cannot allocate %zuB", size)));
    return ptr;
}

/*
 * A wrapper around 'realloc' which does error reporting. Always use this
 * instead of 'realloc'. You can safely assume that this function will always
 * succeed, because if it does not, it will exit the program rather than
 * returning.
 */
void *sio_realloc(void *ptr, size_t size) {
    assert(ptr);
    assert(size > 0);
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        free(ptr);
        exit(PXS(sio_errorf("realloc: cannot allocate %zuB", size)));
    }
    return new_ptr;
}

// =============================================================================
//                 Private functions
// =============================================================================

/*
 * Reads from 'stream' one line at a time, splits the input by words using
 * 'split_words', and passes them to 'func', which expects 'n' words (see
 * 'sio_main' for more details on the 'n' argument). The function 'func' should
 * parse the strings, print the appropriate output or error message ending with
 * a newline, and return 0 on success and 1 on failure.
 *
 * This function returns 0 if all the calls to 'func' returned 0, otherwise it
 * returns 1 to indicate that one or more calls were unsuccessful.
 */
static int foreach_line(FILE *stream, int (*func)(int, char **), int n) {
    assert(stream);
    assert(func);
    assert(n >= 0);

    int status = 0;

    // Line buffer
    char *buf = NULL;
    size_t bufsize;
    size_t nread;

    // Word pointers buffer
    char **words = NULL;
    int words_len;
    int nwords;

    while ((nread = simple_fgetline(&buf, &bufsize, stream)) != 0) {
        if (*buf == '\n')
            continue;
        nwords = split_words(&words, &words_len, buf);

        // Simply OR the statuses together since they are all 0 or 1.
        if (n > 0 && nwords < n)
            status |= sio_error("too few arguments");
        else if (n > 0 && nwords > n)
            status |= sio_error("too many arguments");
        else
            status |= func(nwords, words);

        // If the buffers get too big, free them -- they'll automatically get
        // reallocated by 'simple_fgetline' and 'split_words' to the base size.
        if (bufsize > MAX_BUFSIZE) {
            free(buf);
            buf = NULL;
        }
        if (words_len > MAX_BUFSIZE / (int)sizeof(char *)) {
            free(words);
            words = NULL;
        }
    }
    free(buf);
    free(words);
    return status;
}

/*
 * Splits the words in 'str' by nulling each whitespace character found
 * immediately after a non-whitespace character, and for each word storing a
 * pointer to its beginning. Whether a character is considered as whitespace is
 * determined by the 'isspace' function. Spaces enclosed in quote characters or
 * preceded by backslashes are not counted as word separators. All backslashes
 * and quote pairs are removed from the words, with the exception of backslashes
 * and quote characters escaped with a backslash.
 *
 * The address of the buffer of pointers is stored in '*wordsptr'. The buffer
 * will be terminated by a null pointer. If '*wordsptr' is NULL, this function
 * will allocate a buffer for storing the pointers, WHICH YOU ARE RESPONSIBLE
 * FOR FREEING. In this case, the value of '*len' is ignored.
 *
 * Alternatively, before calling this function, '*wordsptr' can contain a
 * pointer to a 'malloc'-allocated buffer '*n * sizeof(char *)' bytes in size
 * ('*len' myst be greater than 1). If the buffer is not large enough to hold
 * all the pointers, it will be resized with 'sio_realloc', updating
 * '*wordsptr' and '*len' as necessary.
 *
 * In either case, on a succesful call, '*wordsptr' and '*len' will be updated
 * to reflect the buffer address and size respectively.
 *
 * On success, this function returns the number of words found. The return
 * value will always be positive and nonzero (except when 'str' is empty, in
 * which case this function will return 0).
 */
static int split_words(char ***wordsptr, int *len, char *str) {
    assert(wordsptr);
    assert(len);
    assert(str);

    const int block_len = 16;
    int words_len;
    char **words;

    if (*wordsptr) {
        assert(*len > 1);
        words_len = *len;
        words = *wordsptr;
    } else {
        words_len = block_len;
        words = sio_malloc((unsigned)words_len * sizeof(char *));
    }

    // The following monstrosity imitates the shell's handling of backslashes
    // and quotes. I've never written any kind of parser before this, so maybe
    // this can be rewritten in a cleaner way.
    int nwords = 0;
    int previous = -1;
    int quote = -1;
    int shift = 0;
    enum { WHITE, WORD } status = WHITE;
    while (*str) {
        bool delete = false;
        bool prevbs = (previous == '\\');
        previous = *str;
        if (*str == '\\') {
            if (prevbs)
                previous = -1;
            else
                delete = true;
        }
        if (!prevbs) {
            if (quote == -1) {
                int whitespace = isspace(*str);
                if (status == WORD && whitespace) {
                    status = WHITE;
                    *str = '\0';
                } else {
                    if (*str == '\'' || *str == '\"') {
                        quote = *str;
                        delete = true;
                    }
                    if (status == WHITE && !whitespace) {
                        status = WORD;
                        // Save a slot for the terminating null pointer.
                        if (nwords + 1 == words_len) {
                            words_len += block_len;
                            words = sio_realloc(words, (unsigned)words_len *
                                                       sizeof(char *));
                        }
                        words[nwords++] = str - shift;
                    }
                }
            } else if (quote == *str) {
                quote = -1;
                delete = true;
            }
        }
        if (delete)
            shift++;
        else
            *(str - shift) = *str;
        str++;
    }
    *(str - shift) = '\0';
    words[nwords] = NULL;

    *wordsptr = words;
    *len = words_len;
    return nwords;
}

/*
 * Reads bytes from 'stream' into a buffer until a newline character (unless
 * inside quotation marks or following a backlash) is read and transferred, or
 * an end-of-file condition is encountered. If the newline character is preceded
 * by an odd number of backslash characters, neither the newline nor the
 * backslash immediately preceding it will be stored in the buffer.
 *
 * The address of the buffer is stored into '*lineptr'. The buffer is
 * null-termianted and includes the terminating newline character, if one was
 * found. If '*lineptr' is NULL, this function will allocate a buffer for
 * storing the line, WHICH YOU ARE RESPONSIBLE FOR FREEING. In this case, the
 * value in '*size' is ignored.
 *
 * Alternatively, before calling this function, '*lineptr' can contain a pointer
 * to a 'malloc'-allocated buffer '*size' bytes in size ('*size' must be greater
 * than 1). If the buffer is not large enough to hold the line, it will be
 * resized with 'sio_realloc', updating '*lineptr' and '*size' as necessary.
 *
 * In either case, on a successful call, '*lineptr' and '*size' will be updated
 * to reflect the buffer address and size respectively.
 *
 * On success, this function returns the number of characters read, including
 * the newline character if one was found before end-of-file condition, but not
 * includng the terminating null byte. The only time the return value will be
 * zero is on end-of-file condition or when a read error occurs.
 */
static size_t simple_fgetline(char **lineptr, size_t *size, FILE *stream) {
    assert(lineptr);
    assert(size);
    assert(*size > 1);
    assert(stream);

    int c = getc(stream);
    if (c == EOF) return 0;

    const size_t block_size = 128;
    char *buf;
    size_t bufsize;

    if (*lineptr) {
        buf = *lineptr;
        bufsize = *size;
    } else {
        bufsize = block_size;
        buf = sio_malloc(bufsize);
    }

    size_t i = 0;
    int previous = -1;
    int quote = -1;
    while (c != EOF) {
        if (i == bufsize) {
            bufsize += block_size;
            buf = sio_realloc(buf, bufsize);
        }
        buf[i++] = (char)c;
        if (previous == '\\' && c == '\n') {
            i -= 2;
        } else if (quote == -1) {
            if (c == '\n')
                break;
            else if (previous != '\\' && (c == '\'' || c == '\"'))
                quote = c;
        } else if (previous != '\\' && c == quote) {
            quote = -1;
        }
        if (previous == '\\' && c == '\\')
            previous = -1;
        else
            previous = c;
        c = getc(stream);
    }
    buf[i] = '\0';

    *lineptr = buf;
    *size = bufsize;
    return i;
}
