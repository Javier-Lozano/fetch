#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#define ARENA_DEBUG 1
#define STR_ARENA_SIZE (1024 * 4)

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

void get_os_release_value(char *line)
{
}

void cache_os_release(struct fetch_st *fetched)
{
    FILE *file     = NULL;
    char line[128] = {0};
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
	line[line_i] = ch;
	line_i += 1;
	// Check line
	if (ch == '\n' || line_i == 128)
	{
	    line[line_i-1] = 0;

	    // Check Flags
	    if (name_flag && strstr(line, "NAME") != NULL && line[0] == 'N')
	    {
		  get_os_release_value(line);
		  name_flag = false;
	    }

	    // Reset index
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
    printf("%s@%s\n", fetched.username, fetched.kernel.nodename);
    // OS
    printf("OS: %s\n", fetched.os.pretty_name);
    printf("Kernel Release: %s\nKernel Version: %s\n", fetched.kernel.release, fetched.kernel.version);


#if ARENA_DEBUG
    printf("\nDebug:\nArena Count: [ %d / %d]\n", g_StrArenaIndex, STR_ARENA_SIZE);
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
