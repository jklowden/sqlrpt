/*
 * Copyright 2017 James K. Lowden. 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 */

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <search.h>

#include "report.h"

struct colfmt_t *colfmts = NULL;
size_t ncolfmt = 0, capacity = 0;

int
colfmt_cmp( const void *K, const void *E ) {
  const struct colfmt_t *k = K, *e = E;
  return strcmp(k->colname, e->colname);
}

const char *
column_format( const char colname[], const char fmt[] ) {
  const struct colfmt_t key = { .colname = (char *)colname, .fmt = (char*)fmt };
  struct colfmt_t *p;

  if( ncolfmt == capacity ) {
    size_t n = capacity == 0? 16 : 2 * capacity;
    if( (p = realloc(colfmts, sizeof(colfmts[0]) * n)) == NULL ) {
      err(EXIT_FAILURE, "%s", __func__ );
    }
    colfmts = p;
    capacity = n;
  }

  if( (p = lsearch(&key, colfmts, &ncolfmt, sizeof(colfmts[0]),
		   colfmt_cmp)) == NULL )
  {
    err(EXIT_FAILURE, "%s", __func__ );
  }

  return p->fmt;
}
