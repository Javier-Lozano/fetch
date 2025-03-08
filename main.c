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
	struct fetch_kernel_st {
		char *sysname;  // OS implementation
		char *nodename; // Hostname
		char *release;  // Kernel release
		char *version;  // Kernel OS version
		char *machine;  // Architecture
	} kernel;
	struct fetch_os_st {
		char *name;
		char *id;
		char *pretty_name;
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

char *get_value(char *line)
{
	char *result = strtok(line, "=\"");
	result = strtok(NULL, "=\"");
	return result;
}

void cache_os_release(struct fetch_st *fetched)
{
	FILE *file     = NULL;
	char line[LINE_SIZE] = {0};
	int  line_i    =  0;
	char ch        =  0;
	bool name_flag = true, id_flag = true, pretty_flag = true;

	// Open find and open 'os-release'
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

		// Check line, if line size is bigger than LINE_SIZE it gets truncated
		if (ch == '\n' || line_i == LINE_SIZE)
		{
			// Add NULL byte at the end
			line[line_i-1] = 0;

			if (name_flag && strstr(line, "NAME") != NULL && line[0] == 'N')
			{
				fetched->os.name = ar_strcpy(get_value(line));
				name_flag = false;
			}
			else if (id_flag && strstr(line, "ID") != NULL && line[0] == 'I')
			{
				fetched->os.id = ar_strcpy(get_value(line));
				id_flag = false;
			}
			else if (pretty_flag && strstr(line, "PRETTY_NAME") != NULL && line[0] == 'P')
			{
				fetched->os.pretty_name = ar_strcpy(get_value(line));
				pretty_flag = false;
			}

			// Reset line index
			line_i = 0;
		}
	}

	fclose(file);
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

	// usernmae@hostname
	printf("\033[1;31m%s\033[m@\033[1;31m%s\033[m\n", fetched.username, fetched.kernel.nodename);
	for(int i = 0; i < (strlen(fetched.username) + strlen(fetched.kernel.nodename) + 1); i += 1)
		printf("-");

	printf("\n");
	// OS
	printf("\033[1;31mOS\033[m: %s %s\n", fetched.os.pretty_name, fetched.kernel.machine);
	printf("\033[1;31mKernel Release\033[m: %s\n", fetched.kernel.release, fetched.kernel.version);
	printf("\33[1;31mKernel Version\033[m: %s\n", fetched.kernel.version);


#if ARENA_DEBUG
	printf("\nDebug:\nArena Count: [ %d / %d ]\n", g_StrArenaIndex, STR_ARENA_SIZE);
	printf("Arena Data:\n>>>");
	for(int i = 0; i < g_StrArenaIndex; i++)
	{
		char ch = g_StrArena[i];

		if ((unsigned char)ch < 32)
			printf("[0x%02X]", ch);
		else
			printf("%c", ch);
	}
	printf("<<<\n");
#endif

error:
	return -1;
}
