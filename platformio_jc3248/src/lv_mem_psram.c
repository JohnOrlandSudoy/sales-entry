#include "lv_mem_psram.h"

#include <esp_heap_caps.h>
#include <stdlib.h>

void *lv_mem_psram_alloc(size_t size)
{
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = malloc(size);
    }
    return p;
}

void lv_mem_psram_free(void *ptr)
{
    if (ptr) {
        heap_caps_free(ptr);
    }
}

void *lv_mem_psram_realloc(void *ptr, size_t size)
{
    void *p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p && size > 0) {
        p = realloc(ptr, size);
    }
    return p;
}
