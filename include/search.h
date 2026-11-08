// SPDX-License-Identifier: MIT
#pragma once

typedef void posix_tnode;

typedef enum {
    preorder,
    postorder,
    endorder,
    leaf
} VISIT;

#ifdef __cplusplus
extern "C" {
#endif

void* tdelete(const void* key, posix_tnode** rootp,
              int (*compar)(const void*, const void*));
posix_tnode* tfind(const void* key, posix_tnode* const* rootp,
                   int (*compar)(const void*, const void*));
posix_tnode* tsearch(const void* key, posix_tnode** rootp,
                     int (*compar)(const void*, const void*));
void twalk(const posix_tnode* root,
           void (*action)(const posix_tnode*, VISIT, int));
void insque(void* element, void* pred);
void remque(void* element);

#ifdef __cplusplus
}
#endif
