#include "animal_any.h"

#include <stdlib.h>

struct animal_any {
    int n_labels;
    bool* active;
    int count;
};

animal_any_t* animal_any_new(int n_labels) {
    animal_any_t* a = calloc(1, sizeof(*a));
    a->n_labels     = n_labels;
    a->active       = calloc((size_t)n_labels, sizeof(bool));
    return a;
}

void animal_any_free(animal_any_t* a) {
    if (a == NULL)
        return;
    free(a->active);
    free(a);
}

int animal_any_update(animal_any_t* a, int label, bool detected) {
    if (label < 0 || label >= a->n_labels || a->active[label] == detected)
        return 0;
    a->active[label] = detected;
    int before       = a->count;
    a->count += detected ? 1 : -1;
    if (before == 0 && a->count == 1)
        return 1;
    if (before == 1 && a->count == 0)
        return -1;
    return 0;
}
