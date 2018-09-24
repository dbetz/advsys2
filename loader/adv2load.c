#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "db_loader.h"
#include "db_packet.h"

/* defaults */
#if defined(CYGWIN) || defined(WIN32)
#define DEF_PORT    "COM1"
#endif
#ifdef LINUX
#define DEF_PORT    "/dev/ttyUSB0"
#endif
#ifdef MACOSX
#define DEF_PORT    "/dev/tty.usbserial-A30005Dm"
#endif
#define DEF_BOARD   "c3"

static void Usage(void);
static void ConstructFileName(const char *infile, char *outfile, char *ext);

static void MyInfo(System *sys, const char *fmt, va_list ap);
static void MyError(System *sys, const char *fmt, va_list ap);
static SystemOps myOps = {
    MyInfo,
    MyError
};

int main(int argc, char *argv[])
{
    char *infile = NULL, fullName[FILENAME_MAX];
    int runFlags = 0;
    int terminalMode = FALSE;
    BoardConfig *config;
    char *port, *board;
    System sys;
    int i;

    /* get the environment settings */
    if (!(port = getenv("PORT")))
        port = DEF_PORT;
    if (!(board = getenv("BOARD")))
        board = DEF_BOARD;

    /* get the arguments */
    for(i = 1; i < argc; ++i) {

        /* handle switches */
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
            case 'b':   // select a target board
                if (argv[i][2])
                    board = &argv[i][2];
                else if (++i < argc)
                    board = argv[i];
                else
                    Usage();
                break;
            case 'd':
                runFlags |= RUN_PAUSE;
                break;
            case 'p':
                if(argv[i][2])
                    port = &argv[i][2];
                else if(++i < argc)
                    port = argv[i];
                else
                    Usage();
                if (isdigit((int)port[0])) {
#if defined(CYGWIN) || defined(WIN32)
                    static char buf[10];
                    sprintf(buf, "COM%d", atoi(port));
                    port = buf;
#endif
#ifdef LINUX
                    static char buf[10];
                    sprintf(buf, "/dev/ttyUSB%d", atoi(port));
                    port = buf;
#endif
                }
                break;
            case 's':
                runFlags |= RUN_STEP;
                break;
            case 't':
                terminalMode = TRUE;
                break;
            default:
                Usage();
                break;
            }
        }

        /* handle the input filename */
        else {
            if (infile)
                Usage();
            infile = argv[i];
        }
    }
    
    sys.ops = &myOps;
    ParseConfigurationFile(&sys, "advsys2.cfg");

    /* make sure an input file was specified */
    if (!infile)
        Usage();
    ConstructFileName(infile, fullName, ".bai");

    /* setup for the selected board */
    if (!(config = GetBoardConfig(board)))
        Usage();

    /* initialize the serial port */
    if (!InitPort(port)) {
        fprintf(stderr, "error: opening serial port\n");
        return 1;
    }

    /* load the compiled image */
    if (!LoadImage(&sys, config, port, fullName)) {
        fprintf(stderr, "error: load failed\n");
        return 1;
    }
    
    /* run the loaded image */
    if (!RunLoadedProgram(runFlags)) {
        fprintf(stderr, "error: run failed\n");
        return 1;
    }

    /* enter terminal mode if requested */
    if (terminalMode)
        TerminalMode();
    
    return 0;
}

/* Usage - display a usage message and exit */
static void Usage(void)
{
    fprintf(stderr, "\
usage: xload\n\
         [ -b <type> ]   select target board (c3 | ssf | hub | hub96) (default is hub)\n\
         [ -p <port> ]   serial port (default is %s)\n\
         [ -s ]          single step program\n\
         [ -t ]          enter terminal mode after running the program\n\
         <name>          file to compile\n\
", DEF_PORT);
    exit(1);
}

/* ConstructFileName - construct a filename with an optional extension */
static void ConstructFileName(const char *infile, char *outfile, char *ext)
{
    char *end = strrchr(infile, '.');
    strcpy(outfile, infile);
    if (!end || strchr(end, '/') || strchr(end, '\\'))
        strcat(outfile, ext);
}

static void MyInfo(System *sys, const char *fmt, va_list ap)
{
    vfprintf(stdout, fmt, ap);
}

static void MyError(System *sys, const char *fmt, va_list ap)
{
    vfprintf(stderr, fmt, ap);
}
