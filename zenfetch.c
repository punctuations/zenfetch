/*
 * zenfetch - A system info display tool using cbonsai
 *
 * This file is designed to be part of the cbonsai repository.
 * It calls cbonsai directly to display a bonsai tree, then shows system info.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <time.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cbonsai.h"

#define MAX_BUF 512
#define LABEL_WIDTH 18
#define BLOCK_WIDTH 70

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Global noir mode flag (no colors, bold labels)
static int noir_mode = 0;

// Configuration file paths
#define CONFIG_LOCATION "/etc/zenfetch/location"
#define CONFIG_OWNER    "/etc/zenfetch/owner"
#define CONFIG_SUPPORT  "/etc/zenfetch/support"
#define CONFIG_DOCS     "/etc/zenfetch/docs"

static void print_help(void) {
    printf(
        "Usage: zenfetch [OPTIONS]\n"
        "\n"
        "Display system info with a bonsai tree.\n"
        "\n"
        "Options:\n"
        "  -o, --owner TEXT      set owner name in welcome message\n"
        "  -l, --location TEXT   set location\n"
        "  -s, --support TEXT    set support contact info\n"
        "  -d, --docs URL        set documentation URL\n"
        "  -S, --no-support      hide support/docs section\n"
        "  -I, --hide-ip         hide NODE IP field\n"
        "  -n, --noir            noir mode: no colors, bold labels\n"
        "  -h, --help            show this help\n"
        "\n"
        "Config files (one value per line, CLI overrides these):\n"
        "  /etc/zenfetch/owner\n"
        "  /etc/zenfetch/location\n"
        "  /etc/zenfetch/support\n"
        "  /etc/zenfetch/docs\n"
    );
}

/*
 * Run cbonsai to display the tree
 */
static void run_cbonsai(void) {
    // Reset getopt for cbonsai's argument parsing
    optind = 1;

    if (noir_mode) {
        char *args[] = {
            "cbonsai",
            "-n",           // noir mode
            "-b", "3",
            "-p",           // print mode
            "-l",           // live mode
            "-t", "0.003",  // fast growth
            NULL
        };
        cbonsai_run(8, args);
    } else {
        char *args[] = {
            "cbonsai",
            "-b", "3",
            "-p",           // print mode
            "-l",           // live mode
            "-t", "0.003",  // fast growth
            NULL
        };
        cbonsai_run(7, args);
    }
}

// Get terminal width
static int get_term_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        return 80;
    }
    return w.ws_col;
}

// Print padding for centering
static void print_padding(int term_width, int content_width) {
    int pad = (term_width - content_width) / 2;
    if (pad < 0) pad = 0;
    for (int i = 0; i < pad; i++) {
        putchar(' ');
    }
}

// Print a centered line
static void print_centered(int term_width, const char *text) {
    print_padding(term_width, (int)strlen(text));
    printf("%s\n", text);
}

// Print a label-value pair (centered block, left-aligned labels)
static void print_info(int term_width, const char *label, const char *value) {
    print_padding(term_width, BLOCK_WIDTH);
    if (noir_mode) {
        printf(COLOR_BOLD "%-*s" COLOR_RESET " %s\n", LABEL_WIDTH, label, value);
    } else {
        printf(COLOR_CYAN "%-*s" COLOR_RESET " %s\n", LABEL_WIDTH, label, value);
    }
}

// Check if string looks like an email (user@domain.tld)
static int looks_like_email(const char *str) {
    const char *at = strchr(str, '@');
    if (!at || at == str) return 0;  // no @ or nothing before @
    const char *dot = strchr(at, '.');
    if (!dot || dot == at + 1) return 0;  // no dot after @ or nothing between @ and .
    if (!*(dot + 1)) return 0;  // nothing after the dot
    // Make sure no spaces before the @
    for (const char *p = str; p < at; p++) {
        if (*p == ' ') return 0;
    }
    return 1;
}

// Check if string looks like a URL (has scheme or looks like domain.tld/...)
static int looks_like_url(const char *str) {
    if (strncmp(str, "http://", 7) == 0 || strncmp(str, "https://", 8) == 0)
        return 1;
    if (looks_like_email(str)) return 0;  // emails are not URLs
    // Check for domain-like pattern: has a dot before any space/end
    const char *dot = strchr(str, '.');
    const char *space = strchr(str, ' ');
    return dot && (!space || dot < space);
}

// Print a label with a clickable link (OSC 8 hyperlink)
// Handles URLs (auto-prepends https://) and emails (uses mailto:)
static void print_link(int term_width, const char *label, const char *url, int is_email) {
    print_padding(term_width, BLOCK_WIDTH);
    const char *label_style = noir_mode ? COLOR_BOLD : COLOR_CYAN;
    // OSC 8 hyperlink: ESC ] 8 ; ; URL BEL text ESC ] 8 ; ; BEL
    if (is_email) {
        printf("%s%-*s" COLOR_RESET " \033]8;;mailto:%s\a%s\033]8;;\a\n",
               label_style, LABEL_WIDTH, label, url, url);
    } else {
        int has_scheme = (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0);
        if (has_scheme) {
            printf("%s%-*s" COLOR_RESET " \033]8;;%s\a%s\033]8;;\a\n",
                   label_style, LABEL_WIDTH, label, url, url);
        } else {
            printf("%s%-*s" COLOR_RESET " \033]8;;https://%s\a%s\033]8;;\a\n",
                   label_style, LABEL_WIDTH, label, url, url);
        }
    }
}

// Read a line from a file
static int read_file_line(const char *path, char *buf, size_t size) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (fgets(buf, size, f) == NULL) {
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[strcspn(buf, "\n")] = 0;
    return 0;
}

// Read from config file with fallback
static void read_config(const char *path, char *buf, size_t size, const char *fallback) {
    if (read_file_line(path, buf, size) != 0) {
        strncpy(buf, fallback, size - 1);
        buf[size - 1] = 0;
    }
}

// Get CPU model name and core count
static void get_cpu_info(char *buf, size_t size) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        snprintf(buf, size, "Unknown");
        return;
    }
    
    char line[256];
    char model[256] = "Unknown";
    int cores = 0;
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ' || *colon == '\t') colon++;
                strncpy(model, colon, sizeof(model) - 1);
                model[strcspn(model, "\n")] = 0;
            }
        }
        if (strncmp(line, "processor", 9) == 0) {
            cores++;
        }
    }
    fclose(f);
    
    snprintf(buf, size, "%s %d core", model, cores);
}

// Get memory info
static void get_memory_info(char *buf, size_t size) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) {
        snprintf(buf, size, "Unknown");
        return;
    }
    
    char line[256];
    unsigned long total = 0, available = 0;
    
    while (fgets(line, sizeof(line), f)) {
        sscanf(line, "MemTotal: %lu kB", &total);
        sscanf(line, "MemAvailable: %lu kB", &available);
    }
    fclose(f);
    
    unsigned long used = total - available;
    snprintf(buf, size, "%lu MB / %lu MB", used / 1024, total / 1024);
}

// Get storage info for root partition
static void get_storage_info(char *buf, size_t size) {
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) {
        snprintf(buf, size, "Unknown");
        return;
    }
    
    unsigned long long total = (unsigned long long)stat.f_blocks * stat.f_frsize;
    unsigned long long avail = (unsigned long long)stat.f_bavail * stat.f_frsize;
    
    double total_gb = total / (1024.0 * 1024.0 * 1024.0);
    double avail_gb = avail / (1024.0 * 1024.0 * 1024.0);
    
    snprintf(buf, size, "%.1fG / %.1fG", avail_gb, total_gb);
}

// Get network bandwidth (link speed)
static void get_network_bandwidth(char *buf, size_t size) {
    char speed_path[128];
    char state_path[128];
    char speed[32];
    char state[32];
    
    // Common interface names to check
    const char *interfaces[] = {
        "eth0", "eth1",
        "enp0s31f6", "enp0s25", "eno1", "eno2",
        "wlan0", "wlp0s20f3", "wlp2s0",
        NULL
    };
    
    for (int i = 0; interfaces[i] != NULL; i++) {
        snprintf(state_path, sizeof(state_path), 
                 "/sys/class/net/%s/operstate", interfaces[i]);
        
        if (read_file_line(state_path, state, sizeof(state)) == 0 && 
            strcmp(state, "up") == 0) {
            
            snprintf(speed_path, sizeof(speed_path), 
                     "/sys/class/net/%s/speed", interfaces[i]);
            
            if (read_file_line(speed_path, speed, sizeof(speed)) == 0) {
                int spd = atoi(speed);
                if (spd > 0) {
                    const char *type = (interfaces[i][0] == 'w') ? "Wi-Fi" : "Ethernet";
                    snprintf(buf, size, "%d Mbps (%s)", spd, type);
                    return;
                }
            }
        }
    }
    snprintf(buf, size, "Unknown");
}

// Get primary IP address
static void get_ip_address(char *buf, size_t size) {
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) {
        snprintf(buf, size, "Unknown");
        return;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;
        
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, buf, size);
        freeifaddrs(ifaddr);
        return;
    }
    
    freeifaddrs(ifaddr);
    snprintf(buf, size, "127.0.0.1");
}

// Get local time
static void get_local_time(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%B %d %Y, %I:%M:%S %p %Z", tm_info);
}

// Get OS info
static void get_os_info(char *buf, size_t size) {
    char pretty_name[256] = "";
    FILE *f = fopen("/etc/os-release", "r");
    
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                char *start = strchr(line, '"');
                if (start) {
                    start++;
                    char *end = strchr(start, '"');
                    if (end) *end = 0;
                    strncpy(pretty_name, start, sizeof(pretty_name) - 1);
                }
                break;
            }
        }
        fclose(f);
    }
    
    struct utsname uts;
    if (uname(&uts) == 0) {
        if (strlen(pretty_name) > 0) {
            snprintf(buf, size, "%s (%s)", pretty_name, uts.release);
        } else {
            snprintf(buf, size, "%s %s", uts.sysname, uts.release);
        }
    } else {
        snprintf(buf, size, "Unknown");
    }
}

// Get hostname
static void get_hostname(char *buf, size_t size) {
    if (gethostname(buf, size) != 0) {
        snprintf(buf, size, "unknown");
    }
}

// Lowercase a string in-place, unless it's all uppercase
static void lowercase_unless_allcaps(char *str) {
    int has_lower = 0;
    for (int i = 0; str[i]; i++) {
        if (islower((unsigned char)str[i])) {
            has_lower = 1;
            break;
        }
    }
    if (has_lower) {
        for (int i = 0; str[i]; i++) {
            str[i] = tolower((unsigned char)str[i]);
        }
    }
}

// Get uptime
static void get_uptime(char *buf, size_t size) {
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) {
        snprintf(buf, size, "Unknown");
        return;
    }
    
    double uptime_secs;
    if (fscanf(f, "%lf", &uptime_secs) != 1) {
        fclose(f);
        snprintf(buf, size, "Unknown");
        return;
    }
    fclose(f);
    
    int days = (int)(uptime_secs / 86400);
    int hours = (int)((uptime_secs - days * 86400) / 3600);
    int mins = (int)((uptime_secs - days * 86400 - hours * 3600) / 60);
    
    if (days > 0) {
        snprintf(buf, size, "%dd %dh %dm", days, hours, mins);
    } else if (hours > 0) {
        snprintf(buf, size, "%dh %dm", hours, mins);
    } else {
        snprintf(buf, size, "%dm", mins);
    }
}

int main(int argc, char *argv[]) {
    char cpu[MAX_BUF], memory[MAX_BUF], storage[MAX_BUF];
    char bandwidth[MAX_BUF], ip[MAX_BUF], local_time[MAX_BUF];
    char location[MAX_BUF], owner[MAX_BUF], os[MAX_BUF];
    char hostname[MAX_BUF], uptime[MAX_BUF];
    char support[MAX_BUF], docs[MAX_BUF];

    // CLI overrides (NULL = use config file)
    char *cli_owner = NULL;
    char *cli_location = NULL;
    char *cli_support = NULL;
    char *cli_docs = NULL;
    int hide_support = 0;
    int hide_ip = 0;

    // Parse command line options
    static struct option long_options[] = {
        {"owner",      required_argument, NULL, 'o'},
        {"location",   required_argument, NULL, 'l'},
        {"support",    required_argument, NULL, 's'},
        {"docs",       required_argument, NULL, 'd'},
        {"no-support", no_argument,       NULL, 'S'},
        {"hide-ip",    no_argument,       NULL, 'I'},
        {"noir",       no_argument,       NULL, 'n'},
        {"help",       no_argument,       NULL, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "o:l:s:d:SInh", long_options, NULL)) != -1) {
        switch (c) {
            case 'o': cli_owner = optarg; break;
            case 'l': cli_location = optarg; break;
            case 's': cli_support = optarg; break;
            case 'd': cli_docs = optarg; break;
            case 'S': hide_support = 1; break;
            case 'I': hide_ip = 1; break;
            case 'n': noir_mode = 1; break;
            case 'h':
                print_help();
                return 0;
            default:
                print_help();
                return 1;
        }
    }

    // Gather system info
    get_cpu_info(cpu, sizeof(cpu));
    get_memory_info(memory, sizeof(memory));
    get_storage_info(storage, sizeof(storage));
    get_network_bandwidth(bandwidth, sizeof(bandwidth));
    get_ip_address(ip, sizeof(ip));
    get_local_time(local_time, sizeof(local_time));
    get_os_info(os, sizeof(os));
    get_hostname(hostname, sizeof(hostname));
    lowercase_unless_allcaps(hostname);  // lowercase for welcome message, unless ALL CAPS
    get_uptime(uptime, sizeof(uptime));

    // Read config files, then apply CLI overrides
    read_config(CONFIG_LOCATION, location, sizeof(location), "");
    read_config(CONFIG_OWNER, owner, sizeof(owner), "");
    read_config(CONFIG_SUPPORT, support, sizeof(support), "");
    read_config(CONFIG_DOCS, docs, sizeof(docs), "");

    if (cli_owner) strncpy(owner, cli_owner, sizeof(owner) - 1);
    if (cli_location) strncpy(location, cli_location, sizeof(location) - 1);
    if (cli_support) strncpy(support, cli_support, sizeof(support) - 1);
    if (cli_docs) strncpy(docs, cli_docs, sizeof(docs) - 1);

    // Get terminal width for centering
    int term_width = get_term_width();

    // Display bonsai tree
    printf("\n");
    run_cbonsai();

    // Welcome message
    char welcome[256];
    if (owner[0]) {
        snprintf(welcome, sizeof(welcome), "welcome to %.100s - %.100s", hostname, owner);
    } else {
        snprintf(welcome, sizeof(welcome), "welcome to %.100s", hostname);
    }
    print_centered(term_width, welcome);
    printf("\n");

    // System info
    print_info(term_width, "OS", os);
    print_info(term_width, "UPTIME", uptime);
    print_info(term_width, "HARDWARE", cpu);
    print_info(term_width, "MEMORY", memory);
    print_info(term_width, "STORAGE", storage);
    print_info(term_width, "NETWORK BANDWIDTH", bandwidth);
    if (!hide_ip) print_info(term_width, "NODE IP", ip);
    if (location[0]) print_info(term_width, "LOCATION", location);
    print_info(term_width, "LOCAL TIME", local_time);
    printf("\n");

    // Support info (if not hidden and at least one is set)
    // Use clickable links for URLs and emails
    if (!hide_support && (support[0] || docs[0])) {
        if (support[0]) {
            if (looks_like_email(support))
                print_link(term_width, "SUPPORT", support, 1);
            else if (looks_like_url(support))
                print_link(term_width, "SUPPORT", support, 0);
            else
                print_info(term_width, "SUPPORT", support);
        }
        if (docs[0]) {
            if (looks_like_email(docs))
                print_link(term_width, "DOCS", docs, 1);
            else if (looks_like_url(docs))
                print_link(term_width, "DOCS", docs, 0);
            else
                print_info(term_width, "DOCS", docs);
        }
        printf("\n");
    }

    return 0;
}