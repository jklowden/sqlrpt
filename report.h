struct colfmt_t { char *colname, *fmt; };

const char *
column_format( const char colname[], const char dft[] );
  
#define COUNT_OF(x) (sizeof(x)/sizeof(x[0]))

