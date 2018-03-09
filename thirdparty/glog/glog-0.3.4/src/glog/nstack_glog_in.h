/**
* @file nstack_glog_in.h
*
* Copyright(C), Huawei Tech. Co., Ltd. ALL RIGHTS RESERVED. \n
*
* @brief glog header file outside
* @verbatim
   function Description: user define glog print
   user: Custom print log
   Use constraints: NA
   Upgrade effects: no
@endverbatim
*/
#ifndef __NSTACK_GLOG_IN_H__
#define __NSTACK_GLOG_IN_H__

#include <stdio.h>

/**@defgroup GLOG */


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

/**
* @ingroup  GLOG
* @brief print point
*
<br/> function description: dot log.
<br/> use constraints: NA.
<br/> upgrade effects: no.
*/
#define LOG_CODE 1U

/**
* @ingroup  GLOG
* @brief log level
*
<br/> function description: log level.
<br/> use constraints: NA.
<br/> upgrade effects: no.

*/
typedef enum tagLogLevel
{
    GLOG_LEVEL_DEBUG    = 0,   /**< Debug*/
    GLOG_LEVEL_INFO     = 1,   /**< INFO,google_ns::INFO */
    GLOG_LEVEL_WARNING  = 2,   /**< WARNING,google_ns::WARNING */
    GLOG_LEVEL_ERROR    = 3,   /**< ERROR,google_ns::ERROR */
    GLOG_LEVEL_FATAL    = 4,   /**< FATAL,google_ns::FATAL */

    GLOG_LEVEL_BUTT
} nstack_log_level;


/**
* @ingroup  GLOG
* @brief ROSA LOG HOOk
*
* @verbatim
   Function Description: Register the hook for LiteService log printing.
   Target users: users who use the custom print logs
   Use the constraints: the need for users to achieve log printing, dotted by the ROSA unified allocation, the iteration only passed the fixed value LOG_CODE.
   Upgrade effect: no
   
@endverbatim
* @param  logCode    [input] log RBI ID, such as ROSA LOGCode, reserved
* @param  logLevel   [input] log level
* @param  format     [input] log format
* @param  data       [input] parameter
*
* @return NA
* @par  dependency: product registration
* <li>nstack_glog_in.h£ºThe interface declares the header file</li></ul>
  * @see NA¡£
* @since V100R002C00
*/
typedef void (*nstack_log_hook)(unsigned int logCode,
                                    unsigned int logLevel,
                                    void        *format,
                                    void        *data);

/**
* @ingroup  GLOG
* @brief from the process of subscribing to hook functions.
*
<br/> Function description: The hook function issued from the process status information, the user implements and registers the SF function.
<br/> use constraint: NA.
<br/> upgrade effect: no.
*/
typedef struct log_hook_tag {
    nstack_log_hook   log_hook; /**< Custom log callback interface*/
    unsigned int    register_cnt; /**< Number of registrations to prevent multiple registrations */
    unsigned int    level;
}log_hook_tag_t;

extern log_hook_tag_t g_log_hook_tag;


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
* @since V100R002C00
*/
int nstack_log_hook_set(nstack_log_hook log_hook);

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
* @see NA¡£
* @since V100R002C00
*/
int nstack_log_hook_level_set(unsigned int log_level);


/**
* @ingroup  NSTACK_LOG£¬init the hook 

* @see NA¡£
* @since V100R002C00
*/

void nstack_log_hook_init();


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
* @see NA¡£
* @since V100R002C00
*/
void nstack_log_count_set(unsigned int count);


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
* @param  logLevel [input] log level, see nstack_log_level
*
* @return 0 The function returns success
* @return 1 parameter passed in error: the rank is not in range
* @par dependency:
* <li>nstack_glog_in.h£ºThe interface declares the header file.</li></ul>
  * @see NA¡£
* @since V100R002C00
*/
int glogLevelSet(unsigned int logLevel);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
