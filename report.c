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

#include <sqlite3.h>

#include "report.h"

static void
syntax( char name[]  ) {
  errx(EXIT_FAILURE,
       "%s [-f format] [-w width] -d dbname -q query", basename(name));
}

// return true if format contains "box" as a distinct word
static bool
is_box( const char format[] ) {
  const char *p = strstr(format, "box");
  if( !p ) return false;
  if( p == format || !isalnum(p[-1]) ) {
    return !isalnum(p[strlen(p)]);
  }
  return false;
}

static int
report( sqlite3_stmt *stmt, int ncol, const char *formats[] ) {
  const char *sep = "";

  if( formats ) { // first line
    printf("%s\n", formats[0]);
    const char *p = strchr(formats[1], ';');
    if( p == NULL ) {
      printf( "%s;\n", formats[1] ); // table options

      // format of titles
      char heading[2 * ncol];
      memset(heading, 'L', sizeof(heading));
      for( int i=1; i < sizeof(heading); i += 2) { heading[i] = 'B'; }
      printf("%.*s\n", (int)sizeof(heading), heading);

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
	///warnx("%d: %c %s", c, ch, sqlite3_column_name(stmt, c));

	printf( "%s%c", sep, ch );
      }
      printf(" .\n");

      // column headers
      sep = "";
      for( int c=0; c < ncol; c++, sep = "\t" ) {
	printf( "%sT{\n%s\nT}", sep, sqlite3_column_name(stmt, c) );
      }
      printf(is_box(formats[1])? "\n_\n" : "\n");
    } else {
      printf("%.*s\n%s\n", (int)(p - formats[1]), formats[1], p);    }
  }

  sep = "";
  const char *fmt;
  for( int c=0; c < ncol; c++, sep = "\t" ) {
    printf( "%s", sep);
    switch( sqlite3_column_type(stmt, c) ) {
    case SQLITE_INTEGER:
      fmt=column_format(sqlite3_column_name(stmt, c), "%'d");
      printf( fmt, sqlite3_column_int(stmt, c) );
      break;
    case SQLITE_FLOAT:
      fmt=column_format(sqlite3_column_name(stmt, c), "%'f");
      printf( fmt, sqlite3_column_double(stmt, c) );
      break;
    case SQLITE_TEXT:
      printf( "T{\n%s\nT}", (char*)sqlite3_column_text(stmt, c) );
      break;
    case SQLITE_BLOB: default:
      assert(false);
      break;
    case SQLITE_NULL:
      printf( "%s", "NULL" );
    }
  }
  printf("\n");
  return SQLITE_OK;
}

static int
execsql( sqlite3* db, const char *sql, 
	 int (*callback)(sqlite3_stmt *stmt, int ncol, const char *formats[]), 
	 const char *formats[], char **errmsg )
{
  sqlite3_stmt *stmt;
  int erc;
  const char *pend;

  assert(formats);
  
  if( (erc = sqlite3_prepare(db, sql, strlen(sql),
			     &stmt, &pend)) != SQLITE_OK ) {
    *errmsg = strdup(sqlite3_errstr(erc));
    return erc;
  }
  
  while( (erc = sqlite3_step(stmt)) == SQLITE_ROW ) {
    const int ncol = sqlite3_column_count(stmt);

    if( 0 != callback(stmt, ncol, formats) ) {
      return SQLITE_ABORT;
    }
    formats = NULL;
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
  static const char setup_fmt[] = { ".ll +%s%c\n" ".TS" };

  char *setup;
  sqlite3 *db;
  char *errmsg;

  setlocale(LC_ALL, "");

  if( -1 == asprintf(&setup, setup_fmt,
		     line_length, atof(line_length) < 100? 'i' : ' ') ) {
    err(EXIT_FAILURE, "%s:%d", __func__, __LINE__);
  }

  if( sqlite3_open_v2(dbname, &db, mode, NULL) != SQLITE_OK ) {
    errx(EXIT_FAILURE, "'%s': %s", dbname, sqlite3_errmsg(db));
  }
  
  const char *formats[2] = { setup, format };

  if( execsql(db, sql, report, formats, &errmsg) != SQLITE_OK ) {
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

void
set_column_format( const char arg[] ) {
  char input[ 1 + strlen(arg) ];
  strcpy( input, arg );
  char *p = strchr( input, ',' );
  if( !p ) {
    warnx( "could not parse format argument '%s'", input );
    return;
  }

  *p = '\0';
  column_format( strdup(input), strdup(++p) );
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
  
  while ((opt = getopt(argc, argv, "d:f:p:q:w:")) != -1) {
    switch (opt) {
    case 'd':
      dbname = strdup(optarg);
      break;
    case 'f':
      format = strdup(optarg);
      break;
    case 'p':
      set_column_format(optarg);
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
