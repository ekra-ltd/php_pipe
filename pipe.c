#include "pipe.h"

#if COMPILE_DL_PIPE
ZEND_GET_MODULE(pipe)
#endif

#if PHP_MAJOR_VERSION >= 8
#define TSRMLS_CC
#endif

static zend_function_entry pipe_functions[] = {
    PHP_FE(pipe_readwrite, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry pipe_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PIPE_EXTNAME,
    pipe_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    PHP_MINFO(pipe),
#if ZEND_MODULE_API_NO >= 20010901
    PIPE_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

PHP_MINFO_FUNCTION(pipe) {}

BOOL SafeReadFromPipe(
  const HANDLE    hFile,                       ///< Читаемый pipe
  char*           lpBuffer,                    ///< Буфер для чтения
  const DWORD     nNumberOfBytesToRead,        ///< Количество байт для чтения
  const DWORD     timeoutSeconds,              ///< Таймаут выполнения операции
  const zend_bool enableNotice                 ///< Разрешить выдачу отладочных сообщений
) ;

BOOL SafeWriteToPipe(
  const HANDLE    hFile,                       ///< pipe, в который выполняется запись
  const char*     lpBuffer,                    ///< Буфер с данными для записи
  const DWORD     nNumberOfBytesToWrite,       ///< Количество байт для записи
  const DWORD     timeoutSeconds,              ///< Таймаут записи
  const zend_bool enableNotice                 ///< Разрешить выдачу отладочных сообщений
) ;

PHP_FUNCTION(pipe_readwrite) {
    const DWORD HeaderSize = 16;
    const DWORD MaxChunkSize = 2048;
    
    char*       pipeName;
    size_t      pipeNameSize;
    char*       bytes;
    size_t      bytesSize;
    zend_bool   withResponse;
    zend_long   timeoutSecodns;
    zend_bool   enableNotice;
    
    
    if ( zend_parse_parameters(
            ZEND_NUM_ARGS(),
            "ssblb",
            &pipeName,
            &pipeNameSize,
            &bytes,
            &bytesSize,
            &withResponse,
            &timeoutSecodns,
            &enableNotice) == FAILURE)
    {
        RETURN_FALSE;
    }
    
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "input parameters. pipeName: '%s' (len=%i), data [...] (len = %i), withResponse=%i, timeout=%i", 
            pipeName, pipeNameSize,
            bytesSize,
            (int)withResponse,
            (int)timeoutSecodns);
    }
    
    if (timeoutSecodns <= 0) 
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "timeoutSecodns must be > 0");
        RETURN_FALSE;
    }
    
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "begin open pipe '%s'...", pipeName);
    }
    HANDLE handle = CreateFile(
        pipeName, 
        GENERIC_READ | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES, // Read and write access
        0,                                                      // No sharing
        NULL,                                                   // Default security attributes
        OPEN_EXISTING,                                          // Open existing pipe
        FILE_FLAG_OVERLAPPED,                                   // Asynchronous mode
        NULL                                                    // No template file
    );
    if ( handle == INVALID_HANDLE_VALUE )
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "couldn't open pipe file '%s', error:'%i'", pipeName, GetLastError());
        RETURN_FALSE;
    }
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "pipe opened successfull");
    }
    
    DWORD mode = PIPE_NOWAIT;
    if ( !SetNamedPipeHandleState(
            handle, 
            &mode,
            NULL,
            NULL) )
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "SetNamedPipeHandleState error:'%i'", GetLastError());
        CloseHandle(handle);
        RETURN_FALSE;
    }
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "pipe set state successfull");
    }

    if ( !SafeWriteToPipe(handle, bytes, (DWORD)bytesSize, (DWORD)timeoutSecodns, enableNotice) )
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "write to file, error:'%i'", GetLastError());
        CloseHandle(handle);
        RETURN_FALSE;
    }
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "pipe writed successfull");
    }
    
    char* rcvHeader = NULL;
    zend_string * packetResultString = NULL;
    if ( withResponse ) 
    {
        if ( enableNotice )
        {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "with response == true - reading header");
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "reading header (16 bytes)...");    
        }
        rcvHeader = calloc(HeaderSize, sizeof(char));
        if ( !rcvHeader )
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "couldn't allocate memory to packet header");
            CloseHandle(handle);
            RETURN_FALSE;
        }
        
        DWORD readedTotal = HeaderSize;
        if ( !SafeReadFromPipe(handle, rcvHeader, readedTotal, (DWORD)timeoutSecodns, enableNotice) )
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "read from file, error:'%i'", GetLastError());
            // free resources
            if ( rcvHeader )
            {
                free( rcvHeader );
                rcvHeader = NULL;
            }
            CloseHandle(handle);
            // end free resources
            RETURN_FALSE;
        }
        if ( enableNotice )
        {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "readed header (16 bytes):%i-%i-%i-%i %i-%i-%i-%i %i-%i-%i-%i %i-%i-%i-%i",
                (unsigned char)rcvHeader[ 0], (unsigned char)rcvHeader[ 1], (unsigned char)rcvHeader[ 2], (unsigned char)rcvHeader[3],
                (unsigned char)rcvHeader[ 4], (unsigned char)rcvHeader[ 3], (unsigned char)rcvHeader[ 6], (unsigned char)rcvHeader[7],
                (unsigned char)rcvHeader[ 8], (unsigned char)rcvHeader[ 7], (unsigned char)rcvHeader[ 8], (unsigned char)rcvHeader[9],
                (unsigned char)rcvHeader[12], (unsigned char)rcvHeader[13], (unsigned char)rcvHeader[14], (unsigned char)rcvHeader[15]
            );    
        }
        
        DWORD packetLength = 
            ((unsigned char)(rcvHeader[12]) << 24) |
            ((unsigned char)(rcvHeader[13]) << 16) |
            ((unsigned char)(rcvHeader[14]) <<  8) |
            ((unsigned char)(rcvHeader[15]) <<  0) ;
        
        if ( enableNotice )
        {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "packet length is %i bytes", packetLength);    
        }
        
        char* rcvPacketBytes = calloc(HeaderSize + packetLength, sizeof(char) );
        if ( !rcvPacketBytes )
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "couldn't allocate memory to packet data");
            // free resources
            if ( rcvHeader )
            {
                free( rcvHeader );
                rcvHeader = NULL;
            }
            CloseHandle(handle);
            // end free resources
            RETURN_FALSE;
        }
        memcpy(rcvPacketBytes, rcvHeader, HeaderSize);
        if ( rcvHeader )
        {
            free(rcvHeader);
            rcvHeader = NULL;
        }
        
        // В исходном файле было чтение блоками по 2048 байт
        const DWORD expectedSize = HeaderSize + packetLength;
        while ( readedTotal != expectedSize )
        {
            const DWORD chunkSize = (expectedSize - readedTotal > MaxChunkSize) ? MaxChunkSize : expectedSize - readedTotal;
            if ( !SafeReadFromPipe( handle, &rcvPacketBytes[readedTotal], chunkSize, (DWORD)timeoutSecodns, enableNotice) )
            {
                php_error_docref(NULL TSRMLS_CC, E_WARNING, "read from file, error:'%i'", GetLastError());
                // free resources
                if ( rcvPacketBytes )
                {
                    free( rcvPacketBytes );
                    rcvPacketBytes = NULL;
                }
                CloseHandle(handle);
                // end free resources
                RETURN_FALSE;    
            }
            readedTotal += chunkSize;
        }
        
        // Формирование ответа
        packetResultString = zend_string_init(rcvPacketBytes, expectedSize, 0);
        if ( rcvPacketBytes )
        {
            free(rcvPacketBytes);
            rcvPacketBytes = NULL;
        }
        
        if ( enableNotice )
        {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "readed %i bytes total", packetLength);
        }
    }
    else
    {
        if ( enableNotice )
        {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "with response == false");
        }
    }
    
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "read-write operation successfull");
    }
    
    CloseHandle(handle);
    
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "CloseHandle called");
    }
    
    if ( packetResultString )
    {
        RETURN_STR(packetResultString);
    }
    else
    {
        packetResultString = zend_string_init("", 0, 0);
        RETURN_STR(packetResultString);
    }
}

BOOL SafeReadFromPipe(
  const HANDLE       hFile,                       ///< Читаемый pipe
        char*        lpBuffer,                    ///< Буфер для чтения
  const DWORD        nNumberOfBytesToRead,        ///< Количество байт для чтения
  const DWORD        timeoutSeconds,              ///< Таймаут выполнения операции
  const zend_bool    enableNotice                 ///< Разрешить выдачу отладочных сообщений
) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER startingTime;
    LARGE_INTEGER endingTime;
    
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&startingTime);
    
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "begin reading %i bytes...", nNumberOfBytesToRead);
    }

    DWORD readedTotal = 0;
    while(readedTotal != nNumberOfBytesToRead)
    {
        DWORD readed = 0;
        if ( !ReadFile(
                hFile,
                &lpBuffer[readedTotal],
                nNumberOfBytesToRead - readedTotal,
                &readed,
                NULL))
        {
            if ( ERROR_NO_DATA == GetLastError() )
            {
                QueryPerformanceCounter(&endingTime);
                if ( (timeoutSeconds * 1000) < ((endingTime.QuadPart - startingTime.QuadPart) * 1000 / frequency.QuadPart) )
                {
                    SetLastError(ERROR_NO_DATA);
                    return 0;
                }
                Sleep(10);
                continue;
            }
            return 0;
        }
        readedTotal += readed;
    }
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "readed %i bytes...", nNumberOfBytesToRead);
    }
    return 1;
}

BOOL SafeWriteToPipe(
  const HANDLE       hFile,                       ///< pipe, в который выполняется запись
  const char*        lpBuffer,                    ///< Буфер с данными длязаписи
  const DWORD        nNumberOfBytesToWrite,       ///< Количество байт для записи
  const DWORD        timeoutSeconds,              ///< Таймаут записи
  const zend_bool    enableNotice                 ///< Разрешить выдачу отладочных сообщений
) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER startingTime;
    LARGE_INTEGER endingTime;
    
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&startingTime);
    
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "begin writing %i bytes...", nNumberOfBytesToWrite);
    }
    
    DWORD writtenTotal = 0;
    while(writtenTotal != nNumberOfBytesToWrite)
    {
        DWORD written = 0;
        if ( !WriteFile(
                hFile,
                &lpBuffer[writtenTotal],
                nNumberOfBytesToWrite - writtenTotal,
                &written,
                NULL))
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "write to file, error:'%i'", GetLastError());
            return 0;
        }
        writtenTotal += written;
        
        // https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile
        // When writing to a non-blocking, byte-mode pipe handle with insufficient buffer space, WriteFile returns TRUE 
        // with *lpNumberOfBytesWritten < nNumberOfBytesToWrite.
        if ( written == 0 )
        {
            QueryPerformanceCounter(&endingTime);
            if ( (timeoutSeconds * 1000) < ((endingTime.QuadPart - startingTime.QuadPart) * 1000 / frequency.QuadPart) )
            {
                return 0;
            }
            Sleep(10);
        }
    }
    if ( enableNotice )
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "writed %i bytes...", nNumberOfBytesToWrite);
    }
    return 1;
}