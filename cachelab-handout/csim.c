/* 
 *  Name: Aihua Peng
 *  AndrewID: aihuap
 *
 *
 *  csim.c - Cache Simulator
 *
 *
 *  This file will write a cache simulator that takes a valgrind memory trace
 *  as input, simulates the hit/miss behavior of a cache memory on this trace,
 *  and outputs the total number of hits, misses, and evictions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <getopt.h>
#include "cachelab.h"

typedef struct{
    unsigned valid;
    unsigned long tag;
    unsigned LRU;
}line;

typedef struct{
    unsigned long t;
    unsigned long s;
    unsigned long b;
}address;

address parseAddress(long, unsigned, unsigned);
unsigned goToCache(line **, address,unsigned,unsigned);
int parseCMD(int,char **, unsigned*,unsigned*,unsigned*,char*);

int main(int argc, char ** argv)
{
    unsigned s = 0;
    unsigned E = 0;
    unsigned b = 0;
    char path[20] = "";
    
    parseCMD(argc, argv, &s, &E, &b,path);
    
    unsigned S = 1<<s;
    unsigned hitCount = 0;
    unsigned missCount = 0;
    unsigned replaceCount = 0;
    
    line **cache;
    
    //Ask for cache space.
    //Cache is pointing to S pointers.
    //Each of them (line pointer) is pointing to E lines.
    cache = (line**) malloc(S*sizeof(line *));
    for (int i=0;i<S;i++){
        cache[i] = (line*)malloc(E*sizeof(line));
    }
    
    //Initialization.
    for (int i=0; i<S; i++) {
        for (int j=0; j<E; j++) {
            cache[i][j].LRU = 0;
            cache[i][j].tag = 0;
            cache[i][j].valid = 0;
        }
    }
    
    char identifier;
    unsigned long addr;
    int size;
    FILE *pFile;
    pFile = fopen(path, "r");
    
    //Read address from file. Access the cache by this address.
    while (fscanf(pFile, " %c %lx,%d",&identifier, &addr, &size)!=EOF) {
        if (identifier == 'I') {
            continue;
        }
        if (identifier == 'M') {
            hitCount += 1;
        }
        address suchAddr = parseAddress(addr,s,b);
        unsigned whatHappened = goToCache(cache, suchAddr, S, E);
        
        //Based on cache's response, do some statistic.
        switch (whatHappened) {
            case 1:
                hitCount += 1;
                break;
            case 2:
                missCount += 1;
                break;
            case 3:
                replaceCount += 1;
                
                missCount +=1;
                break;
            default:
                printf("something wents wrong");
                break;
        }
    }
    fclose(pFile);
    for (int i=0; i<S; i++) {
        free(cache[i]);
    }
    free(cache);
    printSummary(hitCount, missCount, replaceCount);
    return 0;
    
}


/*
 * ParseCMD - This is a function to parse command line input. You can specify
 *     s, E, b, t and the file path in command line.
 */
int parseCMD(int argc, char** argv, unsigned* s, unsigned* E, unsigned* b, char* path){
    opterr = 0;
    int opt=0;
    while((opt = getopt(argc, argv, "s:E:b:t:")) != -1){
        switch (opt) {
            case 's':
                *s = atoi(optarg);
                break;
            case 'E':
                *E = atoi(optarg);
                break;
            case 'b':
                *b = atoi(optarg);
                break;
            case 't':
                strcpy(path, optarg);
                break;
            default:
                printf("wrong cmd");
                break;
        }
    }
    return 0;
}

/*
 * parseAddress - This is a function to translate numberical address to struct.
 *     The numberical address should be no more than 64 bits.
 */
address parseAddress(long addr, unsigned s, unsigned b){
    address result = {0,0,0};
    result.b = addr& (0x7fffffffffffffff>>(63-b));
    addr = addr>>b;
    result.s = addr& (0x7fffffffffffffff>>(63-s));
    addr = addr>>s;
    result.t = addr;
    return result;
}

/*
 * goToCache - This is a function to react towards a searching quest. 
 *     It returns 1 after search hits, 2 after address is added to the 
 *     cache, 3 after a eviction. The cache response is based on LRU 
 *     (Least Recent Used).
 */
unsigned goToCache(line **cache, address suchAddr, unsigned S,unsigned E){
    unsigned returnCode = 0;
    unsigned long setIndex = suchAddr.s;
    int hit = 0;
    int emptyLine = -1;
    int maxLRU = 0;
    int whoHasMinLRU = -1;
    line *currentSetInCache = cache[setIndex];
    for (int j=0; j<E; j++) {
        line currentLineInCache = currentSetInCache[j];
        if (currentLineInCache.valid == 0) {
            emptyLine = j;
            continue;
        }
        
        //The request hits successfully.
        if (currentLineInCache.valid == 1 && currentLineInCache.tag == suchAddr.t) {
            hit = 1;
            returnCode = 1;
            cache[setIndex][j].LRU = 0;
            break;
        }
        
        //Update LRU accumulator.
        if (currentLineInCache.valid == 1 && currentLineInCache.tag != suchAddr.t) {
            cache[setIndex][j].LRU += 1;
            
        }
    }
    
    //The request did not hit, but to be added.
    if (emptyLine != -1 && hit == 0) {
        line addLine = {1,suchAddr.t,0};
        cache[setIndex][emptyLine] = addLine;
        returnCode = 2;
    }
    
    //The request did not hit, but to do a eviction.
    if (emptyLine == -1 && hit == 0) {
        for (int k=0; k<E; k++) {
            if (cache[setIndex][k].LRU>=maxLRU &&cache[setIndex][k].valid != 0) {
                maxLRU= cache[setIndex][k].LRU;
                whoHasMinLRU = k;
            }
        }
        line replaceLine = {1,suchAddr.t,0};
        cache[setIndex][whoHasMinLRU] = replaceLine;
        returnCode = 3;
    }
    
    return returnCode;
}

