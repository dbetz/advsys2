#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "db_loader.h"
#include "db_packet.h"
#include "PLoadLib.h"
#include "osint.h"

/* default baud rate */
#define BAUD_RATE               115200

/* maximum cog image size */
#define COG_IMAGE_MAX           (496 * 4)

/* spin object file header */
typedef struct {
    uint32_t clkfreq;
    uint8_t clkmode;
    uint8_t chksum;
    uint16_t objstart;
    uint16_t varstart;
    uint16_t stkstart;
    uint16_t pubstart;
    uint16_t stkbase;
} SpinHdr;

/* spin object */
typedef struct {
    uint16_t next;
    uint8_t pubcnt;
    uint8_t objcnt;
    uint16_t pubstart;
    uint16_t numlocals;
} SpinObj;

/* DAT header in serial_helper.spin */
typedef struct {
    uint32_t baudrate;
    uint8_t rxpin;
    uint8_t txpin;
    uint8_t tvpin;
} SerialHelperDatHdr;

/* DAT header in hub_loader.spin */
typedef struct {
    uint32_t image_size;
    uint32_t baudrate;
    uint8_t rxpin;
    uint8_t txpin;
    uint16_t unused;
    uint32_t vm_code_off;
    uint32_t image_off;
    uint32_t max_image_size;
} HubLoaderDatHdr;

/* DAT header in flash_loader.spin */
typedef struct {
    uint32_t cache_size;
    uint32_t baudrate;
    uint8_t rxpin;
    uint8_t txpin;
    uint16_t unused;
    uint32_t vm_code_off;
    uint32_t cache_code_off;
} FlashLoaderDatHdr;

/* target checksum for a binary file */
#define SPIN_TARGET_CHECKSUM    0x14

/* packet types */
#define TYPE_VM_INIT            1
#define TYPE_CACHE_INIT         2
#define TYPE_FLASH_WRITE        3
#define TYPE_RAM_WRITE          4
#define TYPE_HUB_WRITE          5
#define TYPE_DATA               6
#define TYPE_EOF                7
#define TYPE_RUN                8

extern uint8_t serial_helper_array[];
extern int serial_helper_size;
extern uint8_t hub_loader_array[];
extern int hub_loader_size;
extern uint8_t flash_loader_array[];
extern int flash_loader_size;
extern uint8_t advsys2_vm_array[];
extern int advsys2_vm_size;

static int ReadCogImage(System *sys, char *name, uint8_t *buf, int *pSize);
static FILE *OpenAndProbeFile(char *path, char *buf, int *pSize, int *pCnt, int *pType);
static int WriteFileToMemory(char *path);
static int WriteBuffer(uint8_t *buf, int size);
static int WriteFile(FILE *fp, uint8_t *buf, int cnt, int size);
static char *PacketTypeName(int type);
static int Error(char *fmt, ...);

int InitPort(char *port)
{
	return serial_init(port, BAUD_RATE);
}

int LoadImage(System *sys, BoardConfig *config, char *port, char *path)
{    
	SpinHdr *hdr = (SpinHdr *)serial_helper_array;
    SpinObj *obj = (SpinObj *)(serial_helper_array + hdr->objstart);
    SerialHelperDatHdr *dat = (SerialHelperDatHdr *)((uint8_t *)obj + (obj->pubcnt + obj->objcnt) * sizeof(uint32_t));
    uint8_t cacheDriverImage[COG_IMAGE_MAX];
    int imageSize, chksum, i;
	
    /* patch serial helper for clock mode and frequency */
    hdr->clkfreq = config->clkfreq;
    hdr->clkmode = config->clkmode;
	dat->baudrate = config->baudrate;
	dat->rxpin = config->rxpin;
	dat->txpin = config->txpin;
	dat->tvpin = config->tvpin;
        
    /* recompute the checksum */
    hdr->chksum = 0;
    for (chksum = i = 0; i < serial_helper_size; ++i)
        chksum += serial_helper_array[i];
    hdr->chksum = SPIN_TARGET_CHECKSUM - chksum;
    
	/* load the serial helper program */
    if (ploadbuf(serial_helper_array, serial_helper_size, port, DOWNLOAD_RUN_BINARY) != 0)
		return Error("helper load failed");

	/* wait for the serial helper to complete initialization */
    if (!WaitForInitialAck())
		return Error("failed to connect to helper");
    
    /* load the cache driver */
    if (config->cacheDriver) {
        VMUVALUE params[3];
        if (!ReadCogImage(sys, config->cacheDriver, cacheDriverImage, &imageSize))
            return Error("reading cache driver image failed: %s", config->cacheDriver);
		printf("Loading cache driver\n");
        params[0] = config->cacheSize;
        params[1] = config->cacheParam1;
        params[2] = config->cacheParam2;
        if (!SendPacket(TYPE_HUB_WRITE, (uint8_t *)"", 0)
        ||  !WriteBuffer(cacheDriverImage, imageSize)
        ||  !SendPacket(TYPE_CACHE_INIT, (uint8_t *)params, sizeof(params)))
            return Error("Loading cache driver failed");
    }
    
    /* load the vm */
	printf("Loading VM\n");
    if (!SendPacket(TYPE_HUB_WRITE, (uint8_t *)"", 0)
    ||  !WriteBuffer(advsys2_vm_array, advsys2_vm_size)
    ||  !SendPacket(TYPE_VM_INIT, (uint8_t *)"", 0))
        return Error("Loading VM failed");
    
    /* write the image to memory */
	printf("Loading image\n");
	if (!WriteFileToMemory(path)) {
		fprintf(stderr, "error: image load failed\n");
		return FALSE;
	}
	
	/* return successfully */
	return TRUE;
}

#if 0
int WriteHubLoaderToEEPROM(System *sys, BoardConfig *config, char *port, char *path)
{
	SpinHdr *hdr = (SpinHdr *)hub_loader_array;
    SpinObj *obj = (SpinObj *)(hub_loader_array + hdr->objstart);
    HubLoaderDatHdr *dat = (HubLoaderDatHdr *)((uint8_t *)obj + (obj->pubcnt + obj->objcnt) * sizeof(uint32_t));
    int chksum, i;
    uint32_t size;
    FILE *fp;
	
    /* patch serial helper for clock mode and frequency */
    hdr->clkfreq = config->clkfreq;
    hdr->clkmode = config->clkmode;
	dat->baudrate = config->baudrate;
	dat->rxpin = config->rxpin;
	dat->txpin = config->txpin;
    
    /* copy the vm image to the binary file */
    memcpy((uint8_t *)dat + dat->vm_code_off, advsys2_vm_array, advsys2_vm_size);
    
    /* open the image file */
    if ((fp = fopen(path, "rb")) == NULL)
        return Error("can't open image file");
        
    /* get the file size */
    fseek(fp, 0, SEEK_END);
    size = (uint32_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* make sure the image will fit */
    if (size > dat->max_image_size)
        return Error("image too large");
        
    /* read the image into the binary file */
    if (fread((uint8_t *)dat + dat->image_off, 1, size, fp) != size)
        return Error("can't read image file");
        
    /* close the input file */
    fclose(fp);

    /* recompute the checksum */
    hdr->chksum = 0;
    for (chksum = i = 0; i < hub_loader_size; ++i)
        chksum += hub_loader_array[i];
    hdr->chksum = SPIN_TARGET_CHECKSUM - chksum;
    
	/* load the loader program to eeprom */
    if (ploadbuf(hub_loader_array, hub_loader_size, port, DOWNLOAD_EEPROM) != 0)
		return Error("loader load failed");
	
	/* return successfully */
	return TRUE;
}

int WriteFlashLoaderToEEPROM(System *sys, BoardConfig *config, char *port)
{
	SpinHdr *hdr = (SpinHdr *)flash_loader_array;
    SpinObj *obj = (SpinObj *)(flash_loader_array + hdr->objstart);
    FlashLoaderDatHdr *dat = (FlashLoaderDatHdr *)((uint8_t *)obj + (obj->pubcnt + obj->objcnt) * sizeof(uint32_t));
    uint8_t cacheDriverImage[COG_IMAGE_MAX];
    int imageSize, chksum, i;
	
    if (!ReadCogImage(sys, config->cacheDriver, cacheDriverImage, &imageSize))
        return Error("reading cache driver image failed: %s", config->cacheDriver);
        
    /* patch serial helper for clock mode and frequency */
    hdr->clkfreq = config->clkfreq;
    hdr->clkmode = config->clkmode;
	dat->baudrate = config->baudrate;
	dat->rxpin = config->rxpin;
	dat->txpin = config->txpin;
    
    /* copy the vm image to the binary file */
    memcpy((uint8_t *)dat + dat->vm_code_off, advsys2_vm_array, advsys2_vm_size);
    
    /* copy the cache driver image to the binary file */
    memcpy((uint8_t *)dat + dat->cache_code_off, cacheDriverImage, imageSize);
    
    /* get the cache size */
    dat->cache_size = config->cacheSize;
    
    /* recompute the checksum */
    hdr->chksum = 0;
    for (chksum = i = 0; i < flash_loader_size; ++i)
        chksum += flash_loader_array[i];
    hdr->chksum = SPIN_TARGET_CHECKSUM - chksum;
    
	/* load the loader program to eeprom */
    if (ploadbuf(flash_loader_array, flash_loader_size, port, DOWNLOAD_EEPROM) != 0)
		return Error("loader load failed");
	
	/* return successfully */
	return TRUE;
}
#endif

int RunLoadedProgram(int flags)
{
    VMUVALUE arg = (VMUVALUE)flags;
	return SendPacket(TYPE_RUN, (uint8_t *)&arg, sizeof(VMUVALUE));
}

static int ReadCogImage(System *sys, char *name, uint8_t *buf, int *pSize)
{
    void *file;
    if (!(file = xbOpenFileInPath(sys, name, "rb")))
        return Error("can't open cache driver: %s", name);
    *pSize = xbReadFile(file, buf, COG_IMAGE_MAX);
    xbCloseFile(file);
    return *pSize > 0;
}

static FILE *OpenAndProbeFile(char *path, char *buf, int *pSize, int *pCnt, int *pType)
{
    ImageFileHdr *hdr = (ImageFileHdr *)buf;
    FILE *fp;

    if ((fp = fopen(path, "rb")) == NULL)
        return NULL;

    fseek(fp, 0, SEEK_END);
    *pSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if ((*pCnt = fread(buf, 1, PKTMAXLEN, fp)) < sizeof(ImageFileHdr)) {
        Error("bad file header: %s", path);
        return NULL;
    }
    
    if (memcmp(hdr->tag, IMAGE_TAG, sizeof(hdr->tag)) != 0) {
        Error("invalid image file");
        return NULL;
    }
    else if (hdr->version != IMAGE_VERSION) {
        Error("wrong image file version: expected %04x, found %04x\n", IMAGE_VERSION, hdr->version);
        return NULL;
    }

    switch (hdr->sections[0].base) {
    case FLASH_BASE:
        *pType = TYPE_FLASH_WRITE;
        break;
    case RAM_BASE:
        *pType = TYPE_RAM_WRITE;
        break;
    case HUB_BASE:
        *pType = TYPE_HUB_WRITE;
        break;
    default:
        Error("unknown image type");
        return NULL;
    }

    return fp;
}

static int WriteFileToMemory(char *path)
{
    uint8_t buf[PKTMAXLEN];
    int size, type, cnt;
    FILE *fp;

    if ((fp = OpenAndProbeFile(path, (char *)buf, &size, &cnt, &type)) == NULL)
        return FALSE;

    if (!SendPacket(type, (uint8_t *)"", 0))
        return Error("SendPacket %s failed", PacketTypeName(type));

    return WriteFile(fp, buf, cnt, size);
}

static int WriteFile(FILE *fp, uint8_t *buf, int cnt, int size)
{
    int remaining = size;

    while (cnt > 0) {
        printf("%d bytes remaining             \r", remaining); fflush(stdout);
        if (!SendPacket(TYPE_DATA, buf, cnt))
            return Error("SendPacket DATA failed\n");
        remaining -= cnt;
        cnt = fread(buf, 1, PKTMAXLEN, fp);
    }
    printf("%d bytes sent             \n", size);

    fclose(fp);

    if (!SendPacket(TYPE_EOF, (uint8_t *)"", 0))
        return Error("SendPacket EOF failed");

    return TRUE;
}

static int WriteBuffer(uint8_t *buf, int size)
{
    int remaining, cnt;

    for (remaining = size; remaining > 0; remaining -= cnt, buf += cnt) {
        if ((cnt = remaining) > PKTMAXLEN)
            cnt = PKTMAXLEN;
        printf("%d bytes remaining             \r", remaining); fflush(stdout);
        if (!SendPacket(TYPE_DATA, buf, cnt))
            return Error("SendPacket DATA failed\n");
    }
    printf("%d bytes sent             \n", size);

    if (!SendPacket(TYPE_EOF, (uint8_t *)"", 0))
        return Error("SendPacket EOF failed");

    return TRUE;
}

static char *PacketTypeName(int type)
{
    char *typeName = "<unknown>";
    switch (type) {
    case TYPE_VM_INIT:		typeName = "VM_INIT";		break;
    case TYPE_CACHE_INIT:	typeName = "CACHE_INIT";	break;
    case TYPE_FLASH_WRITE:  typeName = "FLASH_WRITE";   break;
    case TYPE_RAM_WRITE:    typeName = "RAM_WRITE";     break;
    case TYPE_HUB_WRITE:    typeName = "HUB_WRITE";     break;
    case TYPE_DATA:         typeName = "DATA";          break;
    case TYPE_EOF:          typeName = "EOF";           break;
    case TYPE_RUN:          typeName = "RUN";           break;
    }
    return typeName;
}

static int Error(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("error: ");
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);
    return FALSE;
}
