#include "tree.h"
#include <stdint.h>
#include <string.h>

// A shim to use dictlib trees instead of TC
struct TCTREE {
  dict *dict;
  dict_itor *iter;
};

// Toyko cabinet accepts any arbitrary-sized data as a tree key.
// We support this by using the first 4 bytes of the key to store the length.
// So we have to wrap the libdict comparators with implementations that skip the first four bytes
// of the key
int tccmpint32(const void* k1, const void* k2) {
  return dict_int_cmp(k1+4, k2+4);
}

int dict_var_str_cmp(const void* k1, const void* k2) {
  return dict_str_cmp(k1+4, k2+4);
}

void tcfree(void *key, void *value) {
  free(key);
  free(value);
}

TCTREE *tctreenew(void) {
  return tctreenew2(dict_var_str_cmp, 0);
}

TCTREE *tctreenew2(dict_compare_func cp, void *cmpop) {
  TCTREE *tree = malloc(sizeof(TCTREE));
  tree->dict = hb_dict_new(cp, tcfree);
  tree->iter = NULL;
  return tree; 
}

// Iterate over elements in ascending key order
void tctreeiterinit(TCTREE *tree) {
  if (tree->iter != NULL) {
    dict_itor_free(tree->iter);
  }
  tree->iter = dict_itor_new(tree->dict);
  dict_itor_first(tree->iter);
}

const void *tctreeiternext(TCTREE *tree, int *sp) {
  void *key = dict_itor_key(tree->iter);
  dict_itor_next(tree->iter);
  *sp = 0;
  if (key) {
    *sp = *(uint32_t*)key;
    return key+4;
  }
  return NULL;
}

void *tcpack(const void *value, uint32_t size) {
  char *buf = malloc(size + 5);
  memcpy(buf, &size, 4);
  memcpy(buf+4, value, size);
  buf[size+4] = 0;
  return buf;
}

void tctreeput(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
  void *key = tcpack(kbuf, ksiz);
  void *val = tcpack(vbuf, vsiz);
  bool inserted; 
  void ** valPtr = dict_insert(tree->dict, key, &inserted);
  *valPtr = val;
}

bool tctreeputkeep(TCTREE *tree, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
  void *key = tcpack(kbuf, ksiz); 
  void *val = tcpack(vbuf, vsiz); 
  bool inserted;
  void **valPtr = dict_insert(tree->dict, key, &inserted);
  if (inserted) {
    *valPtr = val;
  }
  return inserted;
}

const void *tctreeget(TCTREE *tree, const void *kbuf, int ksiz, int *sp) {
  const void *key = tcpack(kbuf, ksiz);
  void *value = dict_search(tree->dict, key);
  *sp = 0;
  if (value) {
    *sp = *(uint32_t*)value;
    return value+4;
  }
  return NULL;
}

void tctreeclear(TCTREE *tree) {
  dict_clear(tree->dict); 
}

void tctreedel(TCTREE *tree) {
  dict_free(tree->dict);
  if (tree->iter) {
    dict_itor_free(tree->iter);
  }
  free(tree);
}

// A simple, singly linked list
TCLIST *tclistnew(void) {
  TCLIST *list = malloc(sizeof(TCLIST));
  list->len = 0;
  TCLISTELEM *newElem = malloc(sizeof(TCLISTELEM));
  newElem->elem = 0;
  list->head = newElem;
  return list;
}

int tclistnum(const TCLIST *list) {
  return list->len;
}

void *tclistpack(void *buf, uint32_t size) {
  void *elem = malloc(size+1);
  memcpy(elem, buf, size);
  ((char *)elem)[size] = 0;
  return elem;
}

void tclistpush(TCLIST *list, const void *ptr, int size) {
  TCLISTELEM *head = list->head;
  for (int i=0; i < list->len; i++) {
    head = head->next;
  }
  TCLISTELEM *newElem = malloc(sizeof(TCLISTELEM));
  newElem->elem = tclistpack(ptr, size);
  newElem->size = size;
  head->next = newElem;
  list->len += 1;
}

void *tclistremove(TCLIST *list, int index, int *sp) {
  if (index < 0 || index >= list->len) {
    return NULL;
  }
  TCLISTELEM *head = list->head;
  for (int i=0; i < index; i++) {
    head = head->next;
  }
  
  TCLISTELEM *removed = head->next;
  head->next = removed->next;
  void* elem = removed->elem;
  *sp = removed->size;
  free(removed);
  list->len -= 1;
  return elem;
}

void tclistover(TCLIST *list, int index, const void *ptr, int size) {
  if (index < 0 || index >= list->len) {
    // Out of bounds, do nothing
    return;
  }
  TCLISTELEM *head = list->head;
  for (int i=0; i <= index; i++) {
    head = head->next;
  }

  if (head->elem) {
    free(head->elem);
  }
 
  head->elem = tclistpack(ptr);
  head->size = size; 
}

const void *tclistval(const TCLIST *list, int index, int *sp) {
  if (index >= list->len) {
    // Out of bounds, do nothing
    return 0;
  }
  TCLISTELEM *head = list->head;
  for (int i=0; i <= index; i++) {
    head = head->next;
  }
  *sp = head->size;
  return head->elem;
}

void tclistdel(TCLIST *list) {
  TCLISTELEM *head = list->head;
  for (int i=0; i < list->len; i++) {
    if (head->elem) {
      free(head->elem);
    }
    TCLISTELEM *oldhead = head;
    head = head->next;
    free(oldhead);
  }
  free(list);
}
