/**
* @file nstack_glog.ph
*
* Copyright(C), Huawei Tech. Co., Ltd. ALL RIGHTS RESERVED. \n
*
* @brief glog on the nsatck header file, glog on nsatck all external interface.
* @verbatim
   Functional Description: the APIs provide to print
   Target users: nStack
   Use the constraints: internal function call, do not do function check, please ensure that the user's legitimacy
   Upgrade effect: no
@endverbatim
*/
#ifndef __H_NSTACK_GLOG_H__
#define __H_NSTACK_GLOG_H__

#include <stdio.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

/**
* @ingroup  GLOG
* @brief log type.
*
<br/>Function description: Alternate log type for printing.
<br/>use constraint: NA.
<br/>upgrade effect: no.
*/
typedef enum glogEnumPrintType
{
    HLOG_PRINT_TYPE_HOOK = 0,      /**< Hook log*/
    HLOG_PRINT_TYPE_GLOG,           /**< nStack log */

    HLOG_PRINT_TYPE_BUTT,
}glog_printType_e;


/**
* Product maximum length of a single log 63, set a character stored '\ 0'
*/
#define MAX_ROSA_LOG_LEN 64

/**
* nStack single log transition length
*/
#define MAX_NSTACK_LOG_TRANSITIONAL_LEN 256

/**
* nStack The average length of a single log
*/
#define MAX_LOG_TRANSITIONAL_LEN 2048

/**
* nStack maximum length of a single log: 30K
*/
#define MAX_NSTACK_LOG_LENGTH 30720

/**
* nStack Sets the log level environment variable
*/
#define ENV_LOG_LEVEL_ENABLE_FLAG "NSTACK_LOG_LEVEL_ENABLE"

/**
* nStack sets the HOOK log level environment variable
*/
#define NSTACK_LOG_HOOK_LEVEL "NSTACK_LOG_HOOK_LEVEL"

/**
* nStack Sets the environment variable for each log print length
*/
#define ENV_LOG_PRINT_LEN_FLAG "NSTACK_LOG_PRINT_LEN"

/**
* Single log prefix maximum length: 8 log records + 1 separator: +3 log serial number + 1 separator]
*/
#define NSTACK_LOG_PRE_LEN 13

/**
* Send product log count calculation
*/
#define NSTACK_LOG_TRANSFER_TO_PRODUCE_ITEMS(_len) (((_len) -1) / (MAX_ROSA_LOG_LEN - NSTACK_LOG_PRE_LEN) +1)

/**
* @ingroup  glog_atomic
* @brief brief atomic variable structure.
<br/> Functional Description: Definition of atomic variable structure.
<br/> use constraint: NA.
<br/> upgrade effect: no.
*/
typedef struct {
    volatile int counter;
} glog_atomic_t;

/*****************************************************************************
Function name: glogAtomicRead
Function Description: Get the atomic variable value.
Input parameters: v: atomic variable.
Output parameters: NA
Return Value: The atomic variable value.
*****************************************************************************/
static inline int glogAtomicRead(const glog_atomic_t *v)
{
    return v->counter;
}

/*****************************************************************************
Function name: glogAtomicInc
Functional Description: Atomic self-increment.
Input parameters: v: atomic variable.
Output parameters: NA
Return Value: NA
*****************************************************************************/
static inline void glogAtomicInc(glog_atomic_t *v)
{
    (void)__sync_fetch_and_add(&v->counter, 1);
}

/**
* @ingroup GLOG
* @brief glog provides C interface print log.
*
* @verbatim
	 Function Description: Print the log.
	 Target users: LITESERVICE
	 Use the constraint: the user guarantees the correctness of the entry
	 Upgrade effect: no
@endverbatim
* @param type [input] print type
* @param prestr [input] pre string
* @param level [input] real level
* @param count [input] repeat count
* @param fileName [input] file name
* @param fileLine [input] file line number
* @param funcName [input] function name
* @param format [input] accepts arguments
*
* @return NA
* @par dependency:
* <li>nstack_glog.ph£ºThis interface declares the header file.</li></ul>
  * @see NA¡£
* @since V100R002C00
*/

void glog_print(unsigned int           type,
                  const char            *prestr,
                  unsigned int           level,
                  int                    count,
                  char                  *fileName,
                  const unsigned int     fileLine,
                  const char            *funcName,
                  const char            *format,
                  ...);

/*****************************************************************************
Function name: glogInit
Function description: Hlog initialization.
Input parameters: param incoming parameters that can make module ID and so on
Output parameters: NA
Return value: NA
*****************************************************************************/
void glogInit(char *param);

/*****************************************************************************
Function name: glogExit
Function description: Hlog exits
Input parameters: NA
Output parameters: NA
Return value: NA
*****************************************************************************/
void glogExit(void);

/*****************************************************************************
Function name: glogDestinationSet
Function Description: Set different files based on different levels of log basename.
Input parameters: logLevel log level
        base_filename file
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogDestinationSet(unsigned int logLevel, const char* base_filename);

/*****************************************************************************
Function name: n_glogLevelSet
Function Description: Set the log level to be printed.
Enter the parameters: severity log level, see nstack_log_level
Output parameters: NA
Return Value: NA
*****************************************************************************/
void n_glogLevelSet(unsigned int logLevel);

/*****************************************************************************
Function name: glogToStderrSet
Function Description: Set whether the log print standard output, the default need to print the file
Input parameters: logToStderr is valid, 1: print standard output, do not print the file
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogToStderrSet(int logToStderr);

/*****************************************************************************
Function name: glogFlushLogSecsSet
Function Description: Sets the maximum number of seconds which logs may be buffered for.
Enter the parameters: secs seconds
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogFlushLogSecsSet(int secs);

/*****************************************************************************
Function name: glogMaxLogSizeSet
Function Description: Sets the maximum log file size (in MB).
Input parameters: maxSize the maximum log file size (in MB)
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogMaxLogSizeSet(int maxSize);

/*****************************************************************************
Function name: glogLevelGet
Function Description: Get the log print level
Input parameters: NA
Output parameters: NA
Return Value: Success: Log level
               Failed: GLOG_LEVEL_BUTT
*****************************************************************************/
int glogLevelGet(void);

/*****************************************************************************
Function name: glogDir
Function Description: If specified, logfiles are written into this directory instead of
                      the default logging directory
Enter the parameters: logDir: directory
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogDir(char *logDir);

/*****************************************************************************
Function name: glogBufLevelSet
Function Description: When the log level is greater than this level will immediately output, 
                without caching. Buffer log messages logged at this level or lower ,
                -1 means don't buffer; 0 means buffer only.
Enter the parameters: log level, see nstack_log_level,and cannot bigger than GLOG_LEVEL_ERROR.
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogBufLevelSet(unsigned int logLevel);

/*****************************************************************************
 Function name: nstack_modify_log_dir
 Function Description: If specified, modify the glog prit path dynamic.
 Enter the parameters: logDir: directory
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void nstack_modify_log_dir(char *log_dir);

/*****************************************************************************
 Function name: release the lock of glog when fork
 Function Description: release the lock of glog when fork.
 Enter the parameters: NA
 Output parameters: NA
 Return Value: NA
 issue: DTS2017110709083
*****************************************************************************/
void nstack_log_lock_release(void);

/*****************************************************************************
Function name: glogInstallFailureSignalHandler
Function Description: Install a signal handler that will dump signal information and a stack
             trace when the program crashes on certain signals.  We'll install the
             signal handler for the following signals.

             SIGSEGV, SIGILL, SIGFPE, SIGABRT, SIGBUS, and SIGTERM.

             By default, the signal handler will write the failure dump to the
             standard error.  You can customize the destination by installing your
             own writer function by InstallFailureWriter() below.

             Note on threading:

             The function should be called before threads are created, if you want
             to use the failure signal handler for all threads.  The stack trace
             will be shown only for the thread that receives the signal.  In other
             words, stack traces of other threads won't be shown.
Enter the parameters: NA
Output parameters: NA
Return Value:  NA
*****************************************************************************/
void glogInstallFailureSignalHandler(void);

typedef void (*glogFailfuncHook)(void);
/*****************************************************************************
Function name: glogInstallFailureFunction
Function Description: Install a function which will be called after LOG(FATAL).
Enter the parameters: hook: function you want to call after LOG(FATAL).
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogInstallFailureFunction(glogFailfuncHook hook);

typedef void (*glogInstallFailureWriterHook)(const char* data, int size);
/*****************************************************************************
Function name: glogInstallFailureWriter
Function Description: By default, the signal handler writes the failure dump to the standard error.
             You can customize the destination by glogInstallFailureWriter().
Enter the parameters: hook: the signal handler writes the failure dump to the destination you want.
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogInstallFailureWriter(glogInstallFailureWriterHook hook);

/*****************************************************************************
Function name: glogRavage
Functional Description: Segment the NSTACK log to suit the product log mode for easy integration
Enter the file name: fileName
                     fileLine file line number
                     funcName function name
                     fmtString NSTACK prints content
Output Parameters: pLogString: Returns the formatted log
Return Value: NA
*****************************************************************************/
void glogRavage(char              *fileName,
                    const unsigned int fileLine,
                    const char        *funcName,
                    const char        *fmtString,
                    char              *pLogString);

/*****************************************************************************
Function name: glogPrintLengthSet
Function Description: Set the number of characters to print for each log, up to 256
Input parameters: len: character length
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogPrintLengthSet(unsigned int len);

/*****************************************************************************
Function name: glogSetLogSymlink
Function Description: Set the basename of the symlink to the latest log file at a given
              severity. If symlink_basename is empty, do not make a symlink
              you do not call this function, the symlink basename is the
              invocation name of the program. Thread-safe.
Input parameters: level: log level, such as nstack_log_level
              symlink_basename: link basename, NULL is no link
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogSetLogSymlink(unsigned int level, const char* symlink_basename);

/*****************************************************************************
Function name: glogFlushLogFiles
Function Description: Flushes all log files that contains messages that are at least of
             the specified severity level.  Thread-safe.
Input parameters: min_level: log level, such as nstack_log_level
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogFlushLogFiles(unsigned int min_level);

/*****************************************************************************
Function name: glogSetLogFilenameExtension
Function Description: Specify an "extension" added to the filename specified via
             SetLogDestination.  This applies to all severity levels.  It's
             often used to append the port we're listening on to the logfile
             name.  Thread-safe.
Input parameters: filename_extension: extension filename of log file
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogSetLogFilenameExtension(const char* filename_extension);

/**
*  Memory request release registration callback
*/
typedef struct tagGlogAllocator {
  /** Function to allocate memory. */
  void* (*allocCb)(size_t size);
  /** Function to free memory. */
  void (*freeCb)(void *pointer);
} glog_allocator_s;

/*****************************************************************************
Function name: glogMemAllocatorReg
Function Description: Register the memory application function.
Input parameters: allocator: memory request and release interface callback registration
Output parameters: NA
Return value: function returned successfully 0; unsuccessful return -1
*****************************************************************************/
int glogMemAllocatorReg(glog_allocator_s *allocator);

/*****************************************************************************
Function name: glogMalloc
Function Description: Memory application function.
Enter the size of the memory request
Output parameters: NA
Return Value: The function executed successfully to return to the requested memory address; 
              returned NULL without success
*****************************************************************************/
void *glogMalloc(unsigned int size);

/*****************************************************************************
Function name: glogFree
Function Description: Memory release function.
Input parameter: pointer: memory release pointer
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogFree(void *pointer);

/* [DTS2017122008920][2012-12-21] */
void SetGlogCtrlOpt(int opt);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
