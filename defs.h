/* TextGameServer/defs.h */
#ifndef DEFS_SENTRY
#define DEFS_SENTRY

#include <stdint.h>
#include <string.h>
#include <errno.h>

typedef int       bool;
#define true      1
#define false     0
 
#define LOG_ERR(_fmt, ...) fprintf(stderr, "[ERR] (%s:%d: errno: %s) " _fmt "\n", \
        __FILE__, __LINE__, (errno == 0 ? "None" : strerror(errno)), ##__VA_ARGS__)

#define ASSERT(_e) if(!(_e)) { fprintf(stderr, "Assertion failed at %s:%d\n", __FILE__, __LINE__); exit(1); }
#define ASSERTF(_e, _fmt, ...) if(!(_e)) { fprintf(stderr, _fmt, ##__VA_ARGS__); exit(1); }
#define ASSERT_ERR(_e) if(!(_e)) { LOG_ERR("Assertion failed at %s:%d\n", __FILE__, __LINE__); exit(1); }
#define ASSERTF_ERR(_e, _fmt, ...) if(!(_e)) { LOG_ERR(_fmt, ##__VA_ARGS__); exit(1); }

#endif
