#ifndef ANIMAL_ANY_H
#define ANIMAL_ANY_H

#include <stdbool.h>

/**
 * "Any animal": active while at least one species is active. Fed with the start/stop events of
 * the individual species.
 */
typedef struct animal_any animal_any_t;

animal_any_t* animal_any_new(int n_labels);
void animal_any_free(animal_any_t* a);

/** +1: "any" just became active, -1: it just became inactive, 0: no change. */
int animal_any_update(animal_any_t* a, int label, bool detected);

#endif
