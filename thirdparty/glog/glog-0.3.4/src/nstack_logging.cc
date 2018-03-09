/*
* @file nstack_logging.cc
* Copyright(C), Huawei Tech. Co., Ltd. ALL RIGHTS RESERVED. \n
* Description: user define the log. \n
*
*/
#include <stdarg.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <iostream>
#include <istream>
#include <streambuf>
#include <string>

#include "glog/logging.h"
#include "glog/log_severity.h"
#include "glog/raw_logging.h"
#include "glog/stl_logging.h"
#include "glog/vlog_is_on.h"
#include "utilities.h"
#include "glog/nstack_glog_in.h"
#include "glog/nstack_glog.ph"
#include "glog/nstack_adapter.ph"

#ifdef  __cplusplus
#if  __cplusplus
extern "C" {
#endif
#endif

using namespace std;
using namespace google_ns;
using namespace nstack_space;

/**
* Max length of signal log 
* Prefix: LogNum:LogSerial] max is NSTACK_LOG_PRE_LEN , most character is MAX_NSTACK_LOG_LENGTH-1
* reference MAX_ROSA_LOG_LEN, need MAX_NSTACK_LOG_LENGTH % (MAX_ROSA_LOG_LEN - NSTACK_LOG_PRE_LEN) can store the log
*/
#define MAX_LOG_SERIAL_NUM  NSTACK_LOG_TRANSFER_TO_PRODUCE_ITEMS(MAX_NSTACK_LOG_LENGTH)

/**
* Log record number, atomic operation
*/
static glog_atomic_t g_logRecordNum = {0};

/**
* A signed 32-bit maximum
*/
static const int max32BitNum = 0x7FFFFFFF;
static const int MAX_32BIT_NUM_HEX_LEN = 8;

/**
* Log messages at a level >= this flag are automatically sent to stderr in addition to log files.
*/
unsigned int g_LogLevelPrint = GLOG_LEVEL_BUTT;

/**
* @ingroup  NSTACK_LOG
* @brief Set the log file number..
<br/> Functional Description: Set the log file number.
<br/> use constraint: The default is 10.
<br/> upgrade effect: no.
*/
extern unsigned int log_file_count;
extern unsigned int modify_path_count;

/**
* Log messages length
*/
static unsigned int g_LogLenPrint = 0;

/**
* @ingroup GLOG
* @brief   Save user-defined callback interface.
<br/>  Function Description: Save the user-defined callback interface.
<br/>  use constraint: NA.
<br/>  upgrade effect: no.
*/
log_hook_tag_t g_log_hook_tag = {NULL, 0, GLOG_LEVEL_INFO};

/**
* @ingroup GLOG
* @brief   ROSA callback registration judgment.
<br/>  Function description: Determines whether ROSA is registered.
<br/>  use constraint: NA.
<br/>  upgrade effect: no.
*/

#define NSTACK_LOG_VARLEN_CHECK (g_log_hook_tag.log_hook != NULL)

/**
* @ingroup GLOG
* @brief  HLOG and GLOG log level mapping.
<br/>  Function description: Log level mapping.
<br/>  use constraint: NA.
<br/>  upgrade effect: no.
*/
static const int glogLevelMapping[GLOG_LEVEL_BUTT]
    = {GLOG_INFO,
       GLOG_INFO,
       GLOG_WARNING,
       GLOG_ERROR,
       GLOG_FATAL};

/**
* @ingroup GLOG
* @brief   ROSA log level in env
<br/>  use constraint: NA.
<br/>  upgrade effect: no.
*/
static const char* const glogLevelName[GLOG_LEVEL_BUTT] = {"DBG",
                                                     "INF",
                                                     "WAR",
                                                     "ERR",
                                                     "FTL"};
/**
* @ingroup  NSTACK_LOG
* @brief product to NSTACK registration log hook.
*
* @verbatim
   Function Description: Set the log to print the hook.
   Target users: users who use NSTACK to print logs
   Use the constraint: log_hook parameter can not be empty, repeat the registration to the last time.
   Upgrade effect: no

@endverbatim
* @param   log_hook [input] ROSA registered to NSTACK callback function, can not be empty
*
* @return 0 The function returns success
* @return 1 parameter passed in error: empty, or re-registered
* @par dependency: Requires product call
* <li>nstack_glog_in.h The interface declares the header file. </li></ul>
* @see NA¡£
*/

int nstack_log_hook_set(nstack_log_hook log_hook)
{
#ifndef MPTCP_SUPPORT
    if (NULL == log_hook || g_log_hook_tag.register_cnt > 0) {
        return 1;
    }
#endif

    g_log_hook_tag.log_hook = log_hook;
    g_log_hook_tag.register_cnt++;

    return 0;
}

/**
* @ingroup  NSTACK_LOG
* @brief Set the log level to be printed, and logs greater than or equal to this level will be printed.
*
* @verbatim
   Function Description: Set the log level to be printed.Support for reading environment variables: 
   NSTACK_LOG_HOOK_LEVEL = DBG / INF / WAR / ERR / FTL,If the value of NSTACK_LOG_HOOK_LEVEL is set
   incorrectly, the log level is unchanged Dynamically set the level
   Target users: users who use NSTACK to print logs
   Use constraint: log level See nstack_log_level.
                   First set the priority level with NSTACK_LOG_HOOK_LEVEL,
                   If the NSTACK_LOG_HOOK_LEVEL environment variable is not set,
                   And then to nstack_log_hook_level_set incoming log_level prevail,
                   The default is INFO level.
                   Note the use of environment variable NSTACK_LOG_HOOK_LEVEL settings and clear. 
   Upgrade effect: no
@endverbatim
* @param  log_level [input] log level, see enum nstack_log_level
*
* @return 0 The function returns success
* @return 1  parameter passed in error: level is not in the range, before the log level unchanged
* @par dependency:NA
* @see NA
*/


int nstack_log_hook_level_set(unsigned int log_level)
{
    if (log_level >= GLOG_LEVEL_BUTT)
    {
        return 1;
    }

    /* accept the log_level set by nstack_log_hook_level_set().
       Otherwise, ignore the log_level.
    */
    g_log_hook_tag.level = log_level;
    
    return 0;
}

/**
* @ingroup  NSTACK_LOG£¬init the hook 

* @see NA
*/

void nstack_log_hook_init()
{
    unsigned int i;
    char *pst_temp = NULL;

    pst_temp = getenv(NSTACK_LOG_HOOK_LEVEL);
    if (NULL == pst_temp) {
        return ;
    }

    for(i = 0; i < GLOG_LEVEL_BUTT; i++) {
        if ((glogLevelName[i][0] == pst_temp[0])
         && (0 == strncmp(pst_temp,
                          glogLevelName[i],
                          strlen(glogLevelName[i])))) {
            g_log_hook_tag.level = i;
            return;
        }
    }

    return ;

}

/**
* @ingroup  NSTACK_LOG
* @brief Set the log file number.
*
* @verbatim
   Function Description: Set the log file number. 
   Target users: users who use NSTACK to print logs
   Use constraint: The default is 10
   Upgrade effect: no
@endverbatim
* @param  count [input] log file number,
*
* @return NA
* @par dependency:NA
*/
void nstack_log_count_set(unsigned int count)
{
    log_file_count = count;
}


/**
* @ingroup  GLOG
* @brief Set the log level to be printed, and logs greater than or equal to this level will be printed.
*
* @verbatim
   Function Description: Set the log level to be printed.
                         Support for reading environment variables NSTACK_LOG_LEVEL_ENABLE = DEBUG / INFO / WARNING / ERROR / FATAL,
                         Dynamically set the level
   Target users: users who use the NSTACK software to print logs
 Use constraint: log level See nstack_log_level, which must be called when NSTACK_LOG_LEVEL_ENABLE is used.
                 First to NSTACK_LOG_LEVEL_ENABLE set the level prevail, again with the incoming logLevel prevail,
                 The default is INFO level. Please note that the environment variable NSTACK_LOG_LEVEL_ENABLE settings and clear.
 Upgrade effect: no
@endverbatim
* @param logLevel [input] log level, see nstack_log_level
*
* @return 0 The function returns success
* @return 1 parameter passed in error: the rank is not in range
* @par dependency:
* <li>nstack_glog_in.h£ºThe interface declares the header file. </li></ul>
*/
int glogLevelSet(unsigned int logLevel)
{
    int levelEnable;

    if (logLevel >= GLOG_LEVEL_BUTT) {
        return 1;
    }

    levelEnable = glogLevelGet();
    g_LogLevelPrint = (GLOG_LEVEL_BUTT == levelEnable) ? logLevel : levelEnable;


    n_glogLevelSet(g_LogLevelPrint);

    return 0;
}

/*****************************************************************************
 Function name: glogPrintLengthSet
 Function Description: Set the number of characters to print for each log, up to 256
 Input parameters: len: character length
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void glogPrintLengthSet(unsigned int len)
{
    char *envPrintLen = NULL;
    unsigned int envLen = 0;

    if (0 == len) {
        return;
    }

    envPrintLen = getenv(ENV_LOG_PRINT_LEN_FLAG);
    if (NULL == envPrintLen) {
        g_LogLenPrint = len > MAX_NSTACK_LOG_LENGTH ? MAX_NSTACK_LOG_LENGTH : len;
        return;
    }

    envLen = (unsigned int)atoi(envPrintLen);
    if (envLen > 0) {
        g_LogLenPrint = envLen > MAX_NSTACK_LOG_LENGTH ? MAX_NSTACK_LOG_LENGTH : envLen;
        return;
    }

    return;
}

/*****************************************************************************
 Function name: glogLevelGet
 Function Description: Get the log print level
 Input parameters: NA
 Output parameters: NA
 Return Value: Success: Log level
                Failed: GLOG_LEVEL_BUTT
*****************************************************************************/
int glogLevelGet(void)
{
    unsigned int i;
    char *enableLevel = NULL;

    enableLevel = getenv(ENV_LOG_LEVEL_ENABLE_FLAG);
    if (NULL == enableLevel) {
        return GLOG_LEVEL_BUTT;
    }

    for(i = 0; i < GLOG_LEVEL_BUTT; i++) {
        if ((glogLevelName[i][0] == enableLevel[0])
         && (0 == strncmp(enableLevel,
                          glogLevelName[i],
                          strlen(glogLevelName[i])))) {
            return i;
        }
    }

    return GLOG_LEVEL_BUTT;

}

/*****************************************************************************
 Function name: glogInit
 Function description: Hlog initialization.
 Input parameters: param incoming parameters that can make module ID and so on
 Output parameters: NA
 Return value: NA
*****************************************************************************/
void glogInit(char *param)
{
    InitGoogleLogging(param);

    return ;
}

/*****************************************************************************
 Function name: glogExit
 Function description: Hlog exits
 Input parameters: NA
 Output parameters: NA
 Return value: NA
*****************************************************************************/
void glogExit(void)
{
    ShutdownGoogleLogging();
}

/*****************************************************************************
Function name: glogDestinationSet
Function Description: Set different files based on different levels of log basename.
Input parameters: logLevel log level
        base_filename file
Output parameters: NA
Return Value: NA
*****************************************************************************/
void glogDestinationSet(unsigned int logLevel, const char* base_filename)
{
    if (logLevel >= GLOG_LEVEL_BUTT) {
        return;
    }
    SetLogDestination(glogLevelMapping[logLevel], base_filename);
}

/**
* @ingroup  GLOG
* @brief Set the log level to be printed.
*
* @verbatim
    Function Description: Set the log level to be printed.
    Target users: users who use the glog software to print logs
    Use constraint: log level See nstack_log_level
    Upgrade effect: no

@endverbatim
* @param logLevel [input] log level, see nstack_log_level
*
* @return NA
* @par dependency:
* <li>nstack_glog.ph£º This interface declares the header file. </li></ul>
*/
void n_glogLevelSet(unsigned int logLevel)
{
    FLAGS_minloglevel = glogLevelMapping[logLevel];
}

/*****************************************************************************
 Function name: glogToStderrSet
 Function Description: Set whether the log print standard output, the default need to print the file
 Input parameters: logToStderr is valid, 1: print standard output, do not print the file
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void glogToStderrSet(int logToStderr)
{
    FLAGS_logtostderr = (1 == logToStderr) ? true : false;
}

/*****************************************************************************
 Function name: glogFlushLogSecsSet
 Function Description: Sets the maximum number of seconds which logs may be buffered for.
 Enter the parameters: secs seconds
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void glogFlushLogSecsSet(int secs)
{
    FLAGS_logbufsecs = secs;
}

/*****************************************************************************
 Function name: glogMaxLogSizeSet
 Function Description: Sets the maximum log file size (in MB).
 Input parameters: maxSize the maximum log file size (in MB)
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void glogMaxLogSizeSet(int maxSize)
{
    FLAGS_max_log_size = maxSize;
}


/*****************************************************************************
 Function name: glogDir
 Function Description: If specified, logfiles are written into this directory instead of
                       the default logging directory
 Enter the parameters: logDir: directory
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void glogDir(char *logDir)
{
    FLAGS_log_dir = logDir;
}

/**
* @ingroup  GLOG
* @brief Set the log level to be printed.
*
* @verbatim
    Function Description: When the log level is greater than this level will immediately output, 
                without caching. Buffer log messages logged at this level or lower ,
    Target users: users who use the glog software to print logs
    Use constraint: log level See nstack_log_level, and cannot bigger than GLOG_LEVEL_ERROR.
    Upgrade effect: no

@endverbatim
* @param logLevel [input] log level, see nstack_log_level
*
* DTS2017110310251
* @return NA
* @par dependency:
* <li>nstack_glog.ph£º This interface declares the header file. </li></ul>
*/
void glogBufLevelSet(unsigned int logLevel)
{
    if(logLevel > GLOG_LEVEL_ERROR)
    {
        return;
    }

    FLAGS_logbuflevel = glogLevelMapping[logLevel];
}

/*****************************************************************************
 Function name: nstack_modify_log_dir
 Function Description: If specified, modify the glog prit path dynamic.
 Enter the parameters: logDir: directory
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void nstack_modify_log_dir(char *log_dir)
{
    if(NULL == log_dir)
        return;
    FLAGS_log_dir = log_dir;
    modify_path_count = 1;
    return;
}

/*****************************************************************************
 Function name: release the lock of glog when fork
 Function Description: release the lock of glog when fork.
 Enter the parameters: NA
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void nstack_log_lock_release(void)
{
    glog_release_rw_lock();
    RleaseLogSinkLock(GLOG_INFO);
    return;
}

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
void glogInstallFailureSignalHandler(void)
{
    InstallFailureSignalHandler();
}

/*****************************************************************************
 Function name: glogInstallFailureWriter
 Function Description: By default, the signal handler writes the failure dump to the standard error.
              You can customize the destination by glogInstallFailureWriter().
 Enter the parameters: hook: the signal handler writes the failure dump to the destination you want.
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void glogInstallFailureWriter(glogInstallFailureWriterHook hook)
{
    InstallFailureWriter(hook);
}


/*****************************************************************************
 Function name: glogInstallFailureFunction
 Function Description: Install a function which will be called after LOG(FATAL).
 Enter the parameters: hook: function you want to call after LOG(FATAL).
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void glogInstallFailureFunction(glogFailfuncHook hook)
{
    InstallFailureFunction(hook);
}

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
void glogSetLogSymlink(unsigned int level, const char* symlink_basename)
{
    SetLogSymlink(glogLevelMapping[level], symlink_basename);
}

/*****************************************************************************
 Function name: glogFlushLogFiles
 Function Description: Flushes all log files that contains messages that are at least of
              the specified severity level.  Thread-safe.
 Input parameters: min_level: log level, such as nstack_log_level
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void glogFlushLogFiles(unsigned int min_level)
{
    FlushLogFiles(glogLevelMapping[min_level]);
}

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
void glogSetLogFilenameExtension(const char* filename_extension)
{
    SetLogFilenameExtension(filename_extension);
}

/**
* Define HlogAllocator type of global variables, initialized to the system malloc, free
*/
void* glogDefaultMalloc(size_t size)
{
    if (0 == size) {
        return NULL;
    }

    return malloc(size);
}

/**
* Define HlogAllocator type of global variables, initialized to the system malloc, free
*/
void glogDefaultFree(void *pointer)
{
    if (NULL != pointer) {
        free(pointer);
    }
}

/**
* Defines a global variable of type HlogAllocator
*/
glog_allocator_s glogMemAllocator = {
    glogDefaultMalloc,
    glogDefaultFree
};

/**
* Whether it has been registered
*/
bool glogMemAllocatorReged = false;

/*****************************************************************************
 Function name: glogMemAllocatorReg
 Function Description: Register the memory application function.
 Input parameters: allocator: memory request and release interface callback registration
 Output parameters: NA
 Return value: function returned successfully 0; unsuccessful return -1
*****************************************************************************/
int glogMemAllocatorReg(glog_allocator_s *allocator)
{
    if (false != glogMemAllocatorReged) {
        return -1;
    }

    if (NULL == allocator) {
        return -1;
    }

    if ((NULL == allocator->allocCb) || (NULL == allocator->freeCb)) {
        return -1;
    }

    glogMemAllocatorReged = true;
    glogMemAllocator.allocCb = allocator->allocCb;
    glogMemAllocator.freeCb = allocator->freeCb;
    return 0;
}

/*****************************************************************************
Function name: glogMalloc
Function Description: Memory application function.
Enter the size of the memory request
Output parameters: NA
Return Value: The function executed successfully to return to the requested memory address; 
              returned NULL without success
*****************************************************************************/

void *glogMalloc(unsigned int size)
{
    return glogMemAllocator.allocCb(size);
}

/*****************************************************************************
 Function name: glogFree
 Function Description: Memory release function.
 Input parameter: pointer: memory release pointer
 Output parameters: NA
 Return Value: NA
*****************************************************************************/
void glogFree(void *pointer)
{
    if (NULL != pointer) {
        glogMemAllocator.freeCb(pointer);

    }
}

/*****************************************************************************
 Function name: glogRecordNumGet
 Function Description: Generates a log record number to distinguish the log from the combination
 Input parameters: NA
 Output parameters: NA
 Return value: return logging number
*****************************************************************************/
int glogRecordNumGet(void)
{
    int ret = glogAtomicRead(&g_logRecordNum) & max32BitNum;
    glogAtomicInc(&g_logRecordNum);
    return ret;
}

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
                char              *pLogString)
{
    int  logNumLen                    = 0;
    int  len                          = strlen(fmtString);
    int  items                        = NSTACK_LOG_TRANSFER_TO_PRODUCE_ITEMS(len);
    char tmpMsgBuff[MAX_ROSA_LOG_LEN +1] = "\0";
    int  logNum                       = 0;
    int  j                            = 0;

    logNum    = glogRecordNumGet();
    logNumLen = snprintf(tmpMsgBuff,
                      MAX_32BIT_NUM_HEX_LEN +1,
                     "%x",
                      logNum);

    for (int i= 0; i < items; i++) {
        int item = i * MAX_ROSA_LOG_LEN;
        int k = snprintf(tmpMsgBuff +logNumLen,
                         MAX_ROSA_LOG_LEN -logNumLen,
                         ":%x]",
                         i);

        snprintf(pLogString +item,
                 MAX_ROSA_LOG_LEN +1,
                 "%s%s",
                 tmpMsgBuff,
                 fmtString +j);

        j += MAX_ROSA_LOG_LEN -logNumLen -k;
        if (j >= len) {
            break;
        }
    }

    return;
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
*/

void glog_print(unsigned int           type,
                  const char            *prestr,
                  unsigned int           level,
                  int                    count,
                  char                  *fileName,
                  const unsigned int     fileLine,
                  const char            *funcName,
                  const char            *format,
                  ...)
{
    va_list ap;
    char    pMsgBuf[MAX_LOG_TRANSITIONAL_LEN] = "\0";
    char   *pRavageBuf = NULL;
    char    premodule[MAX_ROSA_LOG_LEN] = "\0";
    char    baseFile[MAX_ROSA_LOG_LEN +1] = "\0";
    int     buffLen = 0;
    int     prebuffLen = 0;
    int     print_type = 0;

    /**
    * default is GLOG_LEVEL_INFO
    */
    if((type > GLOG_LEVEL_FATAL) || (level > GLOG_LEVEL_FATAL)){
        return;
    }
    if (GLOG_LEVEL_BUTT == g_LogLevelPrint) {
        (void)glogLevelSet(GLOG_LEVEL_INFO);
    }

    if (NULL != strrchr(fileName, '/')) {
        snprintf(baseFile, MAX_ROSA_LOG_LEN, "%s", strrchr(fileName, '/') + 1);
    } else {
        snprintf(baseFile, MAX_ROSA_LOG_LEN, "%s", fileName);
    }

    if((NULL != prestr) && (0 != strlen(prestr))){
        if(count < 0){
            snprintf(premodule, MAX_ROSA_LOG_LEN - 1, "<%s>", prestr);
        }
        else{
            snprintf(premodule, MAX_ROSA_LOG_LEN - 1, " %d <%s>", count, prestr);
        }
    }


    print_type = (g_log_hook_tag.log_hook != NULL) ? HLOG_PRINT_TYPE_HOOK : HLOG_PRINT_TYPE_GLOG;
    if (HLOG_PRINT_TYPE_GLOG != print_type ) {

        if(level < g_log_hook_tag.level){
            return;
        }
        /**
        * log premessage
        */
        prebuffLen = snprintf(pMsgBuf,
                              MAX_LOG_TRANSITIONAL_LEN,
                              "%ld %s:%d]%d,%s]",
                              syscall(__NR_gettid),
                              baseFile,
                              fileLine,
                              getpid(),
                              funcName);
        if (prebuffLen <= 0) {
            printf("ERROR: get prefix log failed]level=%d,fileName=%s,fileLine=%d,funcName=%s,msg=%s\n",
                    level, baseFile, fileLine, funcName, pMsgBuf);
            return;
        }
        prebuffLen = (prebuffLen >= MAX_LOG_TRANSITIONAL_LEN) ? (MAX_LOG_TRANSITIONAL_LEN - 1) : prebuffLen;
    }

    /**
    * log message
    */
    va_start(ap, format);
    buffLen = vsnprintf(pMsgBuf +prebuffLen,
                        MAX_LOG_TRANSITIONAL_LEN -prebuffLen,
                        format,
                        ap);
    if (buffLen <= 0) {
        printf("ERROR: get message failed]level=%d,fileName=%s,fileLine=%d,funcName=%s,msg=%s\n",
                level, baseFile, fileLine, funcName, pMsgBuf);
        va_end(ap);
        return;
    }
    va_end(ap);
    buffLen = (buffLen +prebuffLen >= MAX_LOG_TRANSITIONAL_LEN) ? (MAX_LOG_TRANSITIONAL_LEN -prebuffLen -1) : buffLen;

    switch (print_type) {
        
        case HLOG_PRINT_TYPE_HOOK: {
            int ravageBufLen =  NSTACK_LOG_TRANSFER_TO_PRODUCE_ITEMS(prebuffLen +buffLen) * MAX_ROSA_LOG_LEN  +1;
            pRavageBuf = (char *)glogMalloc(ravageBufLen);
            if (NULL == pRavageBuf) {
                printf("ERROR: malloc failed]level=%d,fileName=%s,fileLine=%d,funcName=%s,msg=%s\n",
                        level, baseFile, fileLine, funcName, pMsgBuf);
                return;
            }
            memset(pRavageBuf, 0, ravageBufLen);

            glogRavage(baseFile, fileLine, funcName, pMsgBuf, pRavageBuf);
            g_log_hook_tag.log_hook(LOG_CODE,
                                    level,
                                   (void*)"%s",
                                   (void*)pRavageBuf);
            glogFree(pRavageBuf);
            break;
        }

        default: {
            LogMessage(baseFile,
                       fileLine,
                       glogLevelMapping[type],
                       level).stream()
                       << getpid()
                       << ","
                       << funcName
                       << premodule
                       << pMsgBuf;
            break;
        }
    }

    return;
}


// overload new: use ram management of user
void *LogMessageAdapter::LogRamMgnt::operator new(size_t size)
{
    if (0 == size) {
        throw bad_alloc();
    }
    void *mem = glogMalloc(size);
    if (mem != NULL) {
        return mem;
    } else {
        throw bad_alloc();
    }
}

// overload delete: use ram management of user
void LogMessageAdapter::LogRamMgnt::operator delete(void *ptr)
{
    if (ptr != NULL) {
        glogFree(ptr);
    }
}

LogMessageAdapter::LogMsgAdpData::~LogMsgAdpData()
{
    delete pMsgBuff;
    pMsgBuff = NULL;
    delete stream_alloc_;
    stream_alloc_ = NULL;
}

// mapping glog level to glog level
static const unsigned int HLogMapping[NUM_SEVERITIES] =
    {GLOG_LEVEL_INFO, GLOG_LEVEL_WARNING, GLOG_LEVEL_ERROR, GLOG_LEVEL_FATAL};

LogMessageAdapter::LogMessageAdapter(const char *file,
                                     int         line,
                                     const char *funcName,
                                     int         logLevel)
{
    /**
    * must be here before return, if not you will be happy to cool
    */
    allocated = NULL;
    data = NULL;

    /**
    * default is GLOG_LEVEL_INFO
    */
    if (GLOG_LEVEL_BUTT == g_LogLevelPrint) {
        int safe_minloglevel = 0;
        if (FLAGS_minloglevel > 0) {
            safe_minloglevel = (FLAGS_minloglevel >= NUM_SEVERITIES) ? (NUM_SEVERITIES -1) : FLAGS_minloglevel;
        }
        (void)glogLevelSet(HLogMapping[safe_minloglevel]);
    }

    if ((HLogMapping[logLevel] < g_LogLevelPrint) && (GLOG_LEVEL_BUTT != g_LogLevelPrint)) {
        return;
    }

    if (0 == g_LogLenPrint) {
        glogPrintLengthSet(MAX_LOG_TRANSITIONAL_LEN);
    }

    try {
        allocated = new LogMsgAdpData();
        data = allocated;

        data->pMsgBuff = NULL;
        data->pMsgBuff = new char[g_LogLenPrint +1];
        memset(data->pMsgBuff, 0, g_LogLenPrint +1);

        data->stream_alloc_ = NULL;
        data->stream_ = NULL;
        data->stream_alloc_ = new LogStream(data->pMsgBuff, g_LogLenPrint, 0);
        data->stream_ = data->stream_alloc_;

        if (NULL != strrchr(const_cast<char*>(file), '/')) {
            snprintf(data->fileName, MAX_ROSA_LOG_LEN, "%s", strrchr(const_cast<char*>(file), '/') + 1);
        } else {
            snprintf(data->fileName, MAX_ROSA_LOG_LEN, "%s", const_cast<char*>(file));
        }

        snprintf(data->func, MAX_ROSA_LOG_LEN, "%s", const_cast<char*>(funcName));

        data->line = line;
        data->level = logLevel;

        stream().fill('0');
    } catch (exception& e) {
        std::cerr << "malloc failed]file=" << file <<",line=" << line
                  << ",funcName" << funcName << ",exception=" << e.what() << endl;
        return;
    }

}

LogMessageAdapter::~LogMessageAdapter()
{
    if (NULL == data) {
        return;
    }

    unsigned int printType = (g_log_hook_tag.log_hook != NULL) ? HLOG_PRINT_TYPE_HOOK : HLOG_PRINT_TYPE_GLOG;
    if (HLOG_PRINT_TYPE_GLOG != printType) {
        glog_print(GLOG_LEVEL_INFO,
                  "GLOG",
                  HLogMapping[data->level],
                  -1,
                  data->fileName,
                  data->line,
                  data->func,
                  "%s",
                  data->pMsgBuff);
    } else {
        LogMessage(data->fileName,
                   data->line,
                   GLOG_INFO,
                   (data->level + 1)).stream() << getpid() << "," << data->func << "(GLOG) " << data->pMsgBuff;
    }


   delete allocated;
   allocated = NULL;
}

static ostream glogNullStream(NULL);
ostream& LogMessageAdapter::stream()
{
    if((NULL == data) || (NULL == data->stream_)) {
        return glogNullStream; 
    }

    return *(data->stream_); 
}


void SetGlogCtrlOpt(int opt)
{
  glogCtrlOpt = opt;
}

#ifdef  __cplusplus
#if  __cplusplus
}
#endif
#endif
