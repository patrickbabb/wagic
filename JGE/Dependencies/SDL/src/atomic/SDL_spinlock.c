/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2011 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not
     be misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "SDL_stdinc.h"

#include "SDL_atomic.h"
#include "SDL_mutex.h"
#include "SDL_timer.h"

/* Don't do the check for Visual Studio 2005, it's safe here */
#ifdef __WIN32__
#include "../core/windows/SDL_windows.h"
#endif

/* This function is where all the magic happens... */
SDL_bool SDL_AtomicTryLock(SDL_SpinLock *lock)
{
#if SDL_ATOMIC_DISABLED
    /* Terrible terrible damage */
    static SDL_mutex *_spinlock_mutex;

    if (!_spinlock_mutex) {
        /* Race condition on first lock... */
        _spinlock_mutex = SDL_CreateMutex();
    }
    SDL_mutexP(_spinlock_mutex);
    if (*lock == 0) {
        *lock = 1;
        SDL_mutexV(_spinlock_mutex);
        return SDL_TRUE;
    } else {
        SDL_mutexV(_spinlock_mutex);
        return SDL_FALSE;
    }

#elif defined(_MSC_VER)
    SDL_COMPILE_TIME_ASSERT(locksize, sizeof(*lock) == sizeof(long));
    return (InterlockedExchange((long*)lock, 1) == 0);

#elif defined(__MACOSX__)
    return OSAtomicCompareAndSwap32Barrier(0, 1, lock);

#elif HAVE_GCC_ATOMICS || HAVE_GCC_SYNC_LOCK_TEST_AND_SET
    return (__sync_lock_test_and_set(lock, 1) == 0);

#elif defined(__GNUC__) && defined(__arm__) && \
        (defined(__ARM_ARCH_4__) || defined(__ARM_ARCH_4T__) || \
         defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_5TE__))
    int result;
    __asm__ __volatile__ (
        "swp %0, %1, [%2]\n"
        : "=&r,&r" (result) : "r,0" (1), "r,r" (lock) : "memory");
    return (result == 0);

#elif defined(__GNUC__) && defined(__arm__)
    int result;
    __asm__ __volatile__ (
        "ldrex %0, [%2]\nteq   %0, #0\nstrexeq %0, %1, [%2]"
        : "=&r" (result) : "r" (1), "r" (lock) : "cc", "memory");
    return (result == 0);

#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    int result;
    __asm__ __volatile__(
        "lock ; xchgl %0, (%1)\n"
        : "=r" (result) : "r" (lock), "0" (1) : "cc", "memory");
    return (result == 0);

#else
    /* Fallback implementation using a standard mutex */
    /* You can use SDL_mutex as a fallback if no atomic instructions are available */
    static SDL_mutex *mutex;
    if (!mutex) {
        mutex = SDL_CreateMutex();
    }

    SDL_mutexP(mutex);
    if (*lock == 0) {
        *lock = 1;
        SDL_mutexV(mutex);
        return SDL_TRUE;
    } else {
        SDL_mutexV(mutex);
        return SDL_FALSE;
    }

#endif
}

void SDL_AtomicLock(SDL_SpinLock *lock)
{
    /* FIXME: Should we have an eventual timeout? */
    while (!SDL_AtomicTryLock(lock)) {
        SDL_Delay(0);
    }
}

void SDL_AtomicUnlock(SDL_SpinLock *lock)
{
#if defined(_MSC_VER)
    _ReadWriteBarrier();
    *lock = 0;

#elif HAVE_GCC_ATOMICS || HAVE_GCC_SYNC_LOCK_TEST_AND_SET
    __sync_lock_release(lock);

#else
    *lock = 0;
#endif
}

/* vi: set ts=4 sw=4 expandtab: */
