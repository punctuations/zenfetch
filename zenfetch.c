/*
 * zenfetch - A system info display tool using cbonsai
 *
 * This file is designed to be part of the cbonsai repository.
 * It calls cbonsai directly to display a bonsai tree, then shows system info.
 *
 * Cross-platform support: Linux and Windows
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "ws2_32.lib")

    // Interface type constants (in case SDK is missing them)
    #ifndef IF_TYPE_ETHERNET_CSMACD
        #define IF_TYPE_ETHERNET_CSMACD 6
    #endif
    #ifndef IF_TYPE_PPP
        #define IF_TYPE_PPP 23
    #endif
    #ifndef IF_TYPE_IEEE80211
        #define IF_TYPE_IEEE80211 71
    #endif
    #ifndef IF_TYPE_GIGABITETHERNET
        #define IF_TYPE_GIGABITETHERNET 117
    #endif
    #ifndef IF_TYPE_TUNNEL
        #define IF_TYPE_TUNNEL 131
    #endif
    #ifndef IF_TYPE_WWANPP
        #define IF_TYPE_WWANPP 243
    #endif
    #ifndef IF_TYPE_WWANPP2
        #define IF_TYPE_WWANPP2 244
    #endif
#else
    #include <unistd.h>
    #include <getopt.h>
    #include <sys/utsname.h>
    #include <sys/statvfs.h>
    #include <sys/ioctl.h>
    #include <ifaddrs.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

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

// Global print mode flag (no animation, instant display)
static int print_mode = 0;

// Configuration file paths
#ifdef _WIN32
    #define CONFIG_LOCATION "C:\\ProgramData\\zenfetch\\location"
    #define CONFIG_OWNER    "C:\\ProgramData\\zenfetch\\owner"
    #define CONFIG_SUPPORT  "C:\\ProgramData\\zenfetch\\support"
    #define CONFIG_DOCS     "C:\\ProgramData\\zenfetch\\docs"
#else
    #define CONFIG_LOCATION "/etc/zenfetch/location"
    #define CONFIG_OWNER    "/etc/zenfetch/owner"
    #define CONFIG_SUPPORT  "/etc/zenfetch/support"
    #define CONFIG_DOCS     "/etc/zenfetch/docs"
#endif

static void print_help(void) {
    printf(
        "Usage: zenfetch [OPTIONS]\n"
        "\n"
        "Display system info with a bonsai tree.\n"
        "\n"
        "Options:\n"
        "  -o, --owner TEXT      set owner name in welcome message\n"
        "  -L, --location TEXT   set location\n"
        "  -s, --support TEXT    set support contact info\n"
        "  -d, --docs URL        set documentation URL\n"
        "  -S, --no-support      hide support/docs section\n"
        "  -I, --hide-ip         hide NODE IP field\n"
        "  -n, --noir            noir mode: no colors, bold labels\n"
        "  -p, --print           print mode: no animation, instant display\n"
        "  -h, --help            show this help\n"
        "\n"
#ifdef _WIN32
        "Config files (one value per line, CLI overrides these):\n"
        "  C:\\ProgramData\\zenfetch\\owner\n"
        "  C:\\ProgramData\\zenfetch\\location\n"
        "  C:\\ProgramData\\zenfetch\\support\n"
        "  C:\\ProgramData\\zenfetch\\docs\n"
#else
        "Config files (one value per line, CLI overrides these):\n"
        "  /etc/zenfetch/owner\n"
        "  /etc/zenfetch/location\n"
        "  /etc/zenfetch/support\n"
        "  /etc/zenfetch/docs\n"
#endif
    );
}

/*
 * Run cbonsai to display the tree
 */
static void run_cbonsai(void) {
#ifndef _WIN32
    // Reset getopt for cbonsai's argument parsing
    optind = 1;
#endif

    if (print_mode) {
        // Print mode: no animation (no -l -t flags)
        if (noir_mode) {
            char *args[] = {
                "cbonsai",
                "-n",           // noir mode
                "-b", "3",
                "-p",           // print mode
                NULL
            };
            cbonsai_run(5, args);
        } else {
            char *args[] = {
                "cbonsai",
                "-b", "3",
                "-p",           // print mode
                NULL
            };
            cbonsai_run(4, args);
        }
    } else {
        // Normal mode: with animation
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
}

// Get terminal width
static int get_term_width(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        return 80;
    }
    return w.ws_col;
#endif
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
    if (fgets(buf, (int)size, f) == NULL) {
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
#ifdef _WIN32
    HKEY hKey;
    char cpu_name[256] = "Unknown";
    DWORD buf_size = sizeof(cpu_name);

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL,
                        (LPBYTE)cpu_name, &buf_size);
        RegCloseKey(hKey);
    }

    // Get number of processors
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    int cores = sysInfo.dwNumberOfProcessors;

    // Trim leading spaces from CPU name
    char *trimmed = cpu_name;
    while (*trimmed == ' ') trimmed++;

    snprintf(buf, size, "%s %d core", trimmed, cores);
#else
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
#endif
}

// Get memory info
static void get_memory_info(char *buf, size_t size) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memInfo)) {
        unsigned long long total = memInfo.ullTotalPhys / (1024 * 1024);
        unsigned long long avail = memInfo.ullAvailPhys / (1024 * 1024);
        unsigned long long used = total - avail;
        snprintf(buf, size, "%llu MB / %llu MB", used, total);
    } else {
        snprintf(buf, size, "Unknown");
    }
#else
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
#endif
}

// Get storage info for root partition
static void get_storage_info(char *buf, size_t size) {
#ifdef _WIN32
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;

    if (GetDiskFreeSpaceExA("C:\\", &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        double total_gb = (double)totalBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
        double avail_gb = (double)freeBytesAvailable.QuadPart / (1024.0 * 1024.0 * 1024.0);
        snprintf(buf, size, "%.1fG / %.1fG", avail_gb, total_gb);
    } else {
        snprintf(buf, size, "Unknown");
    }
#else
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
#endif
}

// Get network bandwidth (link speed)
static void get_network_bandwidth(char *buf, size_t size) {
#ifdef _WIN32
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;

    pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
    if (pAddresses == NULL) {
        snprintf(buf, size, "Unknown");
        return;
    }

    DWORD dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL,
                                          pAddresses, &outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
        if (pAddresses == NULL) {
            snprintf(buf, size, "Unknown");
            return;
        }
        dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL,
                                        pAddresses, &outBufLen);
    }

    if (dwRetVal == NO_ERROR) {
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            if (pCurrAddresses->OperStatus == IfOperStatusUp &&
                pCurrAddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {

                ULONG64 speed = pCurrAddresses->TransmitLinkSpeed;
                if (speed > 0 && speed != (ULONG64)-1) {
                    const char *type;
                    switch (pCurrAddresses->IfType) {
                        case IF_TYPE_ETHERNET_CSMACD:  // 6 - Standard Ethernet
                        case IF_TYPE_GIGABITETHERNET:  // 117 - Gigabit Ethernet
                            type = "Ethernet";
                            break;
                        case IF_TYPE_IEEE80211:        // 71 - Wi-Fi
                            type = "Wi-Fi";
                            break;
                        case IF_TYPE_PPP:              // 23 - PPP (dial-up, VPN)
                            type = "PPP";
                            break;
                        case IF_TYPE_TUNNEL:           // 131 - Tunnel (VPN)
                            type = "Tunnel";
                            break;
                        case IF_TYPE_WWANPP:           // 243 - Mobile broadband (GSM)
                        case IF_TYPE_WWANPP2:          // 244 - Mobile broadband (CDMA)
                            type = "Mobile";
                            break;
                        default:
                            type = "Network";
                            break;
                    }
                    snprintf(buf, size, "%llu Mbps (%s)",
                             (unsigned long long)(speed / 1000000), type);
                    free(pAddresses);
                    return;
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    }

    free(pAddresses);
    snprintf(buf, size, "Unknown");
#else
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
#endif
}

// Get primary IP address
static void get_ip_address(char *buf, size_t size) {
#ifdef _WIN32
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;

    pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
    if (pAddresses == NULL) {
        snprintf(buf, size, "Unknown");
        return;
    }

    DWORD dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL,
                                          pAddresses, &outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
        if (pAddresses == NULL) {
            snprintf(buf, size, "Unknown");
            return;
        }
        dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL,
                                        pAddresses, &outBufLen);
    }

    if (dwRetVal == NO_ERROR) {
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            if (pCurrAddresses->OperStatus == IfOperStatusUp &&
                pCurrAddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {

                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                if (pUnicast != NULL) {
                    struct sockaddr_in *sa_in = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                    inet_ntop(AF_INET, &(sa_in->sin_addr), buf, (socklen_t)size);
                    free(pAddresses);
                    return;
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    }

    free(pAddresses);
    snprintf(buf, size, "127.0.0.1");
#else
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
        inet_ntop(AF_INET, &addr->sin_addr, buf, (socklen_t)size);
        freeifaddrs(ifaddr);
        return;
    }

    freeifaddrs(ifaddr);
    snprintf(buf, size, "127.0.0.1");
#endif
}

// Get local time
static void get_local_time(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
#ifdef _WIN32
    // Windows doesn't have %Z that works reliably, use TIME_ZONE_INFORMATION instead
    TIME_ZONE_INFORMATION tz;
    GetTimeZoneInformation(&tz);
    char tz_name[64];
    wcstombs(tz_name, tz.StandardName, sizeof(tz_name));

    char time_part[128];
    strftime(time_part, sizeof(time_part), "%B %d %Y, %I:%M:%S %p", tm_info);
    snprintf(buf, size, "%s %s", time_part, tz_name);
#else
    strftime(buf, size, "%B %d %Y, %I:%M:%S %p %Z", tm_info);
#endif
}

// Get OS info
static void get_os_info(char *buf, size_t size) {
#ifdef _WIN32
    // Use RtlGetVersion via ntdll for accurate version info
    typedef LONG (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW osvi = {0};
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll) {
        RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
        if (pRtlGetVersion) {
            pRtlGetVersion(&osvi);
        }
    }

    const char *win_name = "Windows";
    if (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 22000) {
        win_name = "Windows 11";
    } else if (osvi.dwMajorVersion == 10) {
        win_name = "Windows 10";
    } else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 3) {
        win_name = "Windows 8.1";
    } else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 2) {
        win_name = "Windows 8";
    } else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1) {
        win_name = "Windows 7";
    }

    snprintf(buf, size, "%s (Build %lu)", win_name, osvi.dwBuildNumber);
#else
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
#endif
}

// Get hostname
static void get_hostname(char *buf, size_t size) {
#ifdef _WIN32
    DWORD buf_size = (DWORD)size;
    if (!GetComputerNameA(buf, &buf_size)) {
        snprintf(buf, size, "unknown");
    }
#else
    if (gethostname(buf, size) != 0) {
        snprintf(buf, size, "unknown");
    }
#endif
}

// Lowercase a string in-place
static void lowercase(char *str) {
    // int has_lower = 0;
    // for (int i = 0; str[i]; i++) {
    //     if (islower((unsigned char)str[i])) {
    //         has_lower = 1;
    //         break;
    //     }
    // }
    // if (has_lower) {
        for (int i = 0; str[i]; i++) {
            str[i] = tolower((unsigned char)str[i]);
        }
    // }
}

// Get uptime
static void get_uptime(char *buf, size_t size) {
#ifdef _WIN32
    ULONGLONG uptime_ms = GetTickCount64();
    double uptime_secs = (double)uptime_ms / 1000.0;

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
#else
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
#endif
}

#ifdef _WIN32
// Simple argument parsing for Windows (no getopt_long)
static int parse_args(int argc, char *argv[],
                      char **cli_owner, char **cli_location,
                      char **cli_support, char **cli_docs,
                      int *hide_support, int *hide_ip) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 1;
        }
        else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--noir") == 0) {
            noir_mode = 1;
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--print") == 0) {
            print_mode = 1;
        }
        else if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--no-support") == 0) {
            *hide_support = 1;
        }
        else if (strcmp(argv[i], "-I") == 0 || strcmp(argv[i], "--hide-ip") == 0) {
            *hide_ip = 1;
        }
        else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--owner") == 0) && i + 1 < argc) {
            *cli_owner = argv[++i];
        }
        else if ((strcmp(argv[i], "-L") == 0 || strcmp(argv[i], "--location") == 0) && i + 1 < argc) {
            *cli_location = argv[++i];
        }
        else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--support") == 0) && i + 1 < argc) {
            *cli_support = argv[++i];
        }
        else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--docs") == 0) && i + 1 < argc) {
            *cli_docs = argv[++i];
        }
    }
    return 0;
}
#endif

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

#ifdef _WIN32
    // Enable ANSI escape sequences on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    // Parse arguments
    if (parse_args(argc, argv, &cli_owner, &cli_location, &cli_support, &cli_docs,
                   &hide_support, &hide_ip)) {
        return 0;  // Help was shown
    }
#else
    // Parse command line options
    static struct option long_options[] = {
        {"owner",      required_argument, NULL, 'o'},
        {"location",   required_argument, NULL, 'L'},
        {"support",    required_argument, NULL, 's'},
        {"docs",       required_argument, NULL, 'd'},
        {"no-support", no_argument,       NULL, 'S'},
        {"hide-ip",    no_argument,       NULL, 'I'},
        {"noir",       no_argument,       NULL, 'n'},
        {"print",      no_argument,       NULL, 'p'},
        {"help",       no_argument,       NULL, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "o:L:s:d:SInph", long_options, NULL)) != -1) {
        switch (c) {
            case 'o': cli_owner = optarg; break;
            case 'L': cli_location = optarg; break;
            case 's': cli_support = optarg; break;
            case 'd': cli_docs = optarg; break;
            case 'S': hide_support = 1; break;
            case 'I': hide_ip = 1; break;
            case 'n': noir_mode = 1; break;
            case 'p': print_mode = 1; break;
            case 'h':
                print_help();
                return 0;
            default:
                print_help();
                return 1;
        }
    }
#endif

    // Gather system info
    get_cpu_info(cpu, sizeof(cpu));
    get_memory_info(memory, sizeof(memory));
    get_storage_info(storage, sizeof(storage));
    get_network_bandwidth(bandwidth, sizeof(bandwidth));
    get_ip_address(ip, sizeof(ip));
    get_local_time(local_time, sizeof(local_time));
    get_os_info(os, sizeof(os));
    get_hostname(hostname, sizeof(hostname));
    lowercase(hostname);  // lowercase for welcome message
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
