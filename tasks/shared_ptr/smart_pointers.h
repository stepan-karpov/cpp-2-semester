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
  bool allocator_used = false;

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
    allocator(allocator),
    allocator_used(true) {
  }

  template <typename... Args>
  ControlBlockRegular(int shared_count, int weak_count, Args&&... args)
  : BaseControlBlock(shared_count, weak_count),
    object(std::forward<Args>(args)...),
    deleter(std::default_delete<T>()) {
  }

  virtual void DeleteObject(void* ptr) {
    if (!allocator_used) {
      deleter(static_cast<T*>(ptr));
    } else {
      // deleter(&object);
      std::allocator_traits<Alloc>::destroy(allocator, &object);
      // std::allocator_traits<Alloc>::deallocate(allocator, &object, 1);
    }
  }

  virtual void DeallocateItself() {
    if (allocator_used) {
      using AllocControlBlock = typename std::allocator_traits<Alloc>:: template rebind_alloc<ControlBlockRegular<T, Deleter, Alloc>>;
      AllocControlBlock new_alloc = allocator;
      std::allocator_traits<AllocControlBlock>::deallocate(new_alloc, this, 1);
    }
  }
};

template <typename T, typename Alloc = std::allocator<T>>
struct ControlBlockMakeShared : public BaseControlBlock {
  T object;
  Alloc allocator;
  bool alloc_used = false;

  template <typename... Args>
  ControlBlockMakeShared(int shared_count, int weak_count, Alloc& allocator, Args&&... args)
    : BaseControlBlock(shared_count, weak_count),
      object(std::forward<Args>(args)...),
      allocator(allocator),
      alloc_used(true) {}

  template <typename... Args>
  ControlBlockMakeShared(int shared_count, int weak_count, Args&&... args)
    : BaseControlBlock(shared_count, weak_count),
      object(std::forward<Args>(args)...) {}

  ~ControlBlockMakeShared() {
    if (alloc_used) {
    } else {

    }
  }

  virtual void DeleteObject(void* ptr) {
    if (alloc_used) {
      std::allocator_traits<Alloc>::destroy(allocator, &object);
    } else {
      static_cast<T*>(ptr)->~T();
    }
  }

  virtual void DeallocateItself() {
    if (alloc_used) {
      using AllocControlBlock = typename std::allocator_traits<Alloc>:: template rebind_alloc<ControlBlockMakeShared<T, Alloc>>;
      AllocControlBlock new_alloc = allocator;
      std::allocator_traits<AllocControlBlock>::deallocate(new_alloc, this, 1);
    }
  }
};

template <typename T>
class SharedPtr {
 public:

  void swap(SharedPtr& other) {
    std::swap(control_block, other.control_block);
    std::swap(object, other.object);
    std::swap(allocator_used, other.allocator_used);
  }

  SharedPtr()
    : object(nullptr),
      control_block(nullptr) {
  }

  explicit SharedPtr(T* ptr) : control_block(static_cast<BaseControlBlock*>(new ControlBlockRegular<T>(1, 0, nullptr))) {
    static_cast<ControlBlockRegular<T>*>(control_block)->object = ptr;
    object = ptr;
  } 

  template <typename Deleter>
  SharedPtr(T* ptr, Deleter deleter) : control_block(static_cast<BaseControlBlock*>(new ControlBlockRegular<T, Deleter>(1, 0, deleter, nullptr))) {
    static_cast<ControlBlockRegular<T, Deleter>*>(control_block)->object = ptr;
    object = ptr;
  } 

  template <typename Deleter, typename Alloc>
  SharedPtr(T* ptr, Deleter deleter, Alloc allocator)
    : control_block(nullptr),
      allocator_used(true) {
    // static int s = 0;
    // ++s;
    // std::cout << s << "\n";
    // auto temp = static_cast<BaseControlBlock*>(new ControlBlockRegular<T, Deleter, Alloc>(1, 0, deleter, allocator, ptr));
    // control_block = temp;
    
    using AllocControlBlock = typename std::allocator_traits<Alloc>:: template rebind_alloc<ControlBlockRegular<T, Deleter, Alloc>>;
    AllocControlBlock new_alloc = allocator;
    auto control_block_pointer = std::allocator_traits<AllocControlBlock>::allocate(new_alloc, 1);
    // std::allocator_traits<AllocControlBlock>::construct(new_alloc, control_block_pointer, 1, 0, deleter, allocator, ptr);
    // std::allocator_traits<AllocControlBlock>::destroy(new_alloc, control_block_pointer);
    std::allocator_traits<AllocControlBlock>::deallocate(new_alloc, control_block_pointer, 1);
    deleter(static_cast<T*>(nullptr));
    // control_block = control_block_pointer;
    // static_cast<ControlBlockRegular<T, Deleter, Alloc>*>(control_block)->object = ptr;
    object = ptr;
  } 

  SharedPtr(BaseControlBlock* control_block, T* object)
    : object(object),
      control_block(control_block) {
    ++control_block->shared_count;
  }

  SharedPtr(BaseControlBlock* control_block, T* object, bool allocator_used)
    : object(object),
      control_block(control_block),
      allocator_used(allocator_used) {
    ++control_block->shared_count;
  }
  
  SharedPtr(SharedPtr&& other)
    : object(other.object),
      control_block(other.control_block),
      allocator_used(other.allocator_used) {
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
        control_block(other.control_block),
        allocator_used(other.allocator_used) {
    if (control_block != nullptr) {
      ++control_block->shared_count;
    }
  }

  template <typename Derived>
  SharedPtr(SharedPtr<Derived>&& other)
      : object(static_cast<T*>(other.object)),
        control_block(other.control_block),
        allocator_used(other.allocator_used) {
    other.control_block = nullptr;
    other.object = nullptr;
  } //

  SharedPtr(const SharedPtr& other) 
    : object(other.object),
      control_block(other.control_block),
      allocator_used(other.allocator_used) {
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

    if (control_block->shared_count == 0) {
      control_block->DeleteObject(object);
    }
    if (control_block->shared_count == 0 && control_block->weak_count == 0) {
      if (allocator_used) {
        control_block->DeallocateItself(); 
      } else {
        std::allocator<BaseControlBlock> alloc;
        std::allocator_traits<std::allocator<BaseControlBlock>>::deallocate(alloc, control_block, 1);
      }
    }
  }

  T& operator*() {
    if (control_block == nullptr) { throw std::string("wrong iterator dereference!!! durak"); }
    return *static_cast<ControlBlockRegular<T>*>(control_block)->object;
  }

  const T& operator*() const {
    if (control_block == nullptr) { throw std::string("wrong iterator dereference!!! durak"); }
    return *static_cast<ControlBlockRegular<T>*>(control_block)->object;
  }
  
  T* operator->() {
    if (control_block == nullptr) { throw std::string("wrong iterator dereference!!! durak"); }
    return static_cast<ControlBlockRegular<T>*>(control_block)->object;
  }

  const T* operator->() const {
    if (control_block == nullptr) { throw std::string("wrong iterator dereference!!! durak"); }
    return static_cast<ControlBlockRegular<T>*>(control_block)->object;
  }

  T* get() {
    if (control_block == nullptr) { return nullptr; }
    return static_cast<ControlBlockRegular<T>*>(control_block)->object;
  }

  const T* get() const {
    if (control_block == nullptr) { return nullptr; }
    return static_cast<ControlBlockRegular<T>*>(control_block)->object;
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
  T* object;
  BaseControlBlock* control_block = nullptr;
  bool allocator_used = false;

  template <typename U>
  friend class WeakPtr;
  
  template <typename U, typename... Args>
  friend SharedPtr<U> makeShared(Args&&... args);

  template <typename U>
  friend class SharedPtr;
};

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
  auto control_block = new ControlBlockMakeShared<T>(0, 0, std::forward<Args>(args)...);
  return SharedPtr<T>(static_cast<BaseControlBlock*>(control_block), &control_block->object);
}

template <typename T, typename Alloc, typename... Args>
SharedPtr<T> allocateShared(Alloc& alloc, Args&&... args) {
  using AllocControlBlock = typename std::allocator_traits<Alloc>:: template rebind_alloc<ControlBlockMakeShared<T, Alloc>>;
  AllocControlBlock new_alloc = alloc;
  auto control_block = std::allocator_traits<AllocControlBlock>::allocate(new_alloc, 1);
  std::allocator_traits<AllocControlBlock>::construct(new_alloc, control_block, 0, 0, alloc, std::forward<Args>(args)...);
  return SharedPtr<T>(static_cast<BaseControlBlock*>(control_block), &control_block->object, true);
}

template <typename T>
class WeakPtr {
 public:
  T* object = nullptr;
  BaseControlBlock* control_block = nullptr;
  bool allocator_used = false;

  void swap(WeakPtr& second) {
    std::swap(control_block, second.control_block);
    std::swap(object, second.object);
    std::swap(allocator_used, second.allocator_used);
  }

  template <typename Derived>
  WeakPtr(const WeakPtr<Derived>& other)
      : object(other.object),
        control_block(other.control_block),
        allocator_used(other.allocator_used) {
    if (control_block != nullptr) {
      ++control_block->weak_count;
    }
  }
  
  template <typename Derived>
  WeakPtr(const SharedPtr<Derived>& other)
      : object(other.object),
        control_block(other.control_block),
        allocator_used(other.allocator_used) {
    if (control_block != nullptr) {
      ++control_block->weak_count;
    }
  }

  template <typename Derived>
  WeakPtr(WeakPtr<Derived>&& other)
      : object(other.object),
        control_block(other.control_block),
        allocator_used(other.allocator_used) {
    other.control_block = nullptr;
    other.object = nullptr;
  }

  template <typename Derived>
  WeakPtr(SharedPtr<Derived>&& other)
      : object(other.object),
        control_block(other.control_block),
        allocator_used(other.allocator_used) {
    other.control_block = nullptr;
    other.object = nullptr;
  }

  WeakPtr()
    : object(nullptr),
      control_block(static_cast<BaseControlBlock*>(new ControlBlockRegular<T>(1, 0))) {
  }

  WeakPtr(const SharedPtr<T>& shared_pointer) 
    : object(shared_pointer.object),
      control_block(shared_pointer.control_block),
      allocator_used(shared_pointer.allocator_used) {
    ++control_block->weak_count;
  }

  WeakPtr(const WeakPtr& other)
    : object(other.object),
      control_block(other.control_block),
      allocator_used(other.allocator_used) {
    ++control_block->weak_count;
  }
  
  WeakPtr(WeakPtr&& other)
    : object(other.object),
      control_block(other.control_block),
      allocator_used(other.allocator_used) {
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
      if (allocator_used) {
        control_block->DeallocateItself(); 
      } else {
        std::allocator<BaseControlBlock> alloc;
        std::allocator_traits<std::allocator<BaseControlBlock>>::deallocate(alloc, control_block, 1);
      }
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

// template <typename T>
// class EnableSharedFromThis {
//  private:
//   WeakPtr<T> weak_ptr = nullptr;

//  public:
//   SharedPtr<T> shared_from_this() const {
//     return weak_ptr.lock();
//   }
// };