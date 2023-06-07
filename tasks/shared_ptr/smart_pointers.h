#pragma once

#include <iostream>
#include <memory>


template <typename T>
class SharedPtr;

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args);

template <typename T>
class WeakPtr;

template <typename T>
class EnableSharedFromThis;

namespace detail {
  struct BaseControlBlock {
    int shared_count = 0;
    int weak_count = 0;

    BaseControlBlock(int shared_count, int weak_count)
      : shared_count(shared_count),
        weak_count(weak_count) {}

    virtual void DeleteObject(void* link) = 0;
    virtual void DeallocateItself() = 0;

    virtual ~BaseControlBlock() = default;
  };

  template <typename T, typename Deleter = std::default_delete<T>, typename Alloc = std::allocator<T>>
  struct ControlBlockRegular : public BaseControlBlock {
    T* object = nullptr;
    Deleter deleter;
    Alloc allocator;

    template <typename... Args>
    ControlBlockRegular(int shared_count, int weak_count, Deleter deleter, Args&&... args)
    : BaseControlBlock(shared_count, weak_count),
      object(std::forward<Args>(args)...),
      deleter(deleter) {
    }

    ControlBlockRegular(int shared_count, int weak_count, Deleter deleter, Alloc& allocator, T* object)
    : BaseControlBlock(shared_count, weak_count),
      object(object),
      deleter(deleter),
      allocator(allocator) {
    }

    template <typename... Args>
    ControlBlockRegular(int shared_count, int weak_count, Args&&... args)
    : BaseControlBlock(shared_count, weak_count),
      object(std::forward<Args>(args)...),
      deleter(std::default_delete<T>()) {
    }

    virtual void DeleteObject(void* ptr) {
      deleter(static_cast<T*>(ptr));
    }

    virtual void DeallocateItself() {
      using AllocControlBlock = typename std::allocator_traits<Alloc>:: template rebind_alloc<ControlBlockRegular<T, Deleter, Alloc>>;
      AllocControlBlock new_alloc = allocator;
      std::allocator_traits<AllocControlBlock>::deallocate(new_alloc, this, 1);
    }
  };

  template <typename T, typename Alloc = std::allocator<T>>
  struct ControlBlockMakeShared : public BaseControlBlock {
    T object;
    Alloc allocator;

    template <typename... Args>
    ControlBlockMakeShared(int shared_count, int weak_count, Alloc& allocator, Args&&... args)
      : BaseControlBlock(shared_count, weak_count),
        object(std::forward<Args>(args)...),
        allocator(allocator) {
    }

    template <typename... Args>
    ControlBlockMakeShared(int shared_count, int weak_count, Args&&... args)
      : BaseControlBlock(shared_count, weak_count),
        object(std::forward<Args>(args)...) {      
    }

    ~ControlBlockMakeShared() {
    }

    virtual void DeleteObject(void* ptr) {
      std::allocator_traits<Alloc>::destroy(allocator, &object);
      if (ptr == nullptr) {
        std::allocator_traits<Alloc>::destroy(allocator, &ptr);
      }
    }

    virtual void DeallocateItself() {
      using AllocControlBlock = typename std::allocator_traits<Alloc>:: template rebind_alloc<ControlBlockMakeShared<T, Alloc>>;
      AllocControlBlock new_alloc = allocator;
      std::allocator_traits<AllocControlBlock>::deallocate(new_alloc, this, 1);
    }
  };

};



template <typename T>
class SharedPtr {
 public:

  void swap(SharedPtr& other) {
    std::swap(control_block, other.control_block);
    std::swap(object, other.object);
  }

  SharedPtr()
    : object(nullptr),
      control_block(nullptr) {
  }

  explicit SharedPtr(T* ptr) 
  : control_block(static_cast<detail::BaseControlBlock*>(new detail::ControlBlockRegular<T>(1, 0, nullptr))) {
    static_cast<detail::ControlBlockRegular<T>*>(control_block)->object = ptr;
    object = ptr;
  } 

  template <typename Deleter>
  SharedPtr(T* ptr, Deleter deleter) : control_block(static_cast<detail::BaseControlBlock*>(new detail::ControlBlockRegular<T, Deleter>(1, 0, deleter, nullptr))) {
    static_cast<detail::ControlBlockRegular<T, Deleter>*>(control_block)->object = ptr;
    object = ptr;
  } 

  template <typename Deleter, typename Alloc>
  SharedPtr(T* ptr, Deleter deleter, Alloc allocator)
    : control_block(nullptr) {

    using AllocControlBlock = typename std::allocator_traits<Alloc>:: template rebind_alloc<detail::ControlBlockRegular<T, Deleter, Alloc>>;
    AllocControlBlock new_alloc = allocator;
    auto control_block_pointer = std::allocator_traits<AllocControlBlock>::allocate(new_alloc, 1);
    std::allocator_traits<AllocControlBlock>::deallocate(new_alloc, control_block_pointer, 1);
    deleter(static_cast<T*>(nullptr));
    object = ptr;
  }

  
  SharedPtr(SharedPtr&& other)
    : object(other.object),
      control_block(other.control_block) {
    other.control_block = nullptr;
    other.object = nullptr;
  }

  SharedPtr& operator=(SharedPtr&& other) {
    SharedPtr copy(std::move(other));
    swap(copy);
    return *this;
  }

  SharedPtr& operator=(const SharedPtr& other) {
    SharedPtr copy(other);
    swap(copy);
    return *this;
  }
  
  template <typename Derived>
  SharedPtr(const SharedPtr<Derived>& other)
      : object(static_cast<T*>(other.object)),
        control_block(other.control_block) {
    if (control_block != nullptr) {
      ++control_block->shared_count;
    }
  }

  template <typename Derived>
  SharedPtr(SharedPtr<Derived>&& other)
      : object(static_cast<T*>(other.object)),
        control_block(other.control_block) {
    other.control_block = nullptr;
    other.object = nullptr;
  }

  SharedPtr(const SharedPtr& other) 
    : object(other.object),
      control_block(other.control_block) {
    if (control_block != nullptr) {
      ++control_block->shared_count;
    }
  }

  ~SharedPtr() {
    if (control_block == nullptr) { return; }
    --control_block->shared_count;

    if (object == nullptr && 
        control_block->shared_count == 0 &&
        control_block->weak_count == 0) {
      return;
    }
    if (control_block->shared_count != 0) {
        return;
    }
    if (control_block->shared_count == 0) {
      control_block->DeleteObject(object);
    }
    if (control_block->shared_count == 0 && control_block->weak_count == 0) {
      control_block->DeallocateItself(); 
    }
  }

  T& operator*() {
    return *object;
  }

  const T& operator*() const {
    return *object;
  }
  
  T* operator->() {
    return object;
  }

  const T* operator->() const {
    return object;
  }

  T* get() {
    if (control_block == nullptr) { return nullptr; }
    return object;
  }

  const T* get() const {
    if (control_block == nullptr) { return nullptr; }
    return object;
  }

  void reset(T* ptr) noexcept {
    SharedPtr<T> copy(ptr);
    swap(copy);
  }

  void reset() noexcept {
    SharedPtr<T> copy;
    swap(copy);
  }

  size_t use_count() const noexcept { return control_block->shared_count; }

 private:
  SharedPtr(detail::BaseControlBlock* control_block, T* object)
    : object(object),
      control_block(control_block) {
    ++control_block->shared_count;
  }

  T* object;
  detail::BaseControlBlock* control_block = nullptr;

  template <typename U>
  friend class WeakPtr;
  
  template <typename U, typename... Args>
  friend SharedPtr<U> makeShared(Args&&... args);

  template <typename U, typename Alloc, typename... Args>
  friend SharedPtr<U> allocateShared(Alloc& alloc, Args&&... args);

  template <typename U>
  friend class SharedPtr;
};

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
  auto control_block = new detail::ControlBlockMakeShared<T>(0, 0, std::forward<Args>(args)...);
  return SharedPtr<T>(control_block, &control_block->object);
}

template <typename T, typename Alloc, typename... Args>
SharedPtr<T> allocateShared(Alloc& alloc, Args&&... args) {
  using AllocControlBlock = typename std::allocator_traits<Alloc>:: template rebind_alloc<detail::ControlBlockMakeShared<T, Alloc>>;
  AllocControlBlock new_alloc = alloc;
  auto control_block = std::allocator_traits<AllocControlBlock>::allocate(new_alloc, 1);
  std::allocator_traits<AllocControlBlock>::construct(new_alloc, control_block, 0, 0, alloc, std::forward<Args>(args)...);
  return SharedPtr<T>(static_cast<detail::BaseControlBlock*>(control_block), &control_block->object);
}

template <typename T>
class WeakPtr {
 public:
  T* object = nullptr;
  detail::BaseControlBlock* control_block = nullptr;

  void swap(WeakPtr& second) {
    std::swap(control_block, second.control_block);
    std::swap(object, second.object);
  }

  template <typename Derived>
  WeakPtr(const WeakPtr<Derived>& other)
      : object(other.object),
        control_block(other.control_block) {
    if (control_block != nullptr) {
      ++control_block->weak_count;
    }
  }
  
  template <typename Derived>
  WeakPtr(const SharedPtr<Derived>& other)
      : object(other.object),
        control_block(other.control_block) {
    if (control_block != nullptr) {
      ++control_block->weak_count;
    }
  }

  template <typename Derived>
  WeakPtr(WeakPtr<Derived>&& other)
      : object(other.object),
        control_block(other.control_block) {
    other.control_block = nullptr;
    other.object = nullptr;
  }

  template <typename Derived>
  WeakPtr(SharedPtr<Derived>&& other)
      : object(other.object),
        control_block(other.control_block) {
    other.control_block = nullptr;
    other.object = nullptr;
  }

  WeakPtr()
    : object(nullptr),
      control_block(static_cast<detail::BaseControlBlock*>(new detail::ControlBlockRegular<T>(1, 0))) {
  }

  WeakPtr(const SharedPtr<T>& shared_pointer) 
    : object(shared_pointer.object),
      control_block(shared_pointer.control_block) {
    ++control_block->weak_count;
  }

  WeakPtr(const WeakPtr& other)
    : object(other.object),
      control_block(other.control_block) {
    ++control_block->weak_count;
  }
  
  WeakPtr(WeakPtr&& other)
    : object(other.object),
      control_block(other.control_block) {
    other.control_block = nullptr;
    other.object = nullptr;
  }

  WeakPtr& operator=(const WeakPtr& other) {
    WeakPtr copy(other);
    swap(copy);
    return *this;
  }

  WeakPtr& operator=(WeakPtr&& other) {
    WeakPtr copy(std::move(other));
    swap(copy);
    return *this;
  }

  ~WeakPtr() {
    if (control_block == nullptr) { return; }
    --control_block->weak_count;
    if (control_block->weak_count == 0 && control_block->shared_count == 0) {
      control_block->DeallocateItself(); 
    }
  }

  size_t use_count() const noexcept { return control_block->shared_count; }

  bool expired() const noexcept {
    return control_block->shared_count == 0;
  }

  SharedPtr<T> lock() const noexcept {
    return SharedPtr<T>(control_block, object);
  }
};