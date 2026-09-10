#pragma once
#include <Arduino.h>
#include <cstring>
#include <utility>
// Explicitly fallible storage for POD map/route data. Never consumes internal
// task-stack RAM or terminates the device through std::vector allocation failure.
template<class T> class PsBuffer {
  T* ptr=nullptr;
  size_t count=0,capacity=0;
public:
  ~PsBuffer() { free(ptr); }
  PsBuffer()=default;
  PsBuffer(const PsBuffer&)=delete;
  PsBuffer& operator=(const PsBuffer&)=delete;
  bool reserve(size_t n) {
    if(n<=capacity)return true;
    T* next=static_cast<T*>(ps_malloc(n*sizeof(T)));
    if(!next)return false;
    if(count)memcpy(next,ptr,count*sizeof(T));
    free(ptr);ptr=next;capacity=n;return true;
  }
  bool resize(size_t n) { if(!reserve(n))return false;count=n;return true; }
  void clear() {count=0;}
  bool empty() const {return count==0;}
  size_t size() const {return count;}
  T* data() {return ptr;}
  T* begin() {return ptr;}
  T* end() {return count?ptr+count:ptr;}
  const T& operator[](size_t n) const {return ptr[n];}
  T& operator[](size_t n) {return ptr[n];}
  const T& back() const {return ptr[count-1];}
  void swap(PsBuffer& b) {std::swap(ptr,b.ptr);std::swap(count,b.count);std::swap(capacity,b.capacity);}
};
