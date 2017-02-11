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
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#define COUNT_OF(x) (sizeof(x)/sizeof(x[0]))

static void
syntax( const char name[]  ) {
  errx(EXIT_FAILURE,
       "%s [-f format] [-w width] -d dbname -q query", basename(name));
}

static int
report( sqlite3_stmt *stmt, int ncol, const char *format ) {
  const char *sep = "";

  if( format ) { // first line
    const char *p = strchr(format, ';');
    if( p == NULL ) {
      printf( "%s;\n", format ); // table options

      // alignment of titles
      char heading[ncol];;
      memset(heading, 'L', ncol);
      printf("%.*s\n", ncol, heading);

      // alignment of data
      char ch = 'c';
      for( int c=0; c < ncol; c++, sep = " " ) {
	switch( sqlite3_column_type(stmt, c) ) {
	case SQLITE_INTEGER:
	case SQLITE_FLOAT:
	   ch = 'N'; break;
	   break;

	case SQLITE_TEXT:
	case SQLITE_BLOB:
	case SQLITE_NULL:
	default:
	  ch = 'L';
	  break;
	}

	printf( "%s%c", sep, ch );
      }
      printf(" .\n");

      // column headers
      sep = "";
      for( int c=0; c < ncol; c++, sep = "\t" ) {
	printf( "%sT{\n%s\nT}", sep, sqlite3_column_name(stmt, c) );
      }
      printf("\n_\n");
    } else {
      printf("%.*s\n%s\n", (int)(p - format), format, p);    }
  }

  sep = "";
  for( int c=0; c < ncol; c++, sep = "\t" ) {
    switch( sqlite3_column_type(stmt, c) ) {
    case SQLITE_INTEGER:
      printf( "%s%d", sep, sqlite3_column_int(stmt, c) );
      break;
    case SQLITE_FLOAT:
      printf( "%s%f", sep, sqlite3_column_double(stmt, c) );
      break;
    case SQLITE_TEXT:
      printf( "%sT{\n%s\nT}", sep, (char*)sqlite3_column_text(stmt, c) );
      break;
    case SQLITE_BLOB: default:
      assert(false);
      break;
    case SQLITE_NULL:
      printf( "%s%s", sep, "NULL" );
    }
  }
  printf("\n");
  return SQLITE_OK;
}

static int
execsql( sqlite3* db, const char *sql, 
	 int (*callback)(sqlite3_stmt *stmt, int ncol, const char *format), 
	 const char *format, char **errmsg )
{
  sqlite3_stmt *stmt;
  int erc;
  const char *pend;

  assert(format);
  
  if( (erc = sqlite3_prepare(db, sql, strlen(sql),
			     &stmt, &pend)) != SQLITE_OK ) {
    *errmsg = strdup(sqlite3_errstr(erc));
    return erc;
  }
  
  while( (erc = sqlite3_step(stmt)) == SQLITE_ROW ) {
    const int ncol = sqlite3_column_count(stmt);

    if( 0 != callback(stmt, ncol, format) ) {
      return SQLITE_ABORT;
    }
    format = NULL;
  }

  if( sqlite3_finalize(stmt) != SQLITE_OK ) {
    errx(EXIT_FAILURE, "'%s': %s", __func__, sqlite3_errmsg(db));
  }

  return SQLITE_OK;
}

int
process( const char *dbname, const char *sql,
	 const char *line_length, const char *format )
{
  const static int mode = SQLITE_OPEN_READONLY;
  static const char setup_fmt[] = { ".ll +%si\n" ".TS" };

  char *setup;
  sqlite3 *db;
  char *errmsg;

  if( -1 == asprintf(&setup, setup_fmt, line_length) ) {
    err(EXIT_FAILURE, "%s:%d", __func__, __LINE__);
  }

  if( sqlite3_open_v2(dbname, &db, mode, NULL) != SQLITE_OK ) {
    errx(EXIT_FAILURE, "'%s': %s", dbname, sqlite3_errmsg(db));
  }
  
  printf("%s\n", setup);

  if( execsql(db, sql, report, format, &errmsg) != SQLITE_OK ) {
    errx(EXIT_FAILURE, "%s: %s: '%s'", __func__, errmsg, sql);
  }

  printf(".TE\n");
  printf(".if (\\n(.l < \\n(TW) " // debugging in case table is too wide
	 ".tm line length \\n(.l less than table width \\n(TW "
	 "\n");
  
  if( sqlite3_close(db) != SQLITE_OK ) {
    errx(EXIT_FAILURE, "'%s': %s", dbname, sqlite3_errmsg(db));
  }
  return SQLITE_OK;
}

int
main(int argc, char *argv[])
{
  const char
    *dbname = NULL,
    *sql = NULL,
    *line_length = "6.5",
    *format = "box";
  int opt;
  
  while ((opt = getopt(argc, argv, "d:f:q:w:")) != -1) {
    switch (opt) {
    case 'd':
      dbname = strdup(optarg);
      break;
    case 'f':
      format = strdup(optarg);
      break;
    case 'q':
      sql = strdup(optarg);
      break;
    case 'w':
      line_length = strdup(optarg);
      break;
    default:
      syntax(argv[0]);
    }
  }

  if( !dbname || !sql ) syntax(argv[0]);

  return process( dbname, sql, line_length, format ) ;
}