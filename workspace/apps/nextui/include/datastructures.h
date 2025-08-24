#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"


// --- General Purpose Array ---
typedef struct Array {
    int count;
    int capacity;
    void** items;
} Array;

Array* Array_new(void);
void Array_push(Array* self, void* item);
void Array_unshift(Array* self, void* item);
void* Array_pop(Array* self);
void Array_remove(Array* self, void* item);
void Array_reverse(Array* self);
void Array_free(Array* self);
void Array_yoink(Array* self, Array* other);
void StringArray_free(Array* self);
int StringArray_indexOf(Array* self, char* str);

// --- Simple Hash (Key-Value Store) ---
typedef struct Hash {
    Array* keys;
    Array* values;
} Hash;

Hash* Hash_new(void);
void Hash_free(Hash* self);
void Hash_set(Hash* self, char* key, char* value);
char* Hash_get(Hash* self, char* key);

// --- Filesystem Entry ---
enum EntryType {
    ENTRY_DIR,
    ENTRY_PAK,
    ENTRY_ROM,
    ENTRY_DIP,
    ENTRY_PLUGIN,
};

typedef struct Entry {
    char* path;
    char* name;
    char* unique;
    int type;
    int alpha;
} Entry;

Entry* Entry_new(char* path, int type);
Entry* Entry_newNamed(char* path, int type, char* displayName);
void Entry_free(Entry* self);
int EntryArray_indexOf(Array* self, char* path);
void EntryArray_sort(Array* self);
void EntryArray_free(Array* self);

// --- Simple Integer Array ---
#define INT_ARRAY_MAX 27
typedef struct IntArray {
    int count;
    int items[INT_ARRAY_MAX];
} IntArray;

IntArray* IntArray_new(void);
void IntArray_push(IntArray* self, int i);
void IntArray_free(IntArray* self);

// --- Directory Representation ---
typedef struct Directory {
    char* path;
    char* name;
    Array* entries;
    IntArray* alphas;
    int selected;
    int start;
    int end;
} Directory;

void Directory_free(Directory* self);
void DirectoryArray_pop(Array* self);
void DirectoryArray_free(Array* self);

// --- Recent Game Entry ---
typedef struct Recent {
    char* path;
    char* alias;
    int available;
} Recent;

Recent* Recent_new(char* path, char* alias);
void Recent_free(Recent* self);
int RecentArray_indexOf(Array* self, char* str);
void RecentArray_free(Array* self);

#endif // DATASTRUCTURES_H
