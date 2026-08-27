/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2020 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef THREAD_WIN32_OSX_H_INCLUDED
#define THREAD_WIN32_OSX_H_INCLUDED

#include <thread>

/// On OSX threads other than the main thread are created with a reduced stack
/// size of 512KB by default, this is too low for deep searches, which require
/// somewhat more than 1MB stack, so adjust it to TH_STACK_SIZE.
/// The implementation calls pthread_create() with the stack size parameter
/// equal to the linux 8MB default, on platforms that support it.

#if defined(__APPLE__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(__3DS__)

#include <new>
#include <pthread.h>

#if defined(__3DS__)
// Stockfish's recursive search needs a larger stack than the default 3DS
// homebrew worker thread. Stockfish itself documents that deep searches need
// more than 1 MiB, so use 2 MiB while staying within the Old 3DS budget.
static const size_t TH_STACK_SIZE = 2 * 1024 * 1024;
#else
static const size_t TH_STACK_SIZE = 8 * 1024 * 1024;
#endif

template <class T, class P = std::pair<T*, void(T::*)()>>
void* start_routine(void* ptr)
{
   P* p = reinterpret_cast<P*>(ptr);
   (p->first->*(p->second))(); // Call member function pointer
   delete p;
   return NULL;
}

class NativeThread {

   pthread_t thread{};
   bool started = false;

public:
  template<class T, class P = std::pair<T*, void(T::*)()>>
  explicit NativeThread(void(T::*fun)(), T* obj) {
    P* parameters = new (std::nothrow) P(obj, fun);
    if (!parameters)
        return;

    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        delete parameters;
        return;
    }

    const int stackResult = pthread_attr_setstacksize(&attr, TH_STACK_SIZE);
    const int createResult = stackResult == 0
        ? pthread_create(&thread, &attr, start_routine<T>, parameters)
        : stackResult;
    pthread_attr_destroy(&attr);
    if (createResult == 0)
        started = true;
    else
        delete parameters;
  }
  bool joinable() const { return started; }
  void join() {
    if (!started) return;
    pthread_join(thread, NULL);
    started = false;
  }
};

#else // Default case: use STL classes

typedef std::thread NativeThread;

#endif

#endif // #ifndef THREAD_WIN32_OSX_H_INCLUDED
