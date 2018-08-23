#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "db_config.h"
#include "db_image.h"
#include "db_system.h"

#define MAXLINE 128

typedef struct {
    char lineBuf[MAXLINE];  /* line buffer */
    char *linePtr;          /* pointer to the current character */
    int lineNumber;         /* current line number */
} LineBuf;

typedef struct {
    char *name;
    int value;
} ConfigSymbol;

static ConfigSymbol configSymbols[] = {
{   "RCFAST",       0x00        },
{   "RCSLOW",       0x01        },
{   "XINPUT",       0x20        },
{   "XTAL1",        0x28        },
{   "XTAL2",        0x30        },
{   "XTAL3",        0x38        },
{   "PLL1X",        0x43        },
{   "PLL2X",        0x44        },
{   "PLL4X",        0x45        },
{   "PLL8X",        0x46        },
{   "PLL16X",       0x47        },
{   "K",            1024        },
{   "M",            1024*1024   },
{   NULL,           0           }
};

/* board configurations */
static BoardConfig *boardConfigs = NULL;

static int SkipSpaces(LineBuf *buf);
static char *NextToken(LineBuf *buf, const char *termSet, int *pTerm);
static int ParseNumericExpr(LineBuf *buf, char *token, int *pValue);
static int DoOp(LineBuf *buf, int op, int left, int right);
static char *CopyString(LineBuf *buf, const char *str);
static void Error(LineBuf *buf, const char *fmt, ...);

static BoardConfig *NewBoardConfig(const char *name)
{
    BoardConfig *config;
    if (!(config = (BoardConfig *)malloc(sizeof(BoardConfig) + strlen(name))))
        return NULL;
    memset(config, 0, sizeof(BoardConfig));
    strcpy(config->name, name);
    return config;
}

BoardConfig *GetBoardConfig(const char *name)
{
    BoardConfig *config;
    for (config = boardConfigs; config != NULL; config = config->next)
        if (strcasecmp(name, config->name) == 0)
            return config;
    return NULL;
}

static Section *NewSection(const char *name)
{
    Section *section;
    if (!(section = (Section *)malloc(sizeof(Section) + strlen(name))))
        return NULL;
    memset(section, 0, sizeof(Section));
    strcpy(section->name, name);
    return section;
}

Section *GetSection(BoardConfig *config, const char *name)
{
    Section *section;
    for (section = config->sections; section != NULL; section = section->next)
        if (strcasecmp(name, section->name) == 0)
            return section;
    return NULL;
}

/* ParseConfigurationFile - parse a configuration file */
void ParseConfigurationFile(System *sys, const char *path)
{
    char text[MAXLINE], data[MAXLINE], *tag, *value;
    BoardConfig **pNextConfig = &boardConfigs;
    BoardConfig *config = NULL;
    Section **pNextSection;
    Section *section;
    LineBuf buf;
    int iValue;
    FILE *fp;
    int ch;

    if (!(fp = xbOpenFileInPath(sys, path, "r")))
        return;

    strcpy(text, "hub");
    strcpy(data, "hub");

    while (fgets(buf.lineBuf, sizeof(buf.lineBuf), fp)) {
        buf.linePtr = buf.lineBuf;
        ++buf.lineNumber;
        switch (SkipSpaces(&buf)) {
        case '\n':  /* blank line */
        case '#':   /* comment */
            // ignore blank lines and comments
            break;
        case '[':   /* board tag */
            ++buf.linePtr;
            if (!(tag = NextToken(&buf, "]", &ch)))
                Error(&buf, "missing board tag");
            if (ch != ']') {
                if (SkipSpaces(&buf) != ']')
                    Error(&buf, "missing close bracket after section tag");
                ++buf.linePtr;
            }
            if (SkipSpaces(&buf) != '\n')
                Error(&buf, "missing end of line");
            if (config) {
                if (!(section = NewSection("hub")))
                    Error(&buf, "insufficient memory");
                section->base = HUB_BASE;
                section->size = HUB_SIZE;
                *pNextSection = section;
                ++config->sectionCount;
                config = NULL;
            }
            if (!(config = NewBoardConfig(tag)))
                Error(&buf, "insufficient memory");
            *pNextConfig = config;
            pNextConfig = &config->next;
            pNextSection = &config->sections;
            break;
        default:    /* tag:value pair */
            if (!config)
                Error(&buf, "not in a board definition");
            if (!(tag = NextToken(&buf, ":", &ch)))
                Error(&buf, "missing tag");
            if (ch != ':') {
                if (SkipSpaces(&buf) != ':')
                    Error(&buf, "missing colon");
                ++buf.linePtr;
            }
            if (!(value = NextToken(&buf, "", &ch)))
                Error(&buf, "missing value");
            if (ch != '\n') {
                if (SkipSpaces(&buf) != '\n')
                    Error(&buf, "missing end of line");
                ++buf.linePtr;
            }
            if (strcasecmp(tag, "clkfreq") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                config->clkfreq = iValue;
            }
            else if (strcasecmp(tag, "clkmode") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                config->clkmode = iValue;
            }
            else if (strcasecmp(tag, "baudrate") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                config->baudrate = iValue;
            }
            else if (strcasecmp(tag, "rxpin") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                config->rxpin = iValue;
            }
            else if (strcasecmp(tag, "txpin") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                config->txpin = iValue;
            }
            else if (strcasecmp(tag, "tvpin") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                config->tvpin = iValue;
            }
            else if (strcasecmp(tag, "cache-driver") == 0) {
                if (config->cacheDriver)
                    free(config->cacheDriver);
                if (!(config->cacheDriver = CopyString(&buf, value)))
                    Error(&buf, "insufficient memory");
            }
            else if (strcasecmp(tag, "cache-size") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                config->cacheSize = iValue;
            }
            else if (strcasecmp(tag, "cache-param1") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                config->cacheParam1 = iValue;
            }
            else if (strcasecmp(tag, "cache-param2") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                config->cacheParam2 = iValue;
            }
            else if (strcasecmp(tag, "text") == 0) {
                if (config->defaultTextSection)
                    free(config->defaultTextSection);
                if (!(config->defaultTextSection = CopyString(&buf, value)))
                    Error(&buf, "insufficient memory");
            }
            else if (strcasecmp(tag, "data") == 0) {
                if (config->defaultDataSection)
                    free(config->defaultDataSection);
                if (!(config->defaultDataSection = CopyString(&buf, value)))
                    Error(&buf, "insufficient memory");
            }
            else if (strcasecmp(tag, "flash-size") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                if (!(section = NewSection("flash")))
                    Error(&buf, "insufficient memory");
                section->base = FLASH_BASE;
                section->size = iValue;
                *pNextSection = section;
                pNextSection = &section->next;
                ++config->sectionCount;
            }
            else if (strcasecmp(tag, "ram-size") == 0) {
                if (!ParseNumericExpr(&buf, value, &iValue))
                    Error(&buf, "invalid numeric value");
                if (!(section = NewSection("ram")))
                    Error(&buf, "insufficient memory");
                section->base = RAM_BASE;
                section->size = iValue;
                *pNextSection = section;
                pNextSection = &section->next;
                ++config->sectionCount;
            }
            else
                Error(&buf, "unknown tag: %s", tag);
            break;
        }
    }

    if (config) {
        if (!(section = NewSection("hub")))
            Error(&buf, "insufficient memory");
        section->base = HUB_BASE;
        section->size = HUB_SIZE;
        *pNextSection = section;
        ++config->sectionCount;
    }

    fclose(fp);
}

static int SkipSpaces(LineBuf *buf)
{
    int ch;
    while ((ch = *buf->linePtr) != '\0' && ch != '#' && ch != '\n' && isspace(ch))
         ++buf->linePtr;
    if (ch == '#') {
        while ((ch = *++buf->linePtr) != '\0' && ch != '\n')
            ;
    }
    return *buf->linePtr;
}

static char *NextToken(LineBuf *buf, const char *termSet, int *pTerm)
{
    char *token;
    int ch;

    /* skip leading spaces */
    if (!SkipSpaces(buf))
        return NULL;

    /* collect the token */
    token = buf->linePtr;
    while ((ch = *buf->linePtr) != '\0' && ch != '\n' && !isspace(ch) && !strchr(termSet, ch))
        ++buf->linePtr;

    /* return the terminator character */
    *pTerm = ch;

    /* terminate the token */
    if (*buf->linePtr != '\0')
        *buf->linePtr++ = '\0';

    /* return the token or NULL if at the end of the line */
    return *token == '\0' ? NULL : token;
}

static int SkipSpacesStr(char **pp)
{
    char *p = *pp;
    while (*p != '\0' && isspace(*p))
        ++p;
    *pp = p;
    return *p;
}

static int ParseNumericExpr(LineBuf *buf, char *token, int *pValue)
{
    char *p = token;
    int value = 0;
    int op = -1;
    while (SkipSpacesStr(&p)) {
        if (isdigit(*p)) {
            value = strtol(p, &p, 0);
            switch (*p) {
            case 'k':
            case 'K':
                value *= 1024;
                ++p;
                break;
            case 'm':
            case 'M':
                value *= 1024 * 1024;
                ++p;
                break;
            default:
                // nothing to do
                break;
            }
            *pValue = DoOp(buf, op, *pValue, value);
            op = -1;
        }
        else if (isalpha(*p)) {
            char id[32], *p2 = id;
            ConfigSymbol *sym;
            while (*p != '\0' && isalnum(*p)) {
                if (p2 < id + sizeof(id) - 1)
                    *p2++ = *p;
                ++p;
            }
            *p2 = '\0';
            for (sym = configSymbols; sym->name != NULL; ++sym)
                if (strcasecmp(id, sym->name) == 0) {
                    *pValue = DoOp(buf, op, *pValue, sym->value);
                    op = -1;
                    break;
                }
            if (!sym)
                Error(buf, "undefined symbol: %s\n", id);
        }
        else {
            switch (*p) {
            case '+':
            case '-':
            case '*':
            case '/':
            case '%':
            case '&':
            case '|':
                op = *p;
                break;
            default:
                Error(buf, "unknown operator: %c", *p);
                break;
            }
            ++p;
        }
    }
    return *p == '\0';
}

static int DoOp(LineBuf *buf, int op, int left, int right)
{
    switch (op) {
    case -1:
        left = right;
        break;
    case '+':
        left += right;
        break;
    case '-':
        left -= right;
        break;
    case '*':
        left *= right;
        break;
    case '/':
        left /= right;
        break;
    case '%':
        left %= right;
        break;
    case '&':
        left &= right;
        break;
    case '|':
        left |= right;
        break;
    default:
        Error(buf, "unknown operator: %c", op);
        break;
    }
    return left;
}

static char *CopyString(LineBuf *buf, const char *str)
{
	char *copy = (char *)malloc(strlen(str) + 1);
	if (!copy)
		Error(buf, "insufficient memory");
	strcpy(copy, str);
	return copy;
}

static void Error(LineBuf *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, ap);
    putc('\n', stderr);
    fprintf(stderr, "  on line number %d\n", buf->lineNumber);
    va_end(ap);
    exit(1);
}

