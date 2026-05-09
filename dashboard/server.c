/*
 * SmartHeap - Web Dashboard Server
 * dashboard/server.c
 *
 * A minimal HTTP server using Winsock2 that serves the web dashboard
 * and exposes JSON API endpoints for controlling SmartHeap.
 *
 * Endpoints:
 *   GET  /              → Dashboard HTML
 *   GET  /api/state     → Full heap state JSON
 *   POST /api/allocate  → Allocate memory (body: {"size":N})
 *   POST /api/free      → Free block (body: {"id":N})
 *   POST /api/calloc    → Calloc (body: {"num":N,"size":M})
 *   POST /api/realloc   → Realloc (body: {"id":N,"size":M})
 *   POST /api/strategy  → Set strategy (body: {"strategy":"best_fit"})
 *   POST /api/reset     → Reset allocator
 *   GET  /api/leaks     → Leak check
 *   POST /api/run-tests     → Run test suite, return output
 *   POST /api/run-demo      → Run basic demo, return output
 *   POST /api/run-benchmark → Run benchmark, return output
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "smartheap.h"

#pragma comment(lib, "ws2_32.lib")

#define PORT         8080
#define BUFFER_SIZE  65536
#define JSON_BUF     (256 * 1024)
#define PROC_BUF     (512 * 1024)

/* ── Tracked Allocations ─────────────────────────────────────── */

#define MAX_ALLOCS 4096

typedef struct {
    void  *ptr;
    size_t size;
    int    id;
    int    active;
} tracked_t;

static tracked_t g_allocs[MAX_ALLOCS];
static int g_next_id = 1;

static int track_alloc(void *ptr, size_t sz)
{
    for (int i = 0; i < MAX_ALLOCS; i++) {
        if (!g_allocs[i].active) {
            g_allocs[i].ptr = ptr;
            g_allocs[i].size = sz;
            g_allocs[i].id = g_next_id++;
            g_allocs[i].active = 1;
            return g_allocs[i].id;
        }
    }
    return -1;
}

static tracked_t *find_alloc(int id)
{
    for (int i = 0; i < MAX_ALLOCS; i++)
        if (g_allocs[i].active && g_allocs[i].id == id)
            return &g_allocs[i];
    return NULL;
}

/* ── HTTP Helpers ────────────────────────────────────────────── */

static void send_response(SOCKET client, int status, const char *ct,
                          const char *body, int body_len)
{
    const char *status_text = "OK";
    if (status == 404) status_text = "Not Found";
    if (status == 400) status_text = "Bad Request";
    if (status == 500) status_text = "Internal Server Error";

    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, ct, body_len);

    send(client, header, hlen, 0);
    if (body_len > 0)
        send(client, body, body_len, 0);
}

static void send_json(SOCKET client, const char *json)
{
    send_response(client, 200, "application/json", json, (int)strlen(json));
}

static void send_json_error(SOCKET client, int status, const char *msg)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", msg);
    send_response(client, status, "application/json", buf, (int)strlen(buf));
}

/* ── Parse Helpers ───────────────────────────────────────────── */

static int parse_int_field(const char *json, const char *field)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", field);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p == ' ') p++;
    return atoi(p);
}

static void parse_string_field(const char *json, const char *field,
                               char *out, int outsize)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", field);
    const char *p = strstr(json, pattern);
    if (!p) { out[0] = 0; return; }
    p += strlen(pattern);
    int i = 0;
    while (*p && *p != '"' && i < outsize - 1)
        out[i++] = *p++;
    out[i] = 0;
}

/* ── Load Dashboard HTML ─────────────────────────────────────── */

static char *g_dashboard_html = NULL;
static int   g_dashboard_len = 0;

static int load_dashboard(void)
{
    /* Try multiple paths to find index.html */
    const char *paths[] = {
        "dashboard/index.html",
        "../dashboard/index.html",
        "index.html",
        NULL
    };

    FILE *f = NULL;
    for (int i = 0; paths[i]; i++) {
        f = fopen(paths[i], "rb");
        if (f) break;
    }

    if (!f) {
        fprintf(stderr, "[Dashboard] ERROR: Cannot find index.html\n");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    g_dashboard_len = (int)ftell(f);
    fseek(f, 0, SEEK_SET);

    g_dashboard_html = (char *)malloc(g_dashboard_len + 1);
    fread(g_dashboard_html, 1, g_dashboard_len, f);
    g_dashboard_html[g_dashboard_len] = 0;
    fclose(f);
    return 0;
}

/* ── API Handlers ────────────────────────────────────────────── */

static void handle_state(SOCKET client)
{
    /* Build JSON including tracked allocations */
    char *buf = (char *)malloc(JSON_BUF);
    int pos = sh_state_json(buf, JSON_BUF - 4096);

    /* Append tracked allocations list */
    /* Remove trailing "}" and append allocations */
    if (pos > 0 && buf[pos - 1] == '}') {
        pos--;  /* overwrite final } */
        pos += snprintf(buf + pos, JSON_BUF - pos, ",\"allocations\":[");

        int first = 1;
        for (int i = 0; i < MAX_ALLOCS; i++) {
            if (g_allocs[i].active) {
                if (!first) pos += snprintf(buf + pos, JSON_BUF - pos, ",");
                pos += snprintf(buf + pos, JSON_BUF - pos,
                    "{\"id\":%d,\"size\":%zu}",
                    g_allocs[i].id, g_allocs[i].size);
                first = 0;
            }
        }
        pos += snprintf(buf + pos, JSON_BUF - pos, "]}");
    }

    send_json(client, buf);
    free(buf);
}

static void handle_allocate(SOCKET client, const char *body)
{
    int size = parse_int_field(body, "size");
    if (size <= 0) {
        send_json_error(client, 400, "Invalid size");
        return;
    }

    void *ptr = sh_malloc((size_t)size);
    if (!ptr) {
        send_json_error(client, 500, "Allocation failed");
        return;
    }

    /* Write pattern to allocated memory */
    memset(ptr, 0xAA, (size_t)size);

    int id = track_alloc(ptr, (size_t)size);

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"ok\":true,\"id\":%d,\"size\":%d}", id, size);
    send_json(client, resp);
}

static void handle_free(SOCKET client, const char *body)
{
    int id = parse_int_field(body, "id");
    tracked_t *t = find_alloc(id);
    if (!t) {
        send_json_error(client, 404, "Block not found");
        return;
    }

    sh_free(t->ptr);
    t->active = 0;

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"id\":%d}", id);
    send_json(client, resp);
}

static void handle_calloc(SOCKET client, const char *body)
{
    int num  = parse_int_field(body, "num");
    int size = parse_int_field(body, "size");
    if (num <= 0 || size <= 0) {
        send_json_error(client, 400, "Invalid parameters");
        return;
    }

    void *ptr = sh_calloc((size_t)num, (size_t)size);
    if (!ptr) {
        send_json_error(client, 500, "Calloc failed");
        return;
    }

    int id = track_alloc(ptr, (size_t)(num * size));

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"ok\":true,\"id\":%d,\"size\":%d}", id, num * size);
    send_json(client, resp);
}

static void handle_realloc(SOCKET client, const char *body)
{
    int id   = parse_int_field(body, "id");
    int size = parse_int_field(body, "size");
    tracked_t *t = find_alloc(id);
    if (!t) {
        send_json_error(client, 404, "Block not found");
        return;
    }
    if (size <= 0) {
        send_json_error(client, 400, "Invalid size");
        return;
    }

    void *ptr = sh_realloc(t->ptr, (size_t)size);
    if (!ptr) {
        send_json_error(client, 500, "Realloc failed");
        return;
    }

    t->ptr = ptr;
    t->size = (size_t)size;

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"ok\":true,\"id\":%d,\"size\":%d}", id, size);
    send_json(client, resp);
}

static void handle_strategy(SOCKET client, const char *body)
{
    char strat[32];
    parse_string_field(body, "strategy", strat, sizeof(strat));

    if (!strcmp(strat, "first_fit"))
        sh_set_strategy(SH_STRATEGY_FIRST_FIT);
    else if (!strcmp(strat, "best_fit"))
        sh_set_strategy(SH_STRATEGY_BEST_FIT);
    else if (!strcmp(strat, "worst_fit"))
        sh_set_strategy(SH_STRATEGY_WORST_FIT);
    else {
        send_json_error(client, 400, "Unknown strategy");
        return;
    }

    char resp[128];
    snprintf(resp, sizeof(resp),
        "{\"ok\":true,\"strategy\":\"%s\"}", sh_get_strategy_name());
    send_json(client, resp);
}

static void handle_reset(SOCKET client)
{
    /* Free all tracked allocations */
    for (int i = 0; i < MAX_ALLOCS; i++) {
        if (g_allocs[i].active) {
            sh_free(g_allocs[i].ptr);
            g_allocs[i].active = 0;
        }
    }

    sh_destroy();
    sh_init();
    g_next_id = 1;

    send_json(client, "{\"ok\":true}");
}

static void handle_leaks(SOCKET client)
{
    size_t count = 0;

    /* Count leaks without printing to stderr */
    /* We just count blocks that are active in our tracker vs the allocator */
    int active = 0;
    for (int i = 0; i < MAX_ALLOCS; i++)
        if (g_allocs[i].active) active++;

    count = (size_t)active;

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"leaks\":%zu,\"message\":\"%s\"}",
        count,
        count > 0 ? "Memory leaks detected" : "No leaks - all clean!");
    send_json(client, resp);
}

/* ── Process Runner ──────────────────────────────────────────── */

/* Strip ANSI escape codes from a string in-place */
static void strip_ansi(char *s)
{
    char *r = s, *w = s;
    while (*r) {
        if (*r == '\033' || *r == '\x1b') {
            r++; /* skip ESC */
            if (*r == '[') {
                r++; /* skip [ */
                while (*r && !((*r >= 'A' && *r <= 'Z') || (*r >= 'a' && *r <= 'z')))
                    r++;
                if (*r) r++; /* skip final letter */
            }
        } else {
            *w++ = *r++;
        }
    }
    *w = 0;
}

/* Escape a string for JSON (handles \n, \r, \t, \", \\) */
static int json_escape(const char *src, char *dst, int dstsize)
{
    int i = 0;
    while (*src && i < dstsize - 2) {
        switch (*src) {
        case '"':  dst[i++]='\\'; dst[i++]='"'; break;
        case '\\': dst[i++]='\\'; dst[i++]='\\'; break;
        case '\n': dst[i++]='\\'; dst[i++]='n'; break;
        case '\r': dst[i++]='\\'; dst[i++]='r'; break;
        case '\t': dst[i++]='\\'; dst[i++]='t'; break;
        default:
            if ((unsigned char)*src >= 0x20)
                dst[i++] = *src;
            break;
        }
        src++;
    }
    dst[i] = 0;
    return i;
}

/* Run a child process and capture its stdout+stderr */
static char *run_process(const char *cmd, DWORD *exit_code)
{
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hReadOut, hWriteOut;
    if (!CreatePipe(&hReadOut, &hWriteOut, &sa, 0)) return NULL;
    SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWriteOut;
    si.hStdError  = hWriteOut;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    /* Need mutable copy for CreateProcessA */
    char cmdline[512];
    strncpy(cmdline, cmd, sizeof(cmdline) - 1);
    cmdline[sizeof(cmdline) - 1] = 0;

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadOut);
        CloseHandle(hWriteOut);
        return NULL;
    }

    CloseHandle(hWriteOut); /* Close write end so ReadFile knows when done */

    /* Read all output */
    char *output = (char *)malloc(PROC_BUF);
    DWORD total = 0, n;
    while (total < PROC_BUF - 1) {
        BOOL ok = ReadFile(hReadOut, output + total, PROC_BUF - 1 - total, &n, NULL);
        if (!ok || n == 0) break;
        total += n;
    }
    output[total] = 0;

    WaitForSingleObject(pi.hProcess, 15000);
    GetExitCodeProcess(pi.hProcess, exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadOut);

    return output;
}

static void handle_run_exe(SOCKET client, const char *exe_name, const char *label)
{
    /* Try multiple paths */
    char cmd[512];
    const char *prefixes[] = { "build\\", ".\\build\\", "..\\build\\", NULL };
    char *output = NULL;
    DWORD exit_code = 1;

    for (int i = 0; prefixes[i]; i++) {
        snprintf(cmd, sizeof(cmd), "%s%s", prefixes[i], exe_name);
        output = run_process(cmd, &exit_code);
        if (output) break;
    }

    if (!output) {
        send_json_error(client, 500, "Failed to run executable");
        return;
    }

    /* Strip ANSI codes for clean web display */
    strip_ansi(output);

    /* Parse test results if this is test_runner */
    int total_tests = 0, passed = 0, failed = 0;
    if (strstr(label, "Test")) {
        /* Search for "N tests" pattern anywhere in output (robust) */
        const char *p = output;
        const char *last_match = NULL;
        while ((p = strstr(p, "tests")) != NULL) {
            last_match = p;
            p++;
        }
        /* Work backwards from "tests" to find the number */
        if (last_match) {
            const char *np = last_match - 1;
            while (np > output && *np == ' ') np--;
            while (np > output && np[-1] >= '0' && np[-1] <= '9') np--;
            if (np >= output) {
                sscanf(np, "%d tests | %d passed | %d failed",
                       &total_tests, &passed, &failed);
            }
        }
    }

    /* Build JSON response */
    int resp_size = PROC_BUF * 2 + 512;
    char *resp = (char *)malloc(resp_size);
    char *escaped = (char *)malloc(PROC_BUF * 2);
    json_escape(output, escaped, PROC_BUF * 2);

    int pos = snprintf(resp, resp_size,
        "{\"ok\":true,\"label\":\"%s\",\"exit_code\":%lu,"
        "\"total_tests\":%d,\"passed\":%d,\"failed\":%d,"
        "\"output\":\"%s\"}",
        label, exit_code, total_tests, passed, failed, escaped);

    send_response(client, 200, "application/json", resp, pos);

    free(escaped);
    free(resp);
    free(output);
}

/* ── Request Router ──────────────────────────────────────────── */

static void handle_request(SOCKET client, const char *method,
                           const char *path, const char *body)
{
    /* OPTIONS preflight */
    if (!strcmp(method, "OPTIONS")) {
        send_response(client, 200, "text/plain", "", 0);
        return;
    }

    /* GET / — Dashboard */
    if (!strcmp(method, "GET") && !strcmp(path, "/")) {
        if (g_dashboard_html)
            send_response(client, 200, "text/html; charset=utf-8",
                          g_dashboard_html, g_dashboard_len);
        else
            send_json_error(client, 500, "Dashboard not loaded");
        return;
    }

    /* API routes */
    if (!strcmp(method, "GET") && !strcmp(path, "/api/state")) {
        handle_state(client);
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/allocate")) {
        handle_allocate(client, body);
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/free")) {
        handle_free(client, body);
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/calloc")) {
        handle_calloc(client, body);
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/realloc")) {
        handle_realloc(client, body);
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/strategy")) {
        handle_strategy(client, body);
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/reset")) {
        handle_reset(client);
    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/leaks")) {
        handle_leaks(client);
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/run-tests")) {
        handle_run_exe(client, "test_runner.exe", "Test Suite");
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/run-demo")) {
        handle_run_exe(client, "demo_basic.exe", "Basic Demo");
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/run-benchmark")) {
        handle_run_exe(client, "demo_benchmark.exe", "Benchmark");
    } else {
        send_json_error(client, 404, "Not found");
    }
}

/* ── Main ────────────────────────────────────────────────────── */

int main(void)
{
    WSADATA wsa;
    SOCKET server_fd, client_fd;
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);

    /* Enable UTF-8 console output */
    SetConsoleOutputCP(65001);

    printf("\n");
    printf("  ======================================\n");
    printf("   SmartHeap Web Dashboard Server\n");
    printf("  ======================================\n\n");

    /* Initialize Winsock */
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "  [ERROR] WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    /* Load dashboard HTML */
    if (load_dashboard() != 0) {
        fprintf(stderr, "  [ERROR] Failed to load dashboard HTML\n");
        WSACleanup();
        return 1;
    }
    printf("  [OK] Dashboard HTML loaded (%d bytes)\n", g_dashboard_len);

    /* Initialize SmartHeap */
    sh_init();
    printf("  [OK] SmartHeap initialized\n");

    /* Create socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        fprintf(stderr, "  [ERROR] socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    /* Allow address reuse */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    /* Bind */
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "  [ERROR] bind() failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    /* Listen */
    if (listen(server_fd, 10) == SOCKET_ERROR) {
        fprintf(stderr, "  [ERROR] listen() failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    printf("  [OK] Listening on http://localhost:%d\n\n", PORT);
    printf("  Open your browser to: http://localhost:%d\n", PORT);
    printf("  Press Ctrl+C to stop.\n\n");

    /* Accept loop */
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
        if (client_fd == INVALID_SOCKET) continue;

        /* Read request */
        char *reqbuf = (char *)malloc(BUFFER_SIZE);
        int total = 0;
        int n;

        /* Read headers first */
        while (total < BUFFER_SIZE - 1) {
            n = recv(client_fd, reqbuf + total, BUFFER_SIZE - 1 - total, 0);
            if (n <= 0) break;
            total += n;
            reqbuf[total] = 0;
            /* Check if we got the end of headers */
            if (strstr(reqbuf, "\r\n\r\n")) break;
        }

        if (total <= 0) {
            free(reqbuf);
            closesocket(client_fd);
            continue;
        }

        /* Parse method and path */
        char method[16] = {0};
        char path[256] = {0};
        sscanf(reqbuf, "%15s %255s", method, path);

        /* Find body (after \r\n\r\n) */
        char *body = strstr(reqbuf, "\r\n\r\n");
        if (body) body += 4; else body = "";

        /* If POST and Content-Length exists, try to read remaining body */
        if (!strcmp(method, "POST")) {
            char *cl = strstr(reqbuf, "Content-Length:");
            if (!cl) cl = strstr(reqbuf, "content-length:");
            if (cl) {
                int content_len = atoi(cl + 15);
                int body_start = (int)(body - reqbuf);
                int body_read = total - body_start;
                while (body_read < content_len && total < BUFFER_SIZE - 1) {
                    n = recv(client_fd, reqbuf + total, BUFFER_SIZE - 1 - total, 0);
                    if (n <= 0) break;
                    total += n;
                    reqbuf[total] = 0;
                    body_read = total - body_start;
                }
            }
        }

        /* Route the request */
        handle_request(client_fd, method, path, body);

        /* Log */
        printf("  %s %s\n", method, path);

        free(reqbuf);
        closesocket(client_fd);
    }

    /* Cleanup */
    closesocket(server_fd);
    sh_destroy();
    if (g_dashboard_html) free(g_dashboard_html);
    WSACleanup();
    return 0;
}
