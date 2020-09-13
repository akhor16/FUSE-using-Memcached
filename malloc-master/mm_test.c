#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

/* Function pointers to hw3 functions */
void* (*mm_malloc)(size_t);
void* (*mm_realloc)(void*, size_t);
void (*mm_free)(void*);

void load_alloc_functions() {
    void *handle = dlopen("hw3lib.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    char* error;
    mm_malloc = dlsym(handle, "mm_malloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_realloc = dlsym(handle, "mm_realloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_free = dlsym(handle, "mm_free");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }
}

int main() {
    load_alloc_functions();

    int *data = (int*) mm_malloc(sizeof(int));
    assert(data != NULL);
    data[0] = 0x162;
    mm_free(data);
    int *data1 = (int*) mm_malloc(sizeof(int));
    assert(data == data1);
    void * ptr = mm_malloc(100);
    mm_free(data1);
    mm_free(ptr);
    void * ptr1 = mm_malloc(100);
    assert(ptr1 == data1);
    void * ptr2 = mm_realloc(ptr1, 200);
    assert(ptr1 == ptr2);
    mm_free(ptr2);
    char * s1 = mm_malloc(10 * sizeof(char));
    for(int i = 0; i < 10; i++){
        s1[i] = 'a' + i;
    }
    int * arr = mm_realloc(NULL, 7 * sizeof(int));
    for(int i = 0; i < 7; i++){
        arr[i] = i;
    }
    char * s2 = mm_malloc(4 * sizeof(char));
    for(int i = 0; i < 4; i++){
        s2[i] = 'a' + i;
    }
    for(int i = 0; i < 10; i++){
        assert(s1[i] == 'a' + i);
    }
    for(int i = 0; i < 7; i++){
        assert(arr[i] == i);
    }
    for(int i = 0; i < 4; i++){
        assert(s2[i] == 'a' + i);
    }
    int * arr2 = mm_realloc(arr, 20 * sizeof(int));
    for(int i = 0; i < 7; i++){
        assert(arr2[i] == i);
    }
    for(int i = 7; i < 20; i++){
        //printf("%d ", arr2[i]);
        assert(arr2[i] == 0);
    }
    printf("malloc test successful!\n");
    return 0;
}
