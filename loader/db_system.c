#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "db_system.h"

#if defined(WIN32)
#include <windows.h>
#include <psapi.h>
#endif

typedef struct PathEntry PathEntry;
struct PathEntry {
    PathEntry *next;
    char path[1];
};

static PathEntry *path = NULL;
static PathEntry **pNextPathEntry = &path;

static const char *MakePath(PathEntry *entry, const char *name);

void xbInfo(System *sys, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xbInfoV(sys, fmt, ap);
    va_end(ap);
}

void xbError(System *sys, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xbErrorV(sys, fmt, ap);
    va_end(ap);
}

void *xbOpenFileInPath(System *sys, const char *name, const char *mode)
{
    PathEntry *entry;
    void *file;
    
#if 0
    xbInfo("path:");
    for (entry = path; entry != NULL; entry = entry->next)
        xbInfo(" '%s'", entry->path);
    xbInfo("\n");
#endif
    
    if (!(file = xbOpenFile(sys, name, mode))) {
        for (entry = path; entry != NULL; entry = entry->next)
            if ((file = xbOpenFile(sys, MakePath(entry, name), mode)) != NULL)
                break;
    }
    return file;
}

int xbAddToPath(const char *p)
{
    PathEntry *entry = malloc(sizeof(PathEntry) + strlen(p));
    if (!(entry))
        return FALSE;
    strcpy(entry->path, p);
    *pNextPathEntry = entry;
    pNextPathEntry = &entry->next;
    entry->next = NULL;
    return TRUE;
}

#if defined(WIN32)

/* GetProgramPath - get the path relative the application directory */
static char *GetProgramPath(void)
{
    static char fullpath[1024];
    char *p;

#if defined(Q_OS_WIN32)
    /* get the full path to the executable */
    if (!GetModuleFileNameA(NULL, fullpath, sizeof(fullpath)))
        return NULL;
#else
    /* get the full path to the executable */
    if (!GetModuleFileNameEx(GetCurrentProcess(), NULL, fullpath, sizeof(fullpath)))
        return NULL;
#endif

    /* remove the executable filename */
    if ((p = strrchr(fullpath, '\\')) != NULL)
        *p = '\0';

    /* remove the immediate directory containing the executable (usually 'bin') */
    if ((p = strrchr(fullpath, '\\')) != NULL) {
        *p = '\0';
        
        /* check for the 'Release' or 'Debug' build directories used by Visual C++ */
        if (strcmp(&p[1], "Release") == 0 || strcmp(&p[1], "Debug") == 0) {
            if ((p = strrchr(fullpath, '\\')) != NULL)
                *p = '\0';
        }
    }

    /* generate the full path to the 'include' directory */
    strcat(fullpath, "\\include\\");
    return fullpath;
}

#endif

int xbAddEnvironmentPath(void)
{
    char *p, *end;
    
    /* add path entries from the environment */
    if ((p = getenv("XB_INC")) != NULL) {
        while ((end = strchr(p, PATH_SEP)) != NULL) {
            *end = '\0';
            if (!xbAddToPath(p))
                return FALSE;
            p = end + 1;
        }
        if (!xbAddToPath(p))
            return FALSE;
    }
    
    /* add the path relative to the location of the executable */
#if defined(WIN32)
    if (!(p = GetProgramPath()))
        if (!xbAddToPath(p))
            return FALSE;
#endif

    return TRUE;
}

static const char *MakePath(PathEntry *entry, const char *name)
{
    static char fullpath[PATH_MAX];
    sprintf(fullpath, "%s%c%s", entry->path, DIR_SEP, name);
	return fullpath;
}

/* functions below depend on stdio */

void *xbCreateTmpFile(System *sys, const char *name, const char *mode)
{
    return fopen(name, mode);
}

int xbRemoveTmpFile(System *sys, const char *name)
{
    return remove(name) == 0;
}

void *xbOpenFile(System *sys, const char *name, const char *mode)
{
    return (void *)fopen(name, mode);
}

int xbCloseFile(void *file)
{
    return fclose((FILE *)file) == 0;
}

char *xbGetLine(void *file, char *buf, size_t size)
{
    return fgets(buf, size, (FILE *)file);
}

size_t xbReadFile(void *file, void *buf, size_t size)
{
    return fread(buf, 1, size, (FILE *)file);
}

size_t xbWriteFile(void *file, const void *buf, size_t size)
{
    return fwrite(buf, 1, size, (FILE *)file);
}

int xbSeekFile(void *file, long offset, int whence)
{
    return fseek((FILE *)file, offset, whence);
}

#if defined(NEED_STRCASECMP)

int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 != '\0' && (tolower(*s1) == tolower(*s2))) {
        ++s1;
        ++s2;
    }
    return tolower((unsigned char) *s1) - tolower((unsigned char) *s2);
}

#endif
