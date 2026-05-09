/*
 * SmartHeap Interactive CLI
 * demo_interactive.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#define SH_TRACK_LEAKS
#include "smartheap.h"

#define C_RESET "\033[0m"
#define C_BOLD  "\033[1m"
#define C_DIM   "\033[2m"
#define C_RED   "\033[91m"
#define C_GREEN "\033[92m"
#define C_YELLOW "\033[93m"
#define C_CYAN  "\033[96m"
#define C_WHITE "\033[97m"

#define MAX_BLOCKS 1024
typedef struct { void *ptr; size_t size; int id; int active; } tracked_t;
static tracked_t blocks[MAX_BLOCKS];
static int next_id = 1;

static int track(void *p, size_t s) {
    for (int i=0;i<MAX_BLOCKS;i++)
        if (!blocks[i].active) { blocks[i]=(tracked_t){p,s,next_id++,1}; return blocks[i].id-1+1; }
    return -1;
}
static tracked_t *find(int id) {
    for (int i=0;i<MAX_BLOCKS;i++) if (blocks[i].active && blocks[i].id==id) return &blocks[i];
    return NULL;
}
static void trim(char *s) {
    char *e=s+strlen(s)-1; while(e>s&&isspace((unsigned char)*e))*e--=0;
    char *b=s; while(*b&&isspace((unsigned char)*b))b++; if(b!=s)memmove(s,b,strlen(b)+1);
}

int main(void) {
    char line[256];
    SetConsoleOutputCP(65001);
    HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE); DWORD m=0;
    GetConsoleMode(h,&m); SetConsoleMode(h,m|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    memset(blocks,0,sizeof(blocks));
    printf("\n%s%s  SmartHeap Interactive CLI%s\n",C_BOLD,C_CYAN,C_RESET);
    printf("  Type 'help' for commands\n\n");
    sh_init();

    while(1) {
        printf("%s%sSmartHeap>%s ",C_BOLD,C_CYAN,C_RESET); fflush(stdout);
        if(!fgets(line,sizeof(line),stdin)) break;
        trim(line); if(!*line) continue;

        if(!strncmp(line,"allocate ",9)||!strncmp(line,"alloc ",6)) {
            size_t sz=(size_t)atol(strchr(line,' ')+1);
            if(!sz){printf("  %sInvalid size%s\n",C_RED,C_RESET);continue;}
            void *p=sh_malloc(sz);
            if(p){int id=track(p,sz);printf("  %s✓%s %zu bytes → Block #%d (%p)\n",C_GREEN,C_RESET,sz,id,p);}
            else printf("  %s✗ Failed%s\n",C_RED,C_RESET);
        } else if(!strncmp(line,"free ",5)) {
            tracked_t *b=find(atoi(line+5));
            if(b){sh_free(b->ptr);printf("  %s✓%s Freed #%d\n",C_GREEN,C_RESET,b->id);b->active=0;}
            else printf("  %s✗ Not found%s\n",C_RED,C_RESET);
        } else if(!strncmp(line,"calloc ",7)) {
            size_t n,s; if(sscanf(line+7,"%zu %zu",&n,&s)==2){
                void *p=sh_calloc(n,s);
                if(p){int id=track(p,n*s);printf("  %s✓%s %zu×%zu=%zu bytes → #%d\n",C_GREEN,C_RESET,n,s,n*s,id);}
                else printf("  %s✗ Failed%s\n",C_RED,C_RESET);
            } else printf("  Usage: calloc <n> <size>\n");
        } else if(!strncmp(line,"realloc ",8)) {
            int id; size_t s; if(sscanf(line+8,"%d %zu",&id,&s)==2){
                tracked_t *b=find(id);
                if(b){void *p=sh_realloc(b->ptr,s);if(p){b->ptr=p;printf("  %s✓%s #%d → %zu bytes\n",C_GREEN,C_RESET,id,s);b->size=s;}
                else printf("  %s✗ Failed%s\n",C_RED,C_RESET);}
                else printf("  %s✗ Not found%s\n",C_RED,C_RESET);
            } else printf("  Usage: realloc <id> <size>\n");
        } else if(!strcmp(line,"visualize")||!strcmp(line,"vis")||!strcmp(line,"map")) {
            sh_visualize();
        } else if(!strcmp(line,"stats")) {
            sh_print_stats();
        } else if(!strncmp(line,"strategy ",9)) {
            char *n=line+9; trim(n);
            if(!strcmp(n,"first_fit")||!strcmp(n,"first")){sh_set_strategy(SH_STRATEGY_FIRST_FIT);printf("  ✓ First Fit\n");}
            else if(!strcmp(n,"best_fit")||!strcmp(n,"best")){sh_set_strategy(SH_STRATEGY_BEST_FIT);printf("  ✓ Best Fit\n");}
            else if(!strcmp(n,"worst_fit")||!strcmp(n,"worst")){sh_set_strategy(SH_STRATEGY_WORST_FIT);printf("  ✓ Worst Fit\n");}
            else printf("  %s✗ Unknown. Use: first_fit, best_fit, worst_fit%s\n",C_RED,C_RESET);
        } else if(!strcmp(line,"list")||!strcmp(line,"ls")) {
            printf("\n  Active Blocks:\n"); int f=0;
            for(int i=0;i<MAX_BLOCKS;i++) if(blocks[i].active){f=1;printf("    #%d: %zu bytes @ %p\n",blocks[i].id,blocks[i].size,blocks[i].ptr);}
            if(!f) printf("    (none)\n"); printf("\n");
        } else if(!strcmp(line,"leak")||!strcmp(line,"leaks")) {
            sh_leak_check();
        } else if(!strncmp(line,"benchmark ",10)) {
            sh_benchmark_strategies((size_t)atol(line+10));
        } else if(!strcmp(line,"help")||!strcmp(line,"?")) {
            printf("\n  Commands: allocate <sz>, free <id>, calloc <n> <sz>,\n");
            printf("  realloc <id> <sz>, visualize, stats, strategy <name>,\n");
            printf("  list, leak, benchmark <n>, help, quit\n\n");
        } else if(!strcmp(line,"quit")||!strcmp(line,"exit")||!strcmp(line,"q")) {
            sh_leak_check(); break;
        } else printf("  %s✗ Unknown: '%s'%s\n",C_RED,line,C_RESET);
    }
    for(int i=0;i<MAX_BLOCKS;i++) if(blocks[i].active) sh_free(blocks[i].ptr);
    sh_destroy();
    printf("\n  %s✓ Shutdown complete.%s\n\n",C_GREEN,C_RESET);
    return 0;
}
