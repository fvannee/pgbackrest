/***********************************************************************************************************************************
File Descriptor Io Read
***********************************************************************************************************************************/
#include "build.auto.h"

#include <sys/select.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/fdRead.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoFdRead
{
    MemContext *memContext;                                         // Object memory context
    const String *name;                                             // File descriptor name for error messages
    int fd;                                                         // File descriptor to read data from
    TimeMSec timeout;                                               // Timeout for read operation
    bool eof;                                                       // Has the end of the stream been reached?
} IoFdRead;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_FD_READ_TYPE                                                                                               \
    IoFdRead *
#define FUNCTION_LOG_IO_FD_READ_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(value, "IoFdRead", buffer, bufferSize)

/***********************************************************************************************************************************
Read data from the file descriptor
***********************************************************************************************************************************/
static size_t
ioFdRead(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(IoFdRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FD_READ, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    ASSERT(!bufFull(buffer));

    ssize_t actualBytes = 0;

    // If blocking keep reading until buffer is full or eof
    if (!this->eof)
    {
        do
        {
            // Initialize the file descriptor set used for select
            fd_set selectSet;
            FD_ZERO(&selectSet);

            // We know the file descriptor is not negative because it passed error handling, so it is safe to cast to unsigned
            FD_SET((unsigned int)this->fd, &selectSet);

            // Initialize timeout struct used for select.  Recreate this structure each time since Linux (at least) will modify it.
            struct timeval timeoutSelect;
            timeoutSelect.tv_sec = (time_t)(this->timeout / MSEC_PER_SEC);
            timeoutSelect.tv_usec = (time_t)(this->timeout % MSEC_PER_SEC * 1000);

            // Determine if there is data to be read
            int result = select(this->fd + 1, &selectSet, NULL, NULL, &timeoutSelect);
            THROW_ON_SYS_ERROR_FMT(result == -1, FileReadError, "unable to select from %s", strZ(this->name));

            // If no data read after time allotted then error
            if (!result)
                THROW_FMT(FileReadError, "unable to read data from %s after %" PRIu64 "ms", strZ(this->name), this->timeout);

            // Read and handle errors
            THROW_ON_SYS_ERROR_FMT(
                (actualBytes = read(this->fd, bufRemainsPtr(buffer), bufRemains(buffer))) == -1, FileReadError,
                "unable to read from %s", strZ(this->name));

            // Update amount of buffer used
            bufUsedInc(buffer, (size_t)actualBytes);

            // If zero bytes were returned then eof
            if (actualBytes == 0)
                this->eof = true;
        }
        while (bufRemains(buffer) > 0 && !this->eof && block);
    }

    FUNCTION_LOG_RETURN(SIZE, (size_t)actualBytes);
}

/***********************************************************************************************************************************
Have all bytes been read from the buffer?
***********************************************************************************************************************************/
static bool
ioFdReadEof(THIS_VOID)
{
    THIS(IoFdRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FD_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(BOOL, this->eof);
}

/***********************************************************************************************************************************
Get file descriptor
***********************************************************************************************************************************/
static int
ioFdReadFd(const THIS_VOID)
{
    THIS(const IoFdRead);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FD_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->fd);
}

/**********************************************************************************************************************************/
IoRead *
ioFdReadNew(const String *name, int fd, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(fd != -1);

    IoRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoFdRead")
    {
        IoFdRead *driver = memNew(sizeof(IoFdRead));

        *driver = (IoFdRead)
        {
            .memContext = memContextCurrent(),
            .name = strDup(name),
            .fd = fd,
            .timeout = timeout,
        };

        this = ioReadNewP(driver, .block = true, .eof = ioFdReadEof, .fd = ioFdReadFd, .read = ioFdRead);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_READ, this);
}
