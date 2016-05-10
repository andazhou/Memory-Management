#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>



typedef struct {
    unsigned long tag;
    int valid;
    int LRU;
} lines;


typedef struct {
    lines* line;
} sets;

typedef struct {
    int s;
    int S;
    int E;
    int b;
    int B;
    int hit;
    int miss;
    int evicted;
    sets* cachesets;
} cache;



cache init_cache(int setflag, int lineflag, int blockflag) {
    cache cacheSim;
    sets init;
    lines iline;
    cacheSim.s = setflag;
    cacheSim.S = (1 << setflag);
    cacheSim.E = lineflag;
    cacheSim.b = blockflag;
    cacheSim.B = (1 << blockflag);

    cacheSim.cachesets = (sets *)malloc(sizeof(sets) * cacheSim.S);
    for (int i = 0; i < cacheSim.S; i++) {
        init.line = (lines *)malloc(sizeof(lines) * cacheSim.E);
        cacheSim.cachesets[i] = init;
        for (int j = 0; j < cacheSim.E; j++) {
            iline.tag = 0;
            iline.valid = 0;
            iline.LRU = 0;
            init.line[j] = iline;

        }

    }
    cacheSim.hit = 0;
    cacheSim.miss = 0;
    cacheSim.evicted = 0;
    return cacheSim;
}


void free_cash(cache cash) {

    for (int i = 0; i < cash.S; i++) {
        free(cash.cachesets[i].line);
        }

    free(cash.cachesets);

}


int evict(sets iset, int E) {
    // Check if full

    for (int i = 0; i < E; i++) {
         if (iset.line[i].valid == 0 && iset.line[i].tag == 0) return 0;
         }

    // Set default lowest as first line

    int min = 0;

    // Indeed full; lowest LRU gtfo

    for (int j = 0; j < E; j++) {
        if (iset.line[j].LRU <= min) min = j;
         }

    // min is kicked out
    // you don't have to go home but you can't stay here

    iset.line[min].valid = 0;
    iset.line[min].tag = 0;
    iset.line[min].LRU = 0;
    return 1;
}

int hit(cache cash, unsigned long long tag, unsigned long long set) {

    for (int i = 0; i < cash.E; i++) {

        // Check if valid and tags match

        if (tag == cash.cachesets[set].line[i].tag) {

            if (cash.cachesets[set].line[i].valid == 1) {

                cash.cachesets[set].line[i].LRU++;

                return 1;

            }
        }

    }

    return 0;
}

cache cacheflow(cache cash, unsigned long address) {

    // Obtain tag & set info

    unsigned long long tag = address >> (cash.s + cash.b);
    unsigned long long iset = (address << (64 - (cash.s + cash.b)));

    iset = iset >> (64 - cash.s);

    if (hit(cash, tag, iset) == 1) {
        cash.hit++;

        return cash;
    }

    // Miss!

    cash.miss++;

    // Is the set full?


    if (evict(cash.cachesets[iset], cash.E) == 1) {
    // Evicted because not enough cash $$
	    cash.evicted++;
	}

    // Write to cache

    for (int j = 0; j < cash.E; j++) {

        // Find empty line

        if (cash.cachesets[iset].line[j].valid == 0) {
            cash.cachesets[iset].line[j].valid = 1;
            cash.cachesets[iset].line[j].tag = tag;
            cash.cachesets[iset].line[j].LRU++;
            return cash;
        }
    }

    // No empty line; error has occured

    printf("ERROR ERROR You shouldn't get here.");

    return cash;
}


int main(int argc, char **argv) {
    //return status of 0
    //calls printSummary(hit_count, miss_count, eviction_count) with results

    int opt;
    int setflag = 0;
    int lineflag = 0;
    int blockflag = 0;
    char* tname = "trace.file";

    while (-1 != (opt = getopt(argc, argv, "s:E:b:t:"))) {
        switch (opt) {
            case 's':
                setflag = atoi(optarg);
                break;
            case 'E':
                lineflag = atoi(optarg);
                break;
            case 'b':
                blockflag = atof(optarg);
                break;
            case 't':
                tname = optarg;
                break;
            default:
                printf("Unknown Arg");
                break;
        }
    }




    //Read trace.file

    FILE *pFile;

    pFile= fopen(tname, "r");

    char access_type;
    unsigned long long address;
    int size;

    cache muchCache = init_cache(setflag, lineflag, blockflag);

    while (fscanf(pFile," %c %llx,%d", &access_type, &address, &size) > 0) {
        switch(access_type) {
            case 'L':
                // Loading data

                muchCache = cacheflow(muchCache, address);

                break;
            case 'S':
                // Storing data

                muchCache = cacheflow(muchCache, address);

                break;
            case 'M':
                // Modifying data

                muchCache = cacheflow(muchCache, address);

                muchCache = cacheflow(muchCache, address);

                break;
            default:
                break;
        }
    }





    printSummary(muchCache.hit, muchCache.miss, muchCache.evicted);

    // Free cash! ;)

    free_cash(muchCache);

    return 0;
}



