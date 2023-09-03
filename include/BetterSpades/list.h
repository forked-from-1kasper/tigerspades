#ifndef LIST_H
#define LIST_H

typedef struct {
    void * data;
    size_t elements, element_size, mem_size;
} List;

enum list_traverse_direction {
    LIST_TRAVERSE_FORWARD,
    LIST_TRAVERSE_BACKWARD,
};

int list_created(List * l);
void list_create(List * l, size_t element_size);
void list_free(List * l);
void list_sort(List * l, int (*cmp)(const void * obj, const void * ref));
void * list_find(List * l, void* ref, enum list_traverse_direction dir, int (*cmp)(void * a, void * b));
void * list_get(List * l, size_t i);
void * list_add(List * l, void * e);
void list_remove(List * l, size_t i);
void list_clear(List * l);
int list_size(List * l);

#endif
