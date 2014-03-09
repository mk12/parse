#ifndef SIMPLEIO_H
#define SIMPLEIO_H

#include <stddef.h>  // size_t

// Constants
extern const char *const SIO_PARSE_BADINPUT;

// Main
int sio_main(int argc, char **argv, int (*func)(int, char **), int n,
             const char *usage_args);

// Parsing
int sio_parse_int(int *ptr, const char *s);
int sio_parse_long(long *ptr, const char *s);
int sio_parse_float(float *ptr, const char *s);
int sio_parse_double(double *ptr, const char *s);

// Error reporting
int sio_error(const char *errmsg);
int sio_errorc(const char *context, const char *errmsg);
int sio_errorf(const char *format, ...) __attribute__((format(printf, 1, 2)));

// Memory management
void *sio_malloc(size_t size);
void *sio_realloc(void *ptr, size_t size);

#endif
