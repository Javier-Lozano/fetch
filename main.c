#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

#define FETCH_DEBUG      1
#define FETCH_ARENA_SIZE (1024 * 4)
#define FETCH_TMP_SIZE   (128)

///// Types

// Fetch

enum {
	FLAG_OPT_ALL_COLS,   // Use all horizontal space available. Set trough Command Line Arguments
	FLAG_OPT_SIMPLE,     // Show information in a simple way.   Set trough Command Line Arguments
	FLAG_SHOW_TRUECOLOR, // Terminal support RGB colors.        Set trough 'check_truecolor()'
	FLAG_SHOW_NAME,      // Show Username and Hostname.         Set trough 'cache_uname()'
	FLAG_SHOW_DISTRO,    // Show OS Distro Name.                Set trough 'cache_distro_name()'
	FLAG_COUNT
};

typedef struct fetch_st {
	// User
	int  rows, cols;
	char *username;

	// Uname
	char *sysname;  // OS implementation
	char *nodename; // Hostname
	char *release;  // Kernel release
	char *version;  // Kernel OS version
	char *machine;  // Architecture

	// OS
	char *distro_name;
} Fetch;

///// Globals

//// OS
//char *name;        // os-release: "NAME"
//char *id;          // os-release: "ID"
//char *pretty_name; // os-release: "PRETTY_NAME"
//
//// Sys. Info [ Not Cached ]
//struct sysinfo sys; // Not Cached
// Flags
bool g_Flags[FLAG_COUNT];

// Colors Palette

int g_Colors[16][3] = {
	/* Normal Black   */ {0X44, 0X4B, 0X6A},
	/* Normal Red     */ {0XFF, 0X7A, 0X93},
	/* Normal Green   */ {0XB9, 0XF2, 0X7C},
	/* Normal Yellow  */ {0XFF, 0X9E, 0X64},
	/* Normal Blue    */ {0X7D, 0XA6, 0XFF},
	/* Normal Magenta */ {0XBB, 0X9A, 0XF7},
	/* Normal Cyan    */ {0X0D, 0XB9, 0XD7},
	/* Normal White   */ {0XAC, 0XB0, 0XD0},
	/* Light  Black   */ {0X32, 0X34, 0X4A},
	/* Light  Red     */ {0XF7, 0X76, 0X8E},
	/* Light  Green   */ {0X9E, 0XCE, 0X6A},
	/* Light  Yellow  */ {0XE0, 0XAF, 0X68},
	/* Light  Blue    */ {0X7A, 0XA2, 0XF7},
	/* Light  Magenta */ {0XAD, 0X8E, 0XE6},
	/* Light  Cyan    */ {0X44, 0X9D, 0XAB},
	/* Light  White   */ {0X78, 0X7C, 0X99}
};


// Arena
char   g_StrArena[FETCH_ARENA_SIZE];
size_t g_StrArenaIndex;
int    g_StrLongest;

///// Arena

char *ar_append(const char *str)
{
	char *result = NULL;
	int   len = 0;

	if (str == NULL)
		return NULL;

	len = strlen(str);

	if (len != 0 && (g_StrArenaIndex + len + 1) <= FETCH_ARENA_SIZE);
	{
		if (len > g_StrLongest) g_StrLongest = len;
		result = g_StrArena + g_StrArenaIndex;
		strncpy(g_StrArena + g_StrArenaIndex, str, len + 1);
		g_StrArenaIndex += len + 1;
	}

	return result;
}

///// Math Functions

float f_fmodf(float a, float b)
{
	float mod;

	mod = (a < 0) ? -a : a;
	b   = (b < 0) ? -b : b;

	while (mod >= b)
		mod -= b;

	if (a < 0)
		return -mod;

	return mod;
}

float f_fabsf(float a)
{
	float     f = a;
	uint32_t *i = (uint32_t *)&f;
	*i &= 0x7fffffff;
	return f;
}

float f_lerp(float a, float b, float t)
{
	return ((1 - t) * a) + (t * b);
}

void hue_to_rgb(float hue, int *r, int *g, int *b)
{

	float h, x;
	float f_r, f_g, f_b;

	h = f_fmodf(hue, 360) / 60;
	x = (1 - f_fabsf(f_fmodf(h, 2)-1));

	if (h >= 0 && h < 1)
	{
		f_r = 255;
		f_g = x * 255;
		f_b = 0;
	}
	else if (h >= 1 && h < 2)
	{
		f_r = x * 255;
		f_g = 255;
		f_b = 0;
	}
	else if (h >= 2 && h < 3)
	{
		f_r = 0;
		f_g = 255;
		f_b = x * 255;
	}
	else if (h >= 3 && h < 4)
	{
		f_r = 0;
		f_g = x * 255;
		f_b = 255;
	}
	else if (h >= 4 && h < 5)
	{
		f_r = x * 255;
		f_g = 0;
		f_b = 255;
	}
	else if (h >= 5 && h < 6)
	{
		f_r = 255;
		f_g = 0;
		f_b = x * 255;
	}

	*r = f_r;
	*g = f_g;
	*b = f_b;
}

///// Fetch Functions

void check_term_size(int *cols, int *rows)
{
	struct winsize term;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &term) == -1)
	{
		fprintf(stderr, "Couldn't get Terminal Size\n");
		exit(EXIT_FAILURE);
	}

	*cols = term.ws_col;
}

bool check_truecolor()
{
	char tmp[FETCH_TMP_SIZE] = {0};
	FILE *proc = NULL;

	// Check if process is attached to a tty
	if (!isatty(STDOUT_FILENO))
		return false;

	// $COLORTERM
	proc = popen("echo $COLORTERM", "r");
	if (proc != NULL)
	{
		while(fgets(tmp, FETCH_TMP_SIZE, proc) != NULL);
		pclose(proc);

		if (strstr(tmp, "truecolor") != NULL || strstr(tmp, "24bit") != NULL)
			return true;
	}

	// $TERM
	proc = popen("echo $TERM", "r");
	if (proc != NULL)
	{
		while(fgets(tmp, FETCH_TMP_SIZE, proc) != NULL);
		pclose(proc);

		if (strstr(tmp, "256color") != NULL || strstr(tmp, "xterm-kitty") != NULL)
			return true;
	}

	return false;
}

void set_pallete(Fetch *fetch)
{
	if (g_Flags[FLAG_SHOW_TRUECOLOR])
	{
	}
	else
	{
	}
}

void set_fg_color(int n)
{
	if (n > 15 || n < 0)
		return;

	printf("\033[38;2;%d;%d;%dm", g_Colors[n][0], g_Colors[n][1], g_Colors[n][2]);
}


void cache_uname(Fetch *fetch)
{
	char tmp[FETCH_TMP_SIZE] = {0};
	struct utsname buf;

	if (uname(&buf) == -1)
	{
		g_Flags[FLAG_SHOW_NAME] = false;
		return;
	}

	fetch->sysname  = ar_append(buf.sysname);
	fetch->nodename = ar_append(buf.nodename);
	fetch->release  = ar_append(buf.release);
	fetch->version  = ar_append(buf.version);
	fetch->machine  = ar_append(buf.machine);

	if (getlogin_r(tmp, FETCH_TMP_SIZE) != 0)
	{
		g_Flags[FLAG_SHOW_NAME] = false;
		return;
	}
	fetch->username = ar_append(tmp);
	g_Flags[FLAG_SHOW_NAME] = true;
}

void cache_distro_name(Fetch *fetch)
{
	char  tmp[FETCH_TMP_SIZE] = {0};
	char *result = NULL;
	FILE *f      = NULL;

	f = fopen("/etc/os-release", "r");
	if (f == NULL)
	{
		g_Flags[FLAG_SHOW_DISTRO] = false;
		return;
	}

	while(fgets(tmp, FETCH_TMP_SIZE, f) != NULL)
	{
		//printf("%s", tmp);
		if (strstr(tmp, "PRETTY_NAME") != NULL) 
		{
			g_Flags[FLAG_SHOW_DISTRO] = true;
			result = strchr(tmp, '=');
			result = strtok(result, "=\"");
			fetch->distro_name = ar_append(result);
			break;
		}
	}

	fclose(f);
}

///// Printing Functions

void print_name(Fetch *fetch)
{
	int user_len, host_len, len;

	if (!g_Flags[FLAG_SHOW_NAME])
		return;

	user_len = strlen(fetch->username);
	host_len = strlen(fetch->nodename);
	len = user_len + host_len + 1;

	if (g_Flags[FLAG_SHOW_TRUECOLOR])
	{
		printf("\033[1m");
		printf("\033[38;2;%d;%d;%dm", 171, 233, 179);
		printf("%s", fetch->username);
		printf("\033[38;2;%d;%d;%dm", 242, 143, 173);
		printf("@%s\n", fetch->nodename);
		printf("\033[38;2;%d;%d;%dm", 185, 193, 221);
	}
	else
	{
		printf("\033[1m\033[92m%s\033[95m@%s\n", fetch->username, fetch->nodename);
	}

	for(int i = 0; i < len; i++)
		printf("═");

	printf("\033[0m\n");
}

void print_distro(Fetch *fetch)
{
	if (g_Flags[FLAG_SHOW_DISTRO])
	{
		if (g_Flags[FLAG_SHOW_TRUECOLOR])
		{
			printf("\033[1m");
			printf("\033[38;2;%d;%d;%dm", 245, 194, 231);
			printf("OS: ");
			printf("\033[38;2;%d;%d;%dm", 185, 193, 221);
			printf("%s\n", fetch->distro_name);
		}
		else
		{
			printf("\033[1m\033[92mOS:\033[0m %s\n", fetch->distro_name);
		}
	}
	else
	{
		if (g_Flags[FLAG_SHOW_TRUECOLOR])
		{
			printf("\033[1m");
			printf("\033[38;2;%d;%d;%dm", 245, 194, 231);
			printf("OS: ");
			printf("\033[38;2;%d;%d;%dm", 185, 193, 221);
			printf("%s\n", fetch->sysname);
		}
		else
		{
			printf("\033[1m\033[92mOS:\033[0m %s\n", fetch->sysname);
		}
	}
	printf("\033[0m");
}

void print_color(Fetch *fetch)
{
	//const int sections = fetch->cols;
	const int sections = 80;
	int r, g, b;
	float hue;

	printf("\n");
	if (g_Flags[FLAG_SHOW_TRUECOLOR])
	{
		for(int i = 0; i < sections; i++)
		{
			hue = ((float)(i+1) / (float)sections) * 360;
			hue_to_rgb(hue, &r, &g, &b);
			printf("\033[48;2;%d;%d;%dm ", r, g, b);
		}
		printf("\n");
	}
	else
	{
		printf( "\033[40m  "
				"\033[41m  "
				"\033[42m  "
				"\033[43m  "
				"\033[44m  "
				"\033[45m  "
				"\033[46m  "
				"\033[47m  \n"
				"\033[100m  "
				"\033[101m  "
				"\033[102m  "
				"\033[103m  "
				"\033[104m  "
				"\033[105m  "
				"\033[106m  "
				"\033[107m  "
				"\033[0m\n");
	}
	printf("\033[0m");
}

void init(Fetch *fetch)
{
	// Clear Fetch
	memset(fetch, 0, sizeof(Fetch));

	// TODO: Process Options

	// Check Truecolor
	g_Flags[FLAG_SHOW_TRUECOLOR] = check_truecolor();

	// Get Username and chache Uname
	cache_uname(fetch);

	// Get Distro Name
	cache_distro_name(fetch);

	// Get Terminal Columns and Rows
	check_term_size(&fetch->cols, &fetch->rows);
}


int main(void)
{
	Fetch fetch;
	init(&fetch);

	print_name(&fetch);
	print_distro(&fetch);
	print_color(&fetch);
//	// Get OS and Distro
//	cache_os_release(&fetched);
//	// Get Host Machine info
//	cache_host_machine(&fetched);
//	// Get System info (Not Cached)
//	if (sysinfo(&fetched.sys) == -1)
//	{
//		printf("Couldn't fetch sysinfo.\n");
//		exit(EXIT_FAILURE);
//	}
//
//	// usernmae@hostname
//	printf("\033[1;31m%s\033[m@\033[1;31m%s\033[m\n", fetched.username, fetched.kernel.nodename);
//	for(int i = 0; i < (strlen(fetched.username) + strlen(fetched.kernel.nodename) + 1); i += 1)
//		printf("-");
//
//	printf("\n");
//	// OS
//	printf("\033[1;31mOS\033[m: %s %s\n", fetched.os.pretty_name, fetched.kernel.machine);
//	// Host
//	printf("\033[1;31mHOST\033[m: %s\n", fetched.machine_name);
//	// Kernel
//	printf("\033[1;31mKernel Release\033[m: %s\n", fetched.kernel.release, fetched.kernel.version);
//	printf("\033[1;31mKernel Version\033[m: %s\n", fetched.kernel.version);
//	// Uptime
//	int uptime = fetched.sys.uptime;
//	int sec    = uptime % 60;
//	int min    = (uptime / 60) % 60;
//	int hour   = ((uptime / 60) / 60) % 24;
//	int day    = ((uptime / 60) / 60) / 24;
//	printf("\033[1;31mUptime\033[m: ");
//	if (day > 0)
//		printf("%d %s ", day, (day > 1) ? "days" : "day");
//	if (hour > 0)
//		printf("%d %s ", hour, (hour > 1) ? "hours" : "hour");
//	printf("%d %s ", min, (min > 1) ? "mins" : "min");
//	printf("%d %s ", sec, (sec > 1) ? "secs" : "sec");
//	printf("\n");

#if FETCH_DEBUG
	printf("\nDebug:\n");
	printf("Terminal Size: %dx%d\n", fetch.cols, fetch.rows);
	for(int i = 0; i < fetch.cols; i++)
		printf("*");
	printf("\n");
	printf("Arena Count: [ \033[1m\033[92m%d\033[0m / \033[1m\033[92m%d\033[0m ]\n", g_StrArenaIndex, FETCH_ARENA_SIZE);
	printf("Arena Data:\n>>>\n");
	for(int i = 0; i < g_StrArenaIndex; i++)
	{
		char ch = g_StrArena[i];

		if ((unsigned char)ch < 32)
			printf("[0x%02X]", ch);
		else
			printf("\033[93m%c\033[0m", ch);

		if (ch == 0)
			printf("\n");
	}
	printf("<<<\n");
#endif

	return 0;
}
