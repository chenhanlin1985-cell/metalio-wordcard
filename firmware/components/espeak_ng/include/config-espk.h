#pragma once
#define _USE_MATH_DEFINES
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#define PACKAGE_VERSION "1.52-dev"
#define PACKAGE_STRING "eSpeak NG 1.52-dev"
#define PACKAGE_NAME "eSpeak NG"
#define PACKAGE "espeak-ng"
#define VERSION "1.52-dev"
#define PATH_ESPEAK_DATA "/sdcard/metalio/e-ink/wordcards/tts/espeak-ng-data"
#define ESPEAK_STACK_HACK 1
#define STACK_T static
#define ESPEAK_ARDUINO_POSIX_FS 0
#define ESPK_LOG(...)
#define HAVE_UNISTD_H 1
#define le16toh(x) (x)
#define le32toh(x) (x)
#ifdef _MSC_VER
#include <io.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strdup _strdup
#define close _close
#define S_ISDIR(mode) (((mode) & 0170000) == 0040000)
#define strerror_r(number,buffer,size) strerror_s(buffer,size,number)
#else
#define HAVE_MKSTEMP 1
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
void *espeak_mem_map(const char *, int *);
#ifdef __cplusplus
}
#endif
