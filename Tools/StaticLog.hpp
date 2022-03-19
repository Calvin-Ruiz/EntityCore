/*
** EPITECH PROJECT, 2021
** B-CPP-501-MAR-5-1-rtype-calvin.ruiz [WSL: Ubuntu-20.04]
** File description:
** StaticLog
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
