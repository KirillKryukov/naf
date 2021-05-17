/*
 * NAF decompressor
 * Copyright (c) 2018-2021 Kirill Kryukov
 * See README.md and LICENSE files of this repository
 */

#ifndef ENNAF_PLATFORM_H
#define ENNAF_PLATFORM_H


#define NDEBUG

#define __USE_MINGW_ANSI_STDIO 1

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __MINGW32__
#include <fcntl.h>
#endif

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"



#if defined(__MINGW32__) || defined(__MINGW64__)
#define HAVE_NO_CHMOD
#define HAVE_NO_CHOWN
#define HAVE_NO_STAT_ST_MTIM_TV_NSEC
#define HAVE_NO_STAT_ST_MTIMENSEC
#define HAVE_NO_FUTIMENS
#define HAVE_NO_FUTIMES
#endif

#ifdef __CYGWIN__
#define HAVE_NO_STAT_ST_MTIMENSEC
#endif



#ifndef HAVE_NO_CHMOD
#define HAVE_CHMOD
#endif

#ifndef HAVE_NO_CHOWN
#define HAVE_CHOWN
#endif



#ifndef HAVE_NO_STAT_ST_MTIM_TV_NSEC
#define HAVE_STAT_ST_MTIM_TV_NSEC
#endif

#ifndef HAVE_NO_STAT_ST_MTIMENSEC
#define HAVE_STAT_ST_MTIMENSEC
#endif

#ifndef HAVE_NO_FUTIMENS
#define HAVE_FUTIMENS
#endif

#ifndef HAVE_NO_FUTIMES
#define HAVE_FUTIMES
#endif

#ifndef HAVE_NO_UTIME
#define HAVE_UTIME
#endif


#if defined(HAVE_FUTIMENS)
#include <sys/stat.h>
#elif defined(HAVE_FUTIMES)
#include <sys/time.h>
#elif defined(HAVE_UTIME)
#include <sys/types.h>
#include <utime.h>
#endif

#if defined(__APPLE__)
#define A_TIME_SEC(fs)  ((fs).st_atimespec.tv_sec)
#define M_TIME_SEC(fs)  ((fs).st_mtimespec.tv_sec)
#define A_TIME_NSEC(fs) ((fs).st_atimespec.tv_nsec)
#define M_TIME_NSEC(fs) ((fs).st_mtimespec.tv_nsec)
#elif defined(HAVE_STAT_ST_MTIM_TV_NSEC)
#define A_TIME_SEC(fs)  ((fs).st_atim.tv_sec)
#define M_TIME_SEC(fs)  ((fs).st_mtim.tv_sec)
#define A_TIME_NSEC(fs) ((fs).st_atim.tv_nsec)
#define M_TIME_NSEC(fs) ((fs).st_mtim.tv_nsec)
#elif defined(HAVE_STAT_ST_MTIMENSEC)
#define A_TIME_SEC(fs)  ((fs).st_atime)
#define M_TIME_SEC(fs)  ((fs).st_mtime)
#define A_TIME_NSEC(fs) ((fs).st_atimensec)
#define M_TIME_NSEC(fs) ((fs).st_mtimensec)
#else
#define A_TIME_SEC(fs)  ((fs).st_atime)
#define M_TIME_SEC(fs)  ((fs).st_mtime)
#define A_TIME_NSEC(fs) (0)
#define M_TIME_NSEC(fs) (0)
#endif


#endif
