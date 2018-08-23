#ifndef __DB_SYSTEM_H__
#define __DB_SYSTEM_H__

#include <stdarg.h>

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

#if defined(WIN32)
#define PATH_SEP    ';'
#define DIR_SEP     '\\'
#else
#define PATH_SEP    ':'
#define DIR_SEP     '/'
#endif

/* forward typedefs */
typedef struct System System;

/* system operations table */
typedef struct {
    void (*info)(System *sys, const char *fmt, va_list ap);
    void (*error)(System *sys, const char *fmt, va_list ap);
} SystemOps;

/* system interface */
struct System {
    SystemOps   *ops;
};

#define xbInfoV(sys, fmt, args)     ((*(sys)->ops->info)((sys), (fmt), (args)))
#define xbErrorV(sys, fmt, args)    ((*(sys)->ops->error)((sys), (fmt), (args)))

void xbInfo(System *sys, const char *fmt, ...);
void xbError(System *sys, const char *fmt, ...);
int xbAddToPath(const char *p);
int xbAddEnvironmentPath(void);
void *xbOpenFileInPath(System *sys, const char *name, const char *mode);
void *xbOpenFile(System *sys, const char *name, const char *mode);
int xbCloseFile(void *file);
char *xbGetLine(void *file, char *buf, size_t size);
size_t xbReadFile(void *file, void *buf, size_t size);
size_t xbWriteFile(void *file, const void *buf, size_t size);
int xbSeekFile(void *file, long offset, int whence);
void *xbCreateTmpFile(System *sys, const char *name, const char *mode);
int xbRemoveTmpFile(System *sys, const char *name);
void *xbGlobalAlloc(System *sys, size_t size);
void *xbLocalAlloc(System *sys, size_t size);
void xbLocalFreeAll(System *sys);

#endif
