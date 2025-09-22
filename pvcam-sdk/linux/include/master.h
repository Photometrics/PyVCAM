/******************************************************************************/
/* Copyright (C) Teledyne Photometrics. All rights reserved.                  */
/******************************************************************************/

#ifndef _MASTER_H
#define _MASTER_H

/*
This allows us to insert the proper compiler flags, in pvcam.h for example,
to cope properly with C++ definitions.
*/
#if defined(__cplusplus)
  /* BORLAND C++ and GCC */
  #define PV_C_PLUS_PLUS
#elif defined(__cplusplus__)
  /* MICROSOFT C++ */
  #define PV_C_PLUS_PLUS
#endif

/******************************************************************************/
/* Platform-specific defined like calling conventions, etc.                   */
/******************************************************************************/

#if defined(_WIN32) || defined(_WIN64)
  #define PV_DECL __stdcall
  #define DEPRECATED __declspec(deprecated)
#elif defined(__linux__)
  #define PV_DECL
  #define DEPRECATED __attribute__((deprecated))
#elif defined(__APPLE__)
  #define PV_DECL
  #define DEPRECATED __attribute__((deprecated))
#endif

/******************************************************************************/
/* PVCAM types                                                                */
/******************************************************************************/

/** General error codes usually returned from functions as rs_bool value. */
enum
{
    PV_FAIL = 0,
    PV_OK
};

typedef unsigned short rs_bool;
typedef signed char    int8;
typedef unsigned char  uns8;
typedef short          int16;
typedef unsigned short uns16;
typedef int            int32;
typedef unsigned int   uns32;
typedef float          flt32;
typedef double         flt64;

#if defined(_MSC_VER)
  typedef unsigned __int64   ulong64;
  typedef signed   __int64   long64;
#else
  typedef unsigned long long ulong64;
  typedef signed   long long long64;
#endif

/**
@defgroup grp_pm_deprecated Deprecated symbols
*/

/**
@defgroup grp_pm_deprecated_typedefs Deprecated types
@ingroup grp_pm_deprecated
These types are included for compatibility reasons.
@{
*/

#define PV_PTR_DECL     *

typedef void*           void_ptr;
typedef void**          void_ptr_ptr;

typedef rs_bool*        rs_bool_ptr;
typedef char*           char_ptr;
typedef int8*           int8_ptr;
typedef uns8*           uns8_ptr;
typedef int16*          int16_ptr;
typedef uns16*          uns16_ptr;
typedef int32*          int32_ptr;
typedef uns32*          uns32_ptr;
typedef flt32*          flt32_ptr;
typedef flt64*          flt64_ptr;
typedef ulong64*        ulong64_ptr;
typedef long64*         long64_ptr;

typedef const rs_bool*  rs_bool_const_ptr;
typedef const char*     char_const_ptr;
typedef const int8*     int8_const_ptr;
typedef const uns8*     uns8_const_ptr;
typedef const int16*    int16_const_ptr;
typedef const uns16*    uns16_const_ptr;
typedef const int32*    int32_const_ptr;
typedef const uns32*    uns32_const_ptr;
typedef const flt32*    flt32_const_ptr;
typedef const flt64*    flt64_const_ptr;
typedef const ulong64*  ulong64_const_ptr;
typedef const long64*   long64_const_ptr;

/** @} */ /* grp_pm_deprecated_typedefs */

/******************************************************************************/
/* PVCAM constants                                                            */
/******************************************************************************/

#ifndef FALSE
    #define FALSE  PV_FAIL  /* FALSE == 0 */
#endif

#ifndef TRUE
    #define TRUE   PV_OK    /* TRUE == 1 */
#endif

#endif /* _MASTER_H */
