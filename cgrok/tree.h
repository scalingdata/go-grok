#include <stdbool.h>
#include <stdlib.h>
#include "dict.h"

typedef struct TCTREE TCTREE;

// A replacement for the 32-bit int key comparator
int tccmpint32(const void* k1, const void* k2);

// The default comparator is for strings
int dict_var_str_cmp(const void* k1, const void* k2);

TCTREE *tctreenew(void);
TCTREE *tctreenew2(dict_compare_func cp, void *cmpop);
void tctreeiterinit(TCTREE *tree);
const void *tctreeiternext(TCTREE *tree, int *sp);
void tctreeput(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
bool tctreeputkeep(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
const void *tctreeget(TCTREE *tree, const void *kbuf, int ksiz, int *sp);
void tctreedel(TCTREE *tree);
void tctreeclear(TCTREE *tree);

// List functions

typedef struct TCLISTELEM TCLISTELEM;

typedef struct {
  TCLISTELEM *head;
  int len;
} TCLIST;

struct TCLISTELEM {
  int size;
  void *elem;
  TCLISTELEM *next;
};

TCLIST *tclistnew(void);
int tclistnum(const TCLIST *list);
void tclistpush(TCLIST *list, const void *ptr, int size);
void *tclistremove(TCLIST *list, int index, int *sp);
void tclistover(TCLIST *list, int index, const void *ptr, int size);
const void *tclistval(const TCLIST *list, int index, int *sp);
void tclistdel(TCLIST *list);