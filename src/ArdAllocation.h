// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

// Test builds can replace only ArdPortal allocations without intercepting the SDK.
#ifdef ARDPORTAL_TEST_ALLOCATE
extern void* ARDPORTAL_TEST_ALLOCATE(size_t bytes) noexcept;
#endif
namespace ArdAllocation {
/**
 * @brief Allocate raw storage without invoking throwing operator new.
 * @param bytes Number of bytes requested.
 * @return Allocated storage, or nullptr when memory is unavailable.
 */
inline void* allocate(size_t bytes) noexcept {
#ifdef ARDPORTAL_TEST_ALLOCATE
  return ARDPORTAL_TEST_ALLOCATE(bytes);
#else
  return std::malloc(bytes);
#endif
}
}

// Class-local operators bypass embedded toolchains whose global nothrow new
// delegates to throwing new and terminates when exceptions are disabled.
#define ARDPORTAL_NO_THROW_ALLOCATION \
  static void* operator new(size_t bytes, const std::nothrow_t&) noexcept { return ArdAllocation::allocate(bytes); } \
  static void operator delete(void* p) noexcept { std::free(p); } \
  static void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); } \
  static void* operator new[](size_t bytes, const std::nothrow_t&) noexcept { return ArdAllocation::allocate(bytes); } \
  static void operator delete[](void* p) noexcept { std::free(p); } \
  static void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }

namespace ArdAllocation {
/**
 * @brief Destroy a placement-constructed object and release its malloc storage.
 * @param p Object pointer; nullptr is allowed.
 * @return No value.
 */
template<class T> struct Deleter {
  /**
   * @brief Destroy the owned object and free its placement-allocation buffer.
   * @param p Object to destroy; nullptr is allowed.
   * @return No value.
   */
  void operator()(T* p) const noexcept { if(p){p->~T();std::free(p);} }
};
template<class T> using Pointer=std::unique_ptr<T,Deleter<T>>;
/**
 * @brief Construct a foreign-library object without invoking global operator new.
 * @param args Constructor arguments forwarded to the object.
 * @return Object pointer, or nullptr on allocation/constructor failure.
 */
template<class T,class... Args> T* create(Args&&... args) noexcept {
  void* bytes=allocate(sizeof(T));if(!bytes)return nullptr;
#if defined(__cpp_exceptions)
  try {return ::new(bytes) T(std::forward<Args>(args)...);}
  catch(...){std::free(bytes);return nullptr;}
#else
  return ::new(bytes) T(std::forward<Args>(args)...);
#endif
}
}
