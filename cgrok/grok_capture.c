#include "grok.h"
#include "grok_capture.h"

#include <assert.h>

#define CAPTURE_NUMBER_NOT_SET (-1)

char *EMPTYSTR = "";

/* 
 * public domain strtok_r() by Charlie Gordon
 *
 *   from comp.lang.c  9/14/2007
 *
 *      http://groups.google.com/group/comp.lang.c/msg/2ab1ecbb86646684
 *
 *     (Declaration that it's public domain):
 *      http://groups.google.com/group/comp.lang.c/msg/7c7b39328fefab9c
 *      
 *      This is needed to build on Windows with GCC 4.9.0
 */

#ifdef _WIN64
char* strtok_r(char *str, const char *delim, char **nextp) {
    char *ret;

    if (str == NULL)
    {
        str = *nextp;
    }

    str += strspn(str, delim);

    if (*str == '\0')
    {
        return NULL;
    }

    ret = str;

    str += strcspn(str, delim);

    if (*str)
    {
        *str++ = '\0';
    }

    *nextp = str;

    return ret;
}
#endif

void grok_capture_init(grok_t *grok, grok_capture *gct) {
  gct->id = CAPTURE_NUMBER_NOT_SET;
  gct->pcre_capture_number = CAPTURE_NUMBER_NOT_SET;

  gct->name = NULL;
  gct->name_len = 0;
  gct->subname = NULL;
  gct->subname_len = 0;
  gct->pattern = NULL;
  gct->pattern_len = 0;
  gct->predicate_lib = NULL;
  gct->predicate_func_name = NULL;
  gct->extra.extra_len = 0;
  gct->extra.extra_val = NULL;
}

void grok_capture_add(grok_t *grok, const grok_capture *gct, int only_renamed) {
  grok_log(grok, LOG_CAPTURE, 
           "Adding pattern '%s' as capture %d (pcrenum %d)",
           gct->name, gct->id, gct->pcre_capture_number);

  if (only_renamed && strstr(gct->name, ":") == NULL) {
    return;
  }

  /* Primary key is id */
  tctreeput(grok->captures_by_id, &(gct->id), sizeof(gct->id),
            gct, sizeof(grok_capture));
  /* Tokyo Cabinet doesn't seem to support 'secondary indexes' like BDB does,
   * so let's manually update all the other 'captures_by_*' trees */
  int unused_size;
  tctreeput(grok->captures_by_capture_number, &(gct->pcre_capture_number), 
            sizeof(gct->pcre_capture_number), gct, sizeof(grok_capture));


  int i, listsize;
  /* TCTREE doesn't permit dups, so let's make the structure a tree of arrays,
   * keyed on a string. */
  /* captures_by_name */
  TCLIST *by_name_list;
  by_name_list = (TCLIST *) tctreeget(grok->captures_by_name,
                                      (const char *)gct->name,
                                      gct->name_len, &unused_size);
  if (by_name_list == NULL) {
    by_name_list = tclistnew();
  }
  /* delete a capture with the same capture id  so we can replace it*/
  listsize = tclistnum(by_name_list);
  for (i = 0; i < listsize; i++) {
    grok_capture *list_gct;
    list_gct = (grok_capture *)tclistval(by_name_list, i, &unused_size);
    if (list_gct->id == gct->id) {
      tclistremove(by_name_list, i, &unused_size);
      break;
    }
  }
  tclistpush(by_name_list, gct, sizeof(grok_capture));
  tctreeput(grok->captures_by_name, gct->name, gct->name_len,
            by_name_list, sizeof(TCLIST));
  /* end captures_by_name */

  /* captures_by_subname */
  TCLIST *by_subname_list;
  by_subname_list = (TCLIST *) tctreeget(grok->captures_by_subname,
                                         (const char *)gct->subname,
                                         gct->subname_len, &unused_size);
  if (by_subname_list == NULL) {
    by_subname_list = tclistnew();
  }
  /* delete a capture with the same capture id so we can replace it*/
  listsize = tclistnum(by_subname_list);
  for (i = 0; i < listsize; i++) {
    grok_capture *list_gct;
    list_gct = (grok_capture *)tclistval(by_subname_list, i, &unused_size);
    if (list_gct->id == gct->id) {
      tclistremove(by_subname_list, i, &unused_size);
      break;
    }
  }
  tclistpush(by_subname_list, gct, sizeof(grok_capture));
  tctreeput(grok->captures_by_subname, gct->subname, gct->subname_len,
            by_subname_list, sizeof(TCLIST));
  /* end captures_by_subname */
}

const grok_capture *grok_capture_get_by_id(const grok_t *grok, int id) {
  int unused_size;
  const grok_capture *gct;
  gct = tctreeget(grok->captures_by_id, &id, sizeof(id), &unused_size);
  return gct;
}

const grok_capture *grok_capture_get_by_name(const grok_t *grok, const char *name) {
  int unused_size;
  const grok_capture *gct;
  const TCLIST *by_name_list;
  by_name_list = tctreeget(grok->captures_by_name, name, strlen(name),
                           &unused_size);

  if (by_name_list == NULL)
    return NULL;

  /* return the first capture by this name in the list */
  gct = tclistval(by_name_list, 0, &unused_size);
  return gct;
}

const grok_capture *grok_capture_get_by_subname(const grok_t *grok,
                                                const char *subname) {
  int unused_size;
  const grok_capture *gct;
  const TCLIST *by_subname_list;
  by_subname_list = tctreeget(grok->captures_by_subname, subname,
                              strlen(subname), &unused_size);

  if (by_subname_list == NULL)
    return NULL;

  gct = tclistval(by_subname_list, 0, &unused_size);
  return gct;
}

const grok_capture *grok_capture_get_by_capture_number(grok_t *grok,
                                                       int capture_number) {
  int unused_size;
  const grok_capture *gct;
  gct = tctreeget(grok->captures_by_capture_number, &capture_number,
                   sizeof(capture_number), &unused_size);
  return gct;
}

int grok_capture_set_extra(grok_t *grok, grok_capture *gct, void *extra) {
  /* Store the pointer to extra.
   * XXX: This is potentially bad voodoo. */
  grok_log(grok, LOG_CAPTURE, "Setting extra value of 0x%x", extra);

  /* We could copy it this way, but if you compile with -fomit-frame-pointer,
   * this data is lost since extra is in the stack. Copy the pointer instead.
   * TODO(sissel): See if we don't need this anymore since using tokyocabinet.
   */
  //gct->extra.extra_val = (char *)&extra;

  gct->extra.extra_len = sizeof(void *); /* allocate pointer size */
  gct->extra.extra_val = malloc(gct->extra.extra_len);
  memcpy(gct->extra.extra_val, &extra, gct->extra.extra_len);
  //gct->extra.extra_len = extra;
  //gct->extra.extra_val = extra;
  return 0;
}

#define _GCT_STRFREE(gct, member) \
  if (gct->member != NULL && gct->member != EMPTYSTR) { \
    free(gct->member); \
  }

void grok_capture_free(grok_capture *gct) {
  _GCT_STRFREE(gct, name);
  _GCT_STRFREE(gct, subname);
  _GCT_STRFREE(gct, pattern);
  _GCT_STRFREE(gct, predicate_lib);
  _GCT_STRFREE(gct, predicate_func_name);
  _GCT_STRFREE(gct, extra.extra_val);
}

/* this function will walk the captures_by_id table */
TCTREE_ITER *grok_capture_walk_init(const grok_t *grok) {
  return tctreeiterinit(grok->captures_by_id);
}

const grok_capture *grok_capture_walk_next(const TCTREE_ITER *iter, const grok_t *grok) {
  int id_size;
  int gct_size;
  int *id;
  const grok_capture *gct;

  id = (int *)tctreeiternext(iter, &id_size);
  if (id == NULL) {
    grok_log(grok, LOG_CAPTURE, "walknext null");
    return NULL;
  }
    grok_log(grok, LOG_CAPTURE, "walknext ok %d", *id);

  gct = (grok_capture *)tctreeget(grok->captures_by_id, id, id_size,
                                  &gct_size);
  return gct;
}

int grok_capture_walk_end(grok_t *grok) {
  /* nothing, anymore */
  return 0;
}
