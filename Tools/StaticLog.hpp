/*
** EntityCore
** C++ Tools - StaticLog
** File description:
** Logs enabled/disabled at compilation time
** Lightest log system for very high verbosity debugging without any impact on release performances
*/

#ifndef STATICLOG_HPP_
#define STATICLOG_HPP_

//#define DEBUG_LOG

#ifdef DEBUG_LOG
#include <iostream>
#define LOG_IN std::cout << " -> " << __FUNCTION__ << std::endl
#define LOG_OUT std::cout << "<-  " << __FUNCTION__ << std::endl
#define LOG_SIN(zone) std::cout << " -: " << #zone << std::endl
#define LOG_SOUT(zone) std::cout << ":-  " << #zone << std::endl
#define LOG_VAR(var) std::cout << " -  " << #var << " = " << var << std::endl
#define LOG(msg) std::cout << " -  " << msg << std::endl
#else
#define LOG_IN
#define LOG_OUT
#define LOG_SIN(zone)
#define LOG_SOUT(zone)
#define LOG_VAR(var)
#define LOG(msg)
#endif

#endif /* !STATICLOG_HPP_ */
