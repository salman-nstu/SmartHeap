/*
 * SmartHeap - demo_benchmark.c
 * Benchmark comparing SmartHeap strategies and vs libc malloc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include "smartheap.h"

#define C_RESET "\033[0m"
#define C_BOLD  "\033[1m"
#define C_RED   "\033[91m"
#define C_GREEN "\033[92m"
#define C_YELLOW "\033[93m"
#define C_CYAN  "\033[96m"
#define C_WHITE "\033[97m"

#define NUM_OPS 50000
#define MAX_PTRS 4096

static double bench_smartheap(sh_strategy strat, const char *name) {
    sh_init();
    sh_set_strategy(strat);
    srand(42);
    void *ptrs[MAX_PTRS]; int cnt=0;
    clock_t start=clock();
    for(int i=0;i<NUM_OPS;i++){
        if(cnt>0&&(rand()%3==0)){int x=rand()%cnt;sh_free(ptrs[x]);ptrs[x]=ptrs[--cnt];}
        else if(cnt<MAX_PTRS){size_t s=16+(rand()%1009);void *p=sh_malloc(s);if(p){memset(p,0xAA,s);ptrs[cnt++]=p;}}
    }
    clock_t end=clock();
    double ms=(double)(end-start)/CLOCKS_PER_SEC*1000.0;
    sh_print_stats();
    for(int i=0;i<cnt;i++) sh_free(ptrs[i]);
    sh_destroy();
    return ms;
}

static double bench_libc(void) {
    srand(42);
    void *ptrs[MAX_PTRS]; int cnt=0;
    clock_t start=clock();
    for(int i=0;i<NUM_OPS;i++){
        if(cnt>0&&(rand()%3==0)){int x=rand()%cnt;free(ptrs[x]);ptrs[x]=ptrs[--cnt];}
        else if(cnt<MAX_PTRS){size_t s=16+(rand()%1009);void *p=malloc(s);if(p){memset(p,0xAA,s);ptrs[cnt++]=p;}}
    }
    clock_t end=clock();
    for(int i=0;i<cnt;i++) free(ptrs[i]);
    return (double)(end-start)/CLOCKS_PER_SEC*1000.0;
}

int main(void) {
    SetConsoleOutputCP(65001);
    HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE); DWORD m=0;
    GetConsoleMode(h,&m); SetConsoleMode(h,m|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    printf("\n%s%s  SmartHeap Benchmark (%d operations)%s\n\n",C_BOLD,C_CYAN,NUM_OPS,C_RESET);

    double t1=bench_smartheap(SH_STRATEGY_FIRST_FIT,"First Fit");
    double t2=bench_smartheap(SH_STRATEGY_BEST_FIT,"Best Fit");
    double t3=bench_smartheap(SH_STRATEGY_WORST_FIT,"Worst Fit");
    double t4=bench_libc();

    printf("\n%s%s  ╔═══════════════════════════════════════╗%s\n",C_YELLOW,C_BOLD,C_RESET);
    printf("%s%s  ║        Benchmark Results              ║%s\n",C_YELLOW,C_BOLD,C_RESET);
    printf("%s%s  ╠═══════════════════════════════════════╣%s\n",C_YELLOW,C_BOLD,C_RESET);
    printf("%s  ║%s  %-15s │ %8.1f ms       %s║%s\n",C_YELLOW,C_RESET,"First Fit",t1,C_YELLOW,C_RESET);
    printf("%s  ║%s  %-15s │ %8.1f ms       %s║%s\n",C_YELLOW,C_RESET,"Best Fit",t2,C_YELLOW,C_RESET);
    printf("%s  ║%s  %-15s │ %8.1f ms       %s║%s\n",C_YELLOW,C_RESET,"Worst Fit",t3,C_YELLOW,C_RESET);
    printf("%s%s  ╠═══════════════════════════════════════╣%s\n",C_YELLOW,C_BOLD,C_RESET);
    printf("%s  ║%s  %s%-15s%s │ %8.1f ms       %s║%s\n",C_YELLOW,C_RESET,C_GREEN,"libc malloc",C_RESET,t4,C_YELLOW,C_RESET);
    printf("%s%s  ╚═══════════════════════════════════════╝%s\n\n",C_YELLOW,C_BOLD,C_RESET);
    return 0;
}
