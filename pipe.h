#ifndef _PIPE_H
#define _PIPE_H 1

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#ifdef ZTS
  #include <TSRM.h>
#endif

#include <php.h>

#define PIPE_VERSION "0.1"
#define PIPE_EXTNAME "pipe"

PHP_FUNCTION(pipe_readwrite);

extern zend_module_entry pipe_module_entry;
#define phpext_pipe_ptr &pipe_module_entry

PHP_MINIT_FUNCTION(pipe);
PHP_MSHUTDOWN_FUNCTION(pipe);
PHP_MINFO_FUNCTION(pipe);

#endif