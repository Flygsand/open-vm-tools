/*********************************************************
 * Copyright (C) 2003-2010 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*********************************************************
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of VMware Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission of VMware Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *********************************************************/

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * vm_basic_defs.h --
 *
 *	Standard macros for VMware source code.
 */

#ifndef _VM_BASIC_DEFS_H_
#define _VM_BASIC_DEFS_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMIROM
#include "includeCheck.h"
#include "vm_basic_types.h" // For INLINE.

/* Checks for FreeBSD, filtering out VMKERNEL. */
#define __IS_FREEBSD__ (!defined(VMKERNEL) && defined(__FreeBSD__))
#define __IS_FREEBSD_VER__(ver) (__IS_FREEBSD__ && __FreeBSD_version >= (ver))

#if defined _WIN32 && defined USERLEVEL
   #include <stddef.h>  /*
                         * We redefine offsetof macro from stddef; make 
                         * sure that it's already defined before we do that.
                         */
   #include <windows.h>	// for Sleep() and LOWORD() etc.
   #undef GetFreeSpace  // Unpollute preprocessor namespace.
#endif


/*
 * Simple macros
 */

#if (defined __APPLE__ || defined __FreeBSD__) && \
    (!defined KERNEL && !defined _KERNEL && !defined VMKERNEL && !defined __KERNEL__)
#   include <stddef.h>
#else
// XXX the _WIN32 one matches that of VC++, to prevent redefinition warning
// XXX the other one matches that of gcc3.3.3/glibc2.2.4 to prevent redefinition warnings
#ifndef offsetof
#ifdef _WIN32
#define offsetof(s,m)   (size_t)&(((s *)0)->m)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#endif
#endif // __APPLE__

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof (a) / sizeof *(a))
#endif

#ifndef MIN
#define MIN(_a, _b)   (((_a) < (_b)) ? (_a) : (_b))
#endif

/* The Solaris 9 cross-compiler complains about these not being used */
#ifndef sun
static INLINE int 
Min(int a, int b)
{
   return a < b ? a : b;
}
#endif

#ifndef MAX
#define MAX(_a, _b)   (((_a) > (_b)) ? (_a) : (_b))
#endif

#ifndef sun
static INLINE int 
Max(int a, int b)
{
   return a > b ? a : b;
}
#endif

#define VMW_CLAMP(x, min, max) \
   ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

#define ROUNDUP(x,y)		(((x) + (y) - 1) / (y) * (y))
#define ROUNDDOWN(x,y)		((x) / (y) * (y))
#define ROUNDUPBITS(x, bits)	(((uintptr_t) (x) + MASK(bits)) & ~MASK(bits))
#define ROUNDDOWNBITS(x, bits)	((uintptr_t) (x) & ~MASK(bits))
#define CEILING(x, y)		(((x) + (y) - 1) / (y))
#if defined __APPLE__
#include <machine/param.h>
#undef MASK
#endif

/*
 * The MASK macro behaves badly when given negative numbers or numbers larger
 * than the highest order bit number (e.g. 32 on a 32-bit machine) as an
 * argument. The range 0..31 is safe.
 */

#define MASK(n)		((1 << (n)) - 1)	    /* make an n-bit mask */
#define MASK64(n)	((CONST64U(1) << (n)) - 1)  /* make an n-bit mask */
/*
 * MASKRANGE64 makes a bit vector starting at bit lo and ending at bit hi.  No
 * checking for lo < hi is done.
 */
#define MASKRANGE64(hi, lo)      (MASK64((hi) - (lo) + 1) << (lo))

/* SIGNEXT64 sign extends a n-bit value to 64-bits. */
#define SIGNEXT64(val, n)       (((int64)(val) << (64 - (n))) >> (64 - (n)))

#define DWORD_ALIGN(x)          ((((x) + 3) >> 2) << 2)
#define QWORD_ALIGN(x)          ((((x) + 7) >> 3) << 3)

#define IMPLIES(a,b) (!(a) || (b))

/*
 * Not everybody (e.g., the monitor) has NULL
 */

#ifndef NULL
#ifdef  __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif


/* 
 * Token concatenation
 *
 * The C preprocessor doesn't prescan arguments when they are
 * concatenated or stringified.  So we need extra levels of
 * indirection to convince the preprocessor to expand its
 * arguments.
 */

#define CONC(x, y)              x##y
#define XCONC(x, y)             CONC(x, y)
#define XXCONC(x, y)            XCONC(x, y)
#define MAKESTR(x)              #x
#define XSTR(x)                 MAKESTR(x)


/*
 * Wide versions of string constants.
 */

#define WSTR_(X)     L ## X
#define WSTR(X)      WSTR_(X)


/*
 * Page operations
 *
 * It has been suggested that these definitions belong elsewhere
 * (like x86types.h).  However, I deem them common enough
 * (since even regular user-level programs may want to do
 * page-based memory manipulation) to be here.
 * -- edward
 */

#ifndef PAGE_SHIFT // {
#if defined VM_I386
   #define PAGE_SHIFT    12
#elif defined __APPLE__
   #define PAGE_SHIFT    12
#elif defined __arm__
   #define PAGE_SHIFT    12
#else
   #error
#endif
#endif // }

#ifndef PAGE_SIZE
#define PAGE_SIZE     (1<<PAGE_SHIFT)
#endif

#ifndef PAGE_MASK
#define PAGE_MASK     (PAGE_SIZE - 1)
#endif

#ifndef PAGE_OFFSET
#define PAGE_OFFSET(_addr)  ((uintptr_t)(_addr)&(PAGE_SIZE-1))
#endif

#ifndef VM_PAGE_BASE
#define VM_PAGE_BASE(_addr)  ((_addr)&~(PAGE_SIZE-1))
#endif

#ifndef VM_PAGES_SPANNED
#define VM_PAGES_SPANNED(_addr, _size) \
   ((((_addr) & (PAGE_SIZE - 1)) + (_size) + (PAGE_SIZE - 1)) >> PAGE_SHIFT)
#endif

#ifndef BYTES_2_PAGES
#define BYTES_2_PAGES(_nbytes)  ((_nbytes) >> PAGE_SHIFT)
#endif

#ifndef PAGES_2_BYTES
#define PAGES_2_BYTES(_npages)  (((uint64)(_npages)) << PAGE_SHIFT)
#endif

#ifndef MBYTES_2_PAGES
#define MBYTES_2_PAGES(_nbytes) ((_nbytes) << (20 - PAGE_SHIFT))
#endif

#ifndef PAGES_2_MBYTES
#define PAGES_2_MBYTES(_npages) ((_npages) >> (20 - PAGE_SHIFT))
#endif

#ifndef BYTES_2_MBYTES
#define BYTES_2_MBYTES(_nbytes) ((_nbytes) >> 20)
#endif

#ifndef MBYTES_2_BYTES
#define MBYTES_2_BYTES(_nbytes) ((uint64)(_nbytes) << 20)
#endif

#ifndef VM_PAE_LARGE_PAGE_SHIFT
#define VM_PAE_LARGE_PAGE_SHIFT 21
#endif 

#ifndef VM_PAE_LARGE_PAGE_SIZE
#define VM_PAE_LARGE_PAGE_SIZE (1 << VM_PAE_LARGE_PAGE_SHIFT)
#endif

#ifndef VM_PAE_LARGE_PAGE_MASK
#define VM_PAE_LARGE_PAGE_MASK (VM_PAE_LARGE_PAGE_SIZE - 1)
#endif

#ifndef VM_PAE_LARGE_2_SMALL_PAGES
#define VM_PAE_LARGE_2_SMALL_PAGES (BYTES_2_PAGES(VM_PAE_LARGE_PAGE_SIZE))
#endif

#ifndef NR_MPNS_PER_PAGE
#define NR_MPNS_PER_PAGE (PAGE_SIZE / sizeof(MPN))
#endif

/*
 * Word operations
 */

#ifndef LOWORD
#define LOWORD(_dw)   ((_dw) & 0xffff)
#endif
#ifndef HIWORD
#define HIWORD(_dw)   (((_dw) >> 16) & 0xffff)
#endif

#ifndef LOBYTE
#define LOBYTE(_w)    ((_w) & 0xff)
#endif
#ifndef HIBYTE
#define HIBYTE(_w)    (((_w) >> 8) & 0xff)
#endif

#define HIDWORD(_qw)   ((uint32)((_qw) >> 32))
#define LODWORD(_qw)   ((uint32)(_qw))
#define QWORD(_hi, _lo)   ((((uint64)(_hi)) << 32) | ((uint32)(_lo)))


/*
 * Deposit a field _src at _pos bits from the right,
 * with a length of _len, into the integer _target.
 */

#define DEPOSIT_BITS(_src,_pos,_len,_target) { \
	unsigned mask = ((1 << _len) - 1); \
	unsigned shiftedmask = ((1 << _len) - 1) << _pos; \
	_target = (_target & ~shiftedmask) | ((_src & mask) << _pos); \
}


/*
 * Get return address.
 */

#ifdef _MSC_VER
#ifdef __cplusplus
extern "C"
#endif 
void *_ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)
#define GetReturnAddress() _ReturnAddress()
#elif __GNUC__
#define GetReturnAddress() __builtin_return_address(0)
#endif


#ifdef __GNUC__
#ifndef sun

static INLINE_SINGLE_CALLER uintptr_t
GetFrameAddr(void)
{
   uintptr_t bp;
#if  !(__GNUC__ == 4 && (__GNUC_MINOR__ == 0 || __GNUC_MINOR__ == 1))
   bp = (uintptr_t)__builtin_frame_address(0);
#else
   /*
    * We use this assembly hack due to a bug discovered in gcc 4.1.1.
    * The bug was fixed in 4.2.0; assume it originated with 4.0.
    * PR147638, PR554369.
    */
     __asm__ __volatile__(
#  if defined(VM_X86_64)
                          "movq %%rbp, %0\n"
#  else
                          "movl %%ebp, %0\n"
#  endif
                          : "=g" (bp));
#endif
   return bp;
}


/*
 * Returns the frame pointer of the calling function.
 * Equivalent to __builtin_frame_address(1).
 */
static INLINE_SINGLE_CALLER uintptr_t
GetCallerFrameAddr(void)
{
   return *(uintptr_t*)GetFrameAddr();
}

#endif // sun
#endif // __GNUC__

/*
 * Data prefetch was added in gcc 3.1.1
 * http://www.gnu.org/software/gcc/gcc-3.1/changes.html
 */
#ifdef __GNUC__
#  if ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ > 1) || \
       (__GNUC__ == 3 && __GNUC_MINOR__ == 1 && __GNUC_PATCHLEVEL__ >= 1))
#     define PREFETCH_R(var) __builtin_prefetch((var), 0 /* read */, \
                                                3 /* high temporal locality */)
#     define PREFETCH_W(var) __builtin_prefetch((var), 1 /* write */, \
                                                3 /* high temporal locality */)
#  else
#     define PREFETCH_R(var) ((void)(var))
#     define PREFETCH_W(var) ((void)(var))
#  endif
#endif /* __GNUC__ */


#ifdef USERLEVEL // {

/*
 * Note this might be a problem on NT b/c while sched_yield guarantees it
 * moves you to the end of your priority list, Sleep(0) offers no such
 * guarantee.  Bummer.  --Jeremy.
 */

#if defined(N_PLAT_NLM)
/* We do not have YIELD() as we do not need it yet... */
#elif defined(_WIN32)
#      define YIELD()		Sleep(0)
#else
#      include <sched.h>        // For sched_yield.  Don't ask.  --Jeremy.
#      define YIELD()		sched_yield()
#endif 


/*
 * Standardize some Posix names on Windows.
 */

#ifdef _WIN32 // {

#define snprintf  _snprintf
#define strtok_r  strtok_s

#if (_MSC_VER < 1500)
#define	vsnprintf _vsnprintf
#endif

typedef int uid_t;
typedef int gid_t;

static INLINE void
sleep(unsigned int sec)
{
   Sleep(sec * 1000);
}

static INLINE int
usleep(unsigned long usec)
{
   Sleep(CEILING(usec, 1000));

   return 0;
}

typedef int pid_t;
#define       F_OK          0
#define       X_OK          1
#define       W_OK          2
#define       R_OK          4

#endif // }

/*
 * Macro for username comparison.
 */

#ifdef _WIN32 // {
#define USERCMP(x,y)  Str_Strcasecmp(x,y)
#else
#define USERCMP(x,y)  strcmp(x,y)
#endif // }


#endif // }

#ifndef va_copy

#ifdef _WIN32

/*
 * Windows needs va_copy. This works for both 32 and 64-bit Windows
 * based on inspection of how varags.h from the Visual C CRTL is
 * implemented. (Future versions of the RTL may break this).
 */

#define va_copy(dest, src) ((dest) = (src))

#elif defined(__APPLE__) && defined(KERNEL)

#include "availabilityMacOS.h"

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
// The Mac OS 10.5 kernel SDK defines va_copy in stdarg.h.
#include <stdarg.h>
#else
/*
 * The Mac OS 10.4 kernel SDK needs va_copy. Based on inspection of
 * stdarg.h from the MacOSX10.4u.sdk kernel framework, this should
 * work.
 */
#define va_copy(dest, src) ((dest) = (src))
#endif // MAC_OS_X_VERSION_MIN_REQUIRED

#elif defined(__GNUC__) && (__GNUC__ < 3)

/*
 * Old versions of gcc recognize __va_copy, but not va_copy.
 */

#define va_copy(dest, src) __va_copy(dest, src)

#endif // _WIN32

#endif // va_copy

/*
 * This one is outside USERLEVEL because it's used by
 * files compiled into the Windows hgfs driver or the display
 * driver.
 */

#ifdef _WIN32
#define PATH_MAX 256
#ifndef strcasecmp
#define strcasecmp(_s1,_s2)   _stricmp((_s1),(_s2))
#endif
#ifndef strncasecmp
#define strncasecmp(_s1,_s2,_n)   _strnicmp((_s1),(_s2),(_n))
#endif
#endif

#if defined __linux__ && !defined __KERNEL__ && !defined MODULE && \
                         !defined VMM && !defined FROBOS && !defined __ANDROID__
#include <features.h>
#if __GLIBC_PREREQ(2, 1) && !defined GLIBC_VERSION_21
#define GLIBC_VERSION_21
#endif
#if __GLIBC_PREREQ(2, 2) && !defined GLIBC_VERSION_22
#define GLIBC_VERSION_22
#endif
#if __GLIBC_PREREQ(2, 3) && !defined GLIBC_VERSION_23
#define GLIBC_VERSION_23
#endif
#if __GLIBC_PREREQ(2, 4) && !defined GLIBC_VERSION_24
#define GLIBC_VERSION_24
#endif
#if __GLIBC_PREREQ(2, 5) && !defined GLIBC_VERSION_25
#define GLIBC_VERSION_25
#endif
#endif

/*
 * Convenience macros and definitions. Can often be used instead of #ifdef.
 */

#undef DEBUG_ONLY
#ifdef VMX86_DEBUG
#define vmx86_debug      1
#define DEBUG_ONLY(x)    x
#else
#define vmx86_debug      0
#define DEBUG_ONLY(x)
#endif

#ifdef VMX86_STATS
#define vmx86_stats   1
#define STATS_ONLY(x) x
#else
#define vmx86_stats   0
#define STATS_ONLY(x)
#endif

#ifdef VMX86_DEVEL
#define vmx86_devel   1
#define DEVEL_ONLY(x) x
#else
#define vmx86_devel   0
#define DEVEL_ONLY(x)
#endif

#ifdef VMX86_LOG
#define vmx86_log     1
#define LOG_ONLY(x)   x
#else
#define vmx86_log     0
#define LOG_ONLY(x)
#endif

#ifdef VMX86_BETA
#define vmx86_beta     1
#define BETA_ONLY(x)   x
#else
#define vmx86_beta     0
#define BETA_ONLY(x)
#endif

#ifdef VMX86_RELEASE
#define vmx86_release   1
#define RELEASE_ONLY(x) x
#else
#define vmx86_release   0
#define RELEASE_ONLY(x) 
#endif

#ifdef VMX86_SERVER
#define vmx86_server 1
#define SERVER_ONLY(x) x
#define HOSTED_ONLY(x)
#else
#define vmx86_server 0
#define SERVER_ONLY(x)
#define HOSTED_ONLY(x) x
#endif

#ifdef VMX86_WGS
#define vmx86_wgs 1
#define WGS_ONLY(x) x
#else
#define vmx86_wgs 0
#define WGS_ONLY(x) 
#endif

#ifdef VMKERNEL
#define vmkernel 1
#define VMKERNEL_ONLY(x) x
#else
#define vmkernel 0
#define VMKERNEL_ONLY(x)
#endif

#ifdef _WIN32
#define WIN32_ONLY(x) x
#define POSIX_ONLY(x)
#define vmx86_win32 1
#else
#define WIN32_ONLY(x)
#define POSIX_ONLY(x) x
#define vmx86_win32 0
#endif

#ifdef __linux__
#define LINUX_ONLY(x) x
#else
#define LINUX_ONLY(x)
#endif

#ifdef __APPLE__
#define vmx86_apple 1
#define APPLE_ONLY(x) x
#else
#define vmx86_apple 0
#define APPLE_ONLY(x) 
#endif

#ifdef VMM
#define VMM_ONLY(x) x
#define USER_ONLY(x)
#else
#define VMM_ONLY(x)
#define USER_ONLY(x) x
#endif

/* VMVISOR ifdef only allowed in the vmkernel */
#ifdef VMKERNEL
#ifdef VMVISOR
#define vmvisor 1
#define VMVISOR_ONLY(x) x
#else
#define vmvisor 0
#define VMVISOR_ONLY(x)
#endif
#endif

#ifdef _WIN32
#define VMW_INVALID_HANDLE INVALID_HANDLE_VALUE
#else
#define VMW_INVALID_HANDLE (-1LL)
#endif

#ifdef _WIN32
#define fsync(fd) _commit(fd)
#define fileno(f) _fileno(f)
#else
#endif

/*
 * Debug output macros for Windows drivers (the Eng variant is for
 * display/printer drivers only.
 */
#ifdef _WIN32
#ifndef USES_OLD_WINDDK
#if defined(VMX86_LOG)
#define WinDrvPrint(arg, ...) DbgPrint(arg, __VA_ARGS__)
#define WinDrvEngPrint(arg, ...) EngDbgPrint(arg, __VA_ARGS__)
#else
#define WinDrvPrint(arg, ...)
#define WinDrvEngPrint(arg, ...)
#endif
#endif
#endif // _WIN32

#ifdef HOSTED_LG_PG
#define hosted_lg_pg 1
#else
#define hosted_lg_pg 0
#endif

/*
 * Use to initialize cbSize for this structure to preserve < Vista
 * compatibility.
 */
#define NONCLIENTMETRICSINFO_V1_SIZE CCSIZEOF_STRUCT(NONCLIENTMETRICS, \
                                                     lfMessageFont)

/* This is not intended to be thread-safe. */
#define DO_ONCE(code)                                                   \
   do {                                                                 \
      static Bool _doOnceDone = FALSE;                                  \
      if (UNLIKELY(!_doOnceDone)) {                                     \
         _doOnceDone = TRUE;                                            \
         code;                                                          \
      }                                                                 \
   } while (0)

/*
 * Bug 827422 and 838523.
 */

#if defined __GNUC__ && __GNUC__ >= 4
#define VISIBILITY_HIDDEN __attribute__((visibility("hidden")))
#else
#define VISIBILITY_HIDDEN /* nothing */
#endif

#endif // ifndef _VM_BASIC_DEFS_H_
