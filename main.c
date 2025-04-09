#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

#define FETCH_DEBUG   0
#define ARENA_SIZE    (1024 * 4)
#define TMP_SIZE      (128)
#define DEFAULT_WIDTH (35)

//  ╒══════════════════════════════════════════════╕
//  │              username@hostname               │
//  ╞══════════════════════════════════════════════╡
//	│       OS: /etc/os-release                    │
//	│   Kernel: uname                              │
//	│    Model: /sys/class/dmi/id/product_*        │
//	│    Shell: $SHELL                             │
//	│   Uptime: sysinfo                            │
//	│ Terminal: proc/ppid/stat                     │
//	└──────────────────────────────────────────────┘

typedef struct {
	char *username;
	char *osname;
	char *model;
	char *kernel;
	char *uptime;
	char *shell;
//	char *wm;
	char *terminal;
	char *cpu;
	char *gpu;
	char *memory;
	struct utsname uname;
	struct sysinfo sysinfo;
} Fetch;

///// Flags

enum {
	FLAG_ISATTY,
	FLAG_TRUECOLOR,
	FLAG_COUNT
};
bool g_Flags[FLAG_COUNT];

///// Color

enum {
	C_NONE = -1,
	C_BLACK,  C_RED,  C_GREEN,  C_YELLOW,  C_BLUE,  C_MAGENTA,  C_CYAN,  C_WHITE,
	C_LBLACK, C_LRED, C_LGREEN, C_LYELLOW, C_LBLUE, C_LMAGENTA, C_LCYAN, C_LWHITE,
	C_COUNT
};
unsigned char g_Palette[C_COUNT][3] = { // Default palette based on Konsole 'Breeze'
	/* Normal Black   */ {0x20, 0x28, 0x28},
	/* Normal Red     */ {0xF0, 0x15, 0x15},
	/* Normal Green   */ {0x10, 0xD8, 0x18},
	/* Normal Yellow  */ {0xF0, 0xC0, 0x00},
	/* Normal Blue    */ {0x20, 0x98, 0xF0},
	/* Normal Magenta */ {0x90, 0x48, 0xB0},
	/* Normal Cyan    */ {0x18, 0xA0, 0x88},
	/* Normal White   */ {0xE0, 0xE0, 0xE0},
	/*  Light Black   */ {0x50, 0x58, 0x58},
	/*  Light Red     */ {0xE0, 0x40, 0x30},
	/*  Light Green   */ {0x20, 0xE0, 0x80},
	/*  Light Yellow  */ {0xFF, 0xC0, 0x50},
	/*  Light Blue    */ {0x38, 0xB0, 0xF0},
	/*  Light Magenta */ {0x98, 0x60, 0xB8},
	/*  Light Cyan    */ {0x20, 0xC0, 0xA0},
	/*  Light White   */ {0xFF, 0xFF, 0xFF}
};

void set_color(char fg, char bg, bool bold)
{
	if (!g_Flags[FLAG_ISATTY])
		return;

	// Bold
	printf("\033[%dm", bold ? 1 : 22);

	if (g_Flags[FLAG_TRUECOLOR])
	{
		// RGB Foreground
		if (fg > -1 && fg < C_COUNT)
			printf("\033[38;2;%u;%u;%um", g_Palette[fg][0], g_Palette[fg][1], g_Palette[fg][2]);
		else
			printf("\033[39m");

		// RGB Background
		if (bg > -1 && bg < C_COUNT)
			printf("\033[48;2;%u;%u;%um", g_Palette[bg][0], g_Palette[bg][1], g_Palette[bg][2]);
		else
			printf("\033[49m");
	}
	else
	{
		// ANSI Foreground
		if (fg > -1 && fg < C_COUNT)
			printf("\033[%dm", (fg < 8) ? 30 + fg : 82 + fg);
		else
			printf("\033[39m");

		// ANSI Background
		if (bg > -1 && bg < C_COUNT)
			printf("\033[%dm", (bg < 8) ? 40 + bg : 92 + bg);
		else
			printf("\033[49m");
	}
}

void reset_color()
{
	if (!g_Flags[FLAG_ISATTY])
		return;

	printf("\033[0m");
}

///// String Arena

char   g_StrArena[ARENA_SIZE];
size_t g_StrArenaIndex;
int    g_StrLongest;
int    g_BoxWidth = DEFAULT_WIDTH;

char *ar_append(const char *str)
{
	char *result = NULL;
	int   len = 0;

	if (str == NULL)
		return NULL;

	len = strlen(str);

	if (len != 0 && (g_StrArenaIndex + len + 1) <= ARENA_SIZE);
	{
		if (len > g_StrLongest) g_StrLongest = len;
		result = g_StrArena + g_StrArenaIndex;
		strncpy(g_StrArena + g_StrArenaIndex, str, len + 1);
		g_StrArenaIndex += len + 1;
	}

	return result;
}

char *concatenate(char *dst, int dst_size, const char *src, int src_size)
{
	int dst_len = 0;
	int size = 0;

	if (dst == NULL || src == NULL)
		return NULL;

	dst_len = strlen(dst);
	size    = strlen(src);

	if (src_size < size)
		size = src_size;

	if (dst_len + size + 1 > dst_size)
		size -= (dst_len + size + 1) - dst_size;

	if (size < 0)
		return dst;

	strncpy(dst + dst_len, src, size);

	return dst;
}

///// Get

bool get_truecolor()
{
	FILE *proc = NULL;
	char tmp[TMP_SIZE] = {0};

	// Check if process is attached to a tty
	if (!g_Flags[FLAG_ISATTY])
		return false;

	// $COLORTERM
	proc = popen("echo $COLORTERM", "r");
	if (proc != NULL)
	{
		while(fgets(tmp, TMP_SIZE, proc) != NULL);
		pclose(proc);

		if (strstr(tmp, "truecolor") != NULL || strstr(tmp, "24bit") != NULL)
			return true;
	}

	// $TERM
	proc = popen("echo $TERM", "r");
	if (proc != NULL)
	{
		while(fgets(tmp, TMP_SIZE, proc) != NULL);
		pclose(proc);

		if (strstr(tmp, "256color") != NULL || strstr(tmp, "xterm-kitty") != NULL)
			return true;
	}

	return false;
}

void get_name(Fetch *fetch)
{
	char tmp[TMP_SIZE] = {0};
	char *name = getlogin();
	if (name != NULL)
	{
		snprintf(tmp, TMP_SIZE, "%s@%s", name, fetch->uname.nodename);
		fetch->username = ar_append(tmp);
	}
}

void get_os(Fetch *fetch)
{
	char tmp[TMP_SIZE] = {0};
	char *osname = NULL;
	FILE *file = NULL;

	file = fopen("/etc/os-release", "r");
	if (file == NULL)
	{
		file = fopen("/usr/lib/os-release", "r");
		if (file == NULL)
			return;
	}

	while(fgets(tmp, TMP_SIZE, file) != NULL)
	{
		if (strstr(tmp, "PRETTY_NAME") != NULL)
		{
			osname = strtok(tmp,  "=\"\n");
			osname = strtok(NULL, "=\"\n");
			fetch->osname = ar_append(osname);
		}
	}

	fclose(file);
}

void get_model(Fetch *fetch)
{
	char tmp[TMP_SIZE] = {0};
	char model[TMP_SIZE] = {0};
	FILE *file = NULL;

	file = fopen("/sys/class/dmi/id/product_name", "r");
	if (file != NULL)
	{
		while(fgets(tmp, TMP_SIZE, file) != NULL)
		{
			char *token = strtok(tmp, "\n");
			if (token != NULL)
				concatenate(model, TMP_SIZE, token, strlen(token));
		}
		fclose(file);
	}

	file = fopen("/sys/class/dmi/id/product_version", "r");
	if (file != NULL)
	{
		while(fgets(tmp, TMP_SIZE, file) != NULL)
		{
			char *token = strtok(tmp, "\n");
			if (token != NULL)
			{
				if (strlen(model) > 0)
					concatenate(model, TMP_SIZE, " ", 1);
				concatenate(model, TMP_SIZE, token, strlen(token));
			}
		}
		fclose(file);
	}

	if (strlen(model) > 0)
		fetch->model = ar_append(model);
}

void get_kernel(Fetch *fetch)
{
	char tmp[TMP_SIZE] = {0};
	snprintf(tmp, TMP_SIZE, "%s", fetch->uname.release);
	//snprintf(tmp, TMP_SIZE, "%s %s", fetch->uname.release, fetch->uname.version);
	fetch->kernel = ar_append(tmp);
}

void get_uptime(Fetch *fetch)
{
	char tmp[TMP_SIZE] = {0};
	char uptime_str[TMP_SIZE] = {0};

	long uptime = fetch->sysinfo.uptime;
	int mins   = (uptime / 60) % 60;
	int hours  = (uptime / 3600) % 24;
	int days   = (uptime / 86400);

	if (uptime >= 86400)
		snprintf(tmp, TMP_SIZE, "%d days, %d hours, %d mins", days, hours, mins);
	else if (uptime >= 3600)
		snprintf(tmp, TMP_SIZE, "%d hours, %d mins", hours, mins);
	else
		snprintf(tmp, TMP_SIZE, "%d mins", mins);

	fetch->uptime = ar_append(tmp);
}

void get_shell(Fetch *fetch)
{
	char tmp[TMP_SIZE] = {0};
	char shell[TMP_SIZE] = {0};
	FILE *proc = NULL;

	// $SHELL
	proc = popen("echo ${SHELL##*/}", "r");
	if (proc != NULL)
	{
		while(fgets(tmp, TMP_SIZE, proc) != NULL)
		{
			char *token = strtok(tmp, "\n");
			if (token != NULL)
				concatenate(shell, TMP_SIZE, token, strlen(token));
		}
		pclose(proc);
	}

	// bash
	if (strncmp(shell, "bash", TMP_SIZE) == 0)
	{
		proc = popen("$SHELL -c 'echo $BASH_VERSION'", "r");
		if (proc != NULL)
		{
			while(fgets(tmp, TMP_SIZE, proc) != NULL)
			{
				char *token = strtok(tmp, "\n");
				if (token != NULL)
				{
					concatenate(shell, TMP_SIZE, " ", 1);
					concatenate(shell, TMP_SIZE, token, strlen(token));
				}
			}
			pclose(proc);
		}
	}

	if (strlen(shell) > 1)
		fetch->shell = ar_append(shell);
}

//void get_wm(Fetch *fetch)
//{
//}

pid_t get_parent_pid(pid_t pid)
{
	char tmp[TMP_SIZE] = {0};
	pid_t ppid = -1;
	FILE *file = NULL;

	snprintf(tmp, TMP_SIZE, "/proc/%d/stat", pid);
	file = fopen(tmp, "r");
	if (file != NULL)
	{
		while(fgets(tmp, TMP_SIZE, file) != NULL)
		{
			char *result = strtok(tmp, " ");
			for(int i = 0; i < 3; i += 1)
				result = strtok(NULL, " ");
			ppid = atol(result);
			break;
		}
		fclose(file);
	}

	return ppid;
}

void get_proc_name(pid_t pid, char *str, size_t size)
{
	char tmp[TMP_SIZE] = {0};
	FILE *file = NULL;

	// Clear buffer
	memset(str, 0, sizeof(char) * size);

	snprintf(tmp, TMP_SIZE, "/proc/%d/stat", pid);
	file = fopen(tmp, "r");
	if (file != NULL)
	{
		while(fgets(tmp, TMP_SIZE, file) != NULL)
		{
			char *result = strtok(tmp, " ()");
			result = strtok(NULL, " ()");

			if (result != NULL)
				strncpy(str, result, size);

			break;
		}
		fclose(file);
	}
}

void get_terminal(Fetch *fetch)
{
	char tmp[TMP_SIZE] = {0};
	pid_t pid = get_parent_pid(getppid());
	get_proc_name(pid, tmp, TMP_SIZE);

	if (strlen(tmp) > 1)
		fetch->terminal = ar_append(tmp);
}

void get_cpu(Fetch *fetch)
{
	char tmp[TMP_SIZE] = {0};
	FILE *file = NULL;

	file = fopen("/proc/cpuinfo", "r");
	if (file == NULL)
		return;

	while(fgets(tmp, TMP_SIZE, file) != NULL)
	{
		if (strstr(tmp, "model name") != NULL)
		{
			char *token = strtok(tmp, ":");
			token = strtok(NULL, ":\n");
			fetch->cpu = ar_append(token + 1);
			break;
		}
	}

	fclose(file);
}

///// Print

void print_name(Fetch *fetch)
{
	int len = strlen(fetch->username);
	int mid = (g_BoxWidth / 2) - (len / 2);

	printf("╒");
	for(int i = 0; i < g_BoxWidth; i++)
		printf("═");
	printf("╕\n");
		
	printf("│");
	for(int i = 0; i < (g_BoxWidth / 2) - (len / 2); i++)
		printf(" ");
	set_color(C_LCYAN, -1, true);
	printf("%s", fetch->username);
	reset_color();
	for(int i = 0; i < g_BoxWidth - (mid + len); i++)
		printf(" ");
	printf("│\n");

	printf("╞");
	for(int i = 0; i < g_BoxWidth; i++)
		printf("═");
	printf("╡\n");
}

void print_tag(const char *tag, char *str)
{
	if (tag == NULL || str == NULL)
		return;

	int tag_len = strlen(tag) + 3;
	int str_len = strlen(str);

	printf("│ ");
	set_color(C_LGREEN, -1, true);
	printf("%s\033[0m: ", tag);
	set_color(C_LYELLOW, -1, false);
	printf("%s", str);
	reset_color();
	for(int i = 0; i < (g_BoxWidth - str_len - tag_len); i++)
		printf(" ");
	printf("│\n");
}


void print_colors()
{
	if (!g_Flags[FLAG_ISATTY])
		return;

	int mid   = g_BoxWidth / 2;

	// Empty Line
	printf("│");
	for(int i = 0; i < g_BoxWidth; i++)
		printf(" ");
	printf("│");

	// Dark Colors
	printf("\n│");
	for(int i = 0; i < mid - 8; i++)
		printf(" ");
	for(int i = 0; i < C_COUNT / 2; i+=1)
	{
		set_color(-1, i, false);
		printf("  ");
	}
	reset_color();
	for(int i = 0; i < g_BoxWidth - (mid + 8); i++)
		printf(" ");
	printf("│");

	// Bright Colors
	printf("\n│");
	for(int i = 0; i < mid - 8; i++)
		printf(" ");
	for(int i = 0; i < C_COUNT / 2; i+=1)
	{
		set_color(-1, i+8, false);
		printf("  ");
	}
	reset_color();
	for(int i = 0; i < g_BoxWidth - (mid + 8); i++)
		printf(" ");
	printf("│");

	printf("\n");
}

///// Main

int main(int arcg, char *argv[])
{
	Fetch fetch;

	// Initialize to Zero
	memset(&fetch, 0, sizeof(&fetch));

	// Cache Uname and Sysinfo
	if (uname(&fetch.uname) != 0 || sysinfo(&fetch.sysinfo) != 0)
		exit(EXIT_FAILURE);

	// Check if Output is a TTY
	g_Flags[FLAG_ISATTY] = isatty(STDOUT_FILENO);

	// Fetch
	g_Flags[FLAG_TRUECOLOR] = get_truecolor();
	get_name(&fetch);
	get_os(&fetch);
	get_model(&fetch);
	get_kernel(&fetch);
	get_uptime(&fetch);
	get_shell(&fetch);
	get_terminal(&fetch);
	get_cpu(&fetch);

	// Print
	if (g_StrLongest + 12 > g_BoxWidth)
		g_BoxWidth = g_StrLongest + 12;

	print_name(&fetch);
	print_tag("OS", fetch.osname);
	print_tag("Model", fetch.model);
	print_tag("Kernel", fetch.kernel);
	print_tag("Uptime", fetch.uptime);
	print_tag("Shell", fetch.shell);
	print_tag("Terminal",fetch.terminal);
	print_tag("CPU", fetch.cpu);
	print_colors();

	printf("└");
	for (int i = 0; i < g_BoxWidth; i++)
		printf("─");
	printf("┘\n");

#if FETCH_DEBUG
	printf("\n[ DEBUG ]\n");
	printf("Arena Size: [ \033[1m\033[92m%d\033[0m / \033[1m\033[92m%d\033[0m ]\n", g_StrArenaIndex, ARENA_SIZE);
	printf("Longest String: %d\n", g_StrLongest);
	printf("Longest Tag (Terminal): %d\n", 8);
	printf("Spacing: %d\n", 4);
	printf("Proposed Box Width: %d\n", g_StrLongest + 8 + 4);
	printf("Minimum Box Width: %d\n", DEFAULT_WIDTH);
	printf("Box Width: %d\n", g_BoxWidth);
	printf("Arena Data\n>>>\n");
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

