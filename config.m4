PHP_ARG_ENABLE(pipe, whether to enable pipe extension support, 
  [--enable-pipe Enable pipe extension support])

if test $PHP_PIPE != "no"; then
    PHP_NEW_EXTENSION(pipe, pipe.c, $ext_shared)
fi
