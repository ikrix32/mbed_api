/**
 * Copyright 2014 Nordic Semiconductor
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#ifndef __LOG__H__
#define __LOG__H__

/**
 * Warning: remove LOG_LEVEL_* defines in production for major power savings.
 *
 * Defining a log level will set up a serial connection to use for logging.
 * This serial connection sets up a timer prohibiting the mbed chip
 * from entering low power states, drawing ~1.4mAh instead of ~20µAh with logging disabled.
 */

#ifdef LOG_LEVEL_VERBOSE
    #define __PUCK_LOG_LEVEL_VERBOSE__
#endif
#ifdef LOG_LEVEL_DEBUG
    #define __PUCK_LOG_LEVEL_DEBUG__
#endif
#ifdef LOG_LEVEL_INFO
    #define __PUCK_LOG_LEVEL_INFO__
#endif
#ifdef LOG_LEVEL_WARN
    #define __PUCK_LOG_LEVEL_WARN__
#endif
#ifdef LOG_LEVEL_ERROR
    #define __PUCK_LOG_LEVEL_ERROR__
#endif

#ifdef __PUCK_LOG_LEVEL_VERBOSE__
    #define LOG_VERBOSE(fmt, ...) do { logger.printf("[V] "); logger.printf(fmt, ##__VA_ARGS__); } while(0)
    #define __PUCK_LOG_LEVEL_DEBUG__
#else
    #define LOG_VERBOSE(fmt, ...)
#endif

#ifdef __PUCK_LOG_LEVEL_DEBUG__
    #define LOG_DEBUG(fmt, ...) do {  logger.printf("[D] "); logger.printf(fmt, ##__VA_ARGS__); } while(0)
    #define __PUCK_LOG_LEVEL_INFO__
#else
    #define LOG_DEBUG(fmt, ...)
#endif

#ifdef __PUCK_LOG_LEVEL_INFO__
    #define LOG_INFO(fmt, ...) do {  logger.printf("[I] "); logger.printf(fmt, ##__VA_ARGS__); } while(0)
    #define __PUCK_LOG_LEVEL_WARN__
#else
    #define LOG_INFO(fmt, ...)
#endif

#ifdef __PUCK_LOG_LEVEL_WARN__
    #define LOG_WARN(fmt, ...) do {  logger.printf("![W] "); logger.printf(fmt, ##__VA_ARGS__); } while(0)
    #define __PUCK_LOG_LEVEL_ERROR__
#else
    #define LOG_WARN(fmt, ...)
#endif

#ifdef __PUCK_LOG_LEVEL_ERROR__
    #define LOG_ERROR(fmt, ...) do {  logger.printf("!![E] "); logger.printf(fmt, ##__VA_ARGS__); } while(0)
#else
    #define LOG_ERROR(fmt, ...)
#endif

#ifdef __PUCK_LOG_LEVEL_ERROR__
    static Serial logger(USBTX, USBRX);
#endif


#endif //__LOG__H__