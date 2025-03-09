#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#define ARENA_DEBUG 1
#define STR_ARENA_SIZE (1024 * 4)
#define LINE_SIZE (128)

struct fetch_st {
	char *username;
	char *machine_name;
	struct fetch_kernel_st {
		char *sysname;  // OS implementation
		char *nodename; // Hostname
		char *release;  // Kernel release
		char *version;  // Kernel OS version
		char *machine;  // Architecture
	} kernel;
	struct fetch_os_st {
		char *name;        // os-release: "NAME"
		char *id;          // os-release: "ID"
		char *pretty_name; // os-release: "PRETTY_NAME"
	} os;
	struct sysinfo sys; // Not Cached
};

char   g_StrArena[STR_ARENA_SIZE];
size_t g_StrArenaIndex;

char *ar_strcpy(const char *str)
{
	int   len = 0;

	if (str == NULL)
		return NULL;

	len = strlen(str);

	if (len != 0 && (g_StrArenaIndex + len + 1) <= STR_ARENA_SIZE);
	{
		char *result = g_StrArena + g_StrArenaIndex;
		strncpy(g_StrArena + g_StrArenaIndex, str, len + 1);
		g_StrArenaIndex += len + 1;
		return result;
	}

	return NULL;
}

void cache_uname(struct fetch_st *fetched)
{
	struct utsname buf;
	if (uname(&buf) == -1)
	{
		fprintf(stderr, "Could't fetch utsname.\n");
		exit(EXIT_FAILURE);
	}
	fetched->kernel.sysname  = ar_strcpy(buf.sysname);
	fetched->kernel.nodename = ar_strcpy(buf.nodename);
	fetched->kernel.release  = ar_strcpy(buf.release);
	fetched->kernel.version  = ar_strcpy(buf.version);
	fetched->kernel.machine  = ar_strcpy(buf.machine);
}

char *get_value(const char *line, const char *key)
{
	char *result = NULL;

	if (strstr(line, key) != NULL && line[0] == key[0])
	{
		result = strchr(line, '=');
		result = strtok(result, "=\"");
	}

	return result;
}

void cache_os_release(struct fetch_st *fetched)
{
	FILE *file           = NULL;
	char line[LINE_SIZE] = {0};
	int  line_i          = 0;
	char ch              = 0;
	bool name = true, id = true, pretty = true;

	// Open 'os-release'
	file = fopen("/etc/os-release", "r");
	if (file == NULL)
	{
		file = fopen("/usr/lib/os-release","r");
		if (file == NULL)
		{
			fprintf(stdout, "Couldn't fetch OS information.\n");
			exit(EXIT_FAILURE);
		}
	}

	// Read file line by line
	while((ch = fgetc(file)) != EOF)
	{
		// Append line
		line[line_i] = ch;
		line_i += 1;

		if (ch == '\n' || line_i == LINE_SIZE)
		{
			line[line_i-1] = 0;

			char *value = NULL;
			if (name && (value = get_value(line, "NAME")) != NULL)
			{
				fetched->os.name = ar_strcpy(value);
				name = false;
			}
			else if (id && (value = get_value(line, "ID")) != NULL)
			{
				fetched->os.id = ar_strcpy(value);
				id = false;
			}
			else if (pretty && (value = get_value(line, "PRETTY_NAME")) != NULL)
			{
				fetched->os.pretty_name = ar_strcpy(value);
				pretty = false;
			}

			// Reset line index
			line_i = 0;
		}
	}

	fclose(file);
}

void cache_host_machine(struct fetch_st *fetched)
{
	const char *product_name    = "/sys/devices/virtual/dmi/id/product_name";
	const char *product_version = "/sys/devices/virtual/dmi/id/product_version";

	FILE *file           = NULL;
	char line[LINE_SIZE] = {0};
	char line_i          = 0;
	char ch              = 0;
	bool name = true, version =  true;

	// product_name
	file = fopen(product_name, "r");
	if (file == NULL)
		name = false;

	if (name)
	{
		while((ch = fgetc(file)) != EOF && line_i < LINE_SIZE)
		{
			line[line_i] = ch;
			line_i += 1;
		}
		line[line_i-1] = 0;
		fclose(file);
	}

	// product_version
	file = fopen(product_version, "r");
	if (file == NULL)
		version = false;

	if (version)
	{
		if (name)
		{
			line[line_i-1] = ' ';
		}

		while((ch = fgetc(file)) != EOF && line_i < LINE_SIZE)
		{
			line[line_i] = ch;
			line_i += 1;
		}
		line[line_i-1] = 0;

		fclose(file);
	}


	if (!name && !version)
	{
		printf("Couldn't find machine host name\n");
		exit(EXIT_FAILURE);
	}

	fetched->machine_name = ar_strcpy(line);
}

// TODO: Add Options
// TODO: Embelish with a CURSES/TUI LIbrary

int main(void)
{
	struct fetch_st fetched;
	memset(&fetched, 0, sizeof(struct fetch_st));

	// Get Userna name
	fetched.username = ar_strcpy(getlogin());
	// Get Kernel info
	cache_uname(&fetched);
	// Get OS and Distro
	cache_os_release(&fetched);
	// Get Host Machine info
	cache_host_machine(&fetched);
	// Get System info (Not Cached)
	if (sysinfo(&fetched.sys) == -1)
	{
		printf("Couldn't fetch sysinfo.\n");
		exit(EXIT_FAILURE);
	}

	// usernmae@hostname
	printf("\033[1;31m%s\033[m@\033[1;31m%s\033[m\n", fetched.username, fetched.kernel.nodename);
	for(int i = 0; i < (strlen(fetched.username) + strlen(fetched.kernel.nodename) + 1); i += 1)
		printf("-");

	printf("\n");
	// OS
	printf("\033[1;31mOS\033[m: %s %s\n", fetched.os.pretty_name, fetched.kernel.machine);
	// Host
	printf("\033[1;31mHOST\033[m: %s\n", fetched.machine_name);
	// Kernel
	printf("\033[1;31mKernel Release\033[m: %s\n", fetched.kernel.release, fetched.kernel.version);
	printf("\033[1;31mKernel Version\033[m: %s\n", fetched.kernel.version);
	// Uptime
	int uptime = fetched.sys.uptime;
	int sec    = uptime % 60;
	int min    = (uptime / 60) % 60;
	int hour   = ((uptime / 60) / 60) % 24;
	int day    = ((uptime / 60) / 60) / 24;
	printf("\033[1;31mUptime\033[m: ");
	if (day > 0)
		printf("%d %s ", day, (day > 1) ? "days" : "day");
	if (hour > 0)
		printf("%d %s ", hour, (hour > 1) ? "hours" : "hour");
	printf("%d %s ", min, (min > 1) ? "mins" : "min");
	printf("%d %s ", sec, (sec > 1) ? "secs" : "sec");
	printf("\n");

#if ARENA_DEBUG
	printf("\nDebug:\nArena Count: [ %d / %d ]\n", g_StrArenaIndex, STR_ARENA_SIZE);
	printf("Arena Data:\n>>>\n");
	for(int i = 0; i < g_StrArenaIndex; i++)
	{
		char ch = g_StrArena[i];

		if ((unsigned char)ch < 32)
			printf("[0x%02X]", ch);
		else
			printf("%c", ch);

		if (ch == 0)
			printf("\n");
	}
	printf("<<<\n");
#endif

	return 0;
}
