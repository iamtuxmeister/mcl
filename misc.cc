#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "mcl.h"
#include "cui.h"
#include <time.h>

void error(const char *fmt, ...) {
    va_list va;

    freopen("/dev/tty", "r+", stderr);
    va_start(va,fmt);
    fputs(CLEAR_SCREEN,stderr);
	vfprintf(stderr, fmt,va);
	va_end(va);
	
    fprintf(stderr, "\n");
	
	exit (EXIT_FAILURE);
}

void report(const char *fmt, ...) {
    va_list va;
    char buf[4096];

	va_start(va,fmt);	
	vsnprintf(buf, sizeof(buf)-1, fmt,va);
    va_end(va);

    strcat(buf, "\n");

    if (output)
        output->printf("%s", buf);
    else
        fprintf(stderr, "%s", buf);
            
}

// #define MEMORY_DEBUG

#ifdef MEMORY_DEBUG

#define MAGIC_MARKER (int) 0xEAEAEAEA

#define FRONT(ptr) *(int*)ptr
#define SIZE(ptr) ((int*)ptr)[1]
#define BACK(ptr, size) *(int*)(ptr + size + 2 * sizeof(int))

void * operator new (size_t size)
{
    char *ptr = (char*) malloc(size+3*sizeof(int));
    FRONT(ptr) = MAGIC_MARKER;
    SIZE(ptr) = size;

    BACK(ptr,size) = MAGIC_MARKER;

//    fprintf(stderr, "Allocated   %6u bytes at %08x\n", size, (int)ptr);

    return ptr+(2*sizeof(int));
}

void operator delete (void *ptr)
{
    int size;
    char *p = ((char*)ptr)- (2 * sizeof(int));

    if (FRONT(p) != MAGIC_MARKER)
        abort();

    size = SIZE(p);

    if (BACK(p, size) != MAGIC_MARKER)
        abort();

    FRONT(p) = 0;
    

//    fprintf(stderr, "Deallocated %6d bytes at %08x\n", size,(int)p);
}

#endif

const char * versionToString(int version)
{
    static char buf[64];
    sprintf(buf, "%d.%02d.%02d",
            version/10000, (version - ((10000 * (version/10000)))) / 100, version % 100);

    return buf;
}

int countChar(const char *s, int c)
{
    int count = 0;
    while (*s)
        if (*s++ == c)
            count++;

    return count;
}

int longestLine (const char *s)
{
    char buf[MAX_MUD_BUF];
    int max_len = 0;
    strcpy(buf, s);

    s = strtok(buf, "\n");
    while (s)
    {
        max_len = max(strlen(s), max_len);
        s = strtok(NULL, "\n");
    }

    return max_len;
}


GlobalStats::GlobalStats() {
    time (&starting_time);
}

static int color_conv_table[8] =  {
    fg_black,
    fg_red,
    fg_green,
    fg_yellow,
    fg_blue,
    fg_magenta,
    fg_cyan,
    fg_white
};

ColorConverter::ColorConverter() : fBold(false), fReport(false), last_fg(fg_white), last_bg(bg_black) {
}

#define MAX_COLOR_BUF 256
int ColorConverter::convert (const unsigned char *s, int size) {
	char    buf[MAX_COLOR_BUF];
	int     code;
	int     color;
	char   *pc;

	if (size < 0 || size >= MAX_COLOR_BUF-1)
		return 0;

	memcpy (buf, s, size);
	buf[size] = NUL;

	if (buf[0] != '\e' || buf[1] != '[')
		return 0;

	pc = buf + 2;

	for (;;) 	{
		code = 0;

		while (isdigit (*pc))
			code = code * 10 + *pc++ - '0';

		switch (code)  {
        case 0:			/* Default */
            fBold = false;
            last_fg = fg_white;
            last_bg = bg_black;
            break;

        case 1:			/* bold ON */
            fBold = true;
            break;

        case 6: 
            if (pc[1] == 'n')
                fReport = true;
            break;

        case 7:			/* reverse */
            /* Ignore for now. It's usually ugly anyway */
            break;

        case 30 ... 37:
        case 40 ... 47:
            if (code <= 37)
                last_fg = (color_conv_table[code - 30]);
            else
                last_bg = (color_conv_table[code - 40]) << 4;

        default:
            ;
        }

		/* Allow for multiple codes separeated with ;s */
		if (*pc != ';')
			break;
		
		pc++;
	}

	color = last_fg | last_bg;
	if (fBold)
		color |= fg_bold;

	/* Suppress black on black */
	if (color == (fg_black|bg_black))
		color |= fg_bold;		

	return color;
}

