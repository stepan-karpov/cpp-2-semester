#pragma once

#include <iostream>
#include <memory>
#include <utility>

template <size_t N>
class StackStorage {
 public:
  int shift = 0;
  char storage[N];

  StackStorage(){};
  StackStorage(const StackStorage&) = delete;
  ~StackStorage(){};

  int GetShift() {
    return shift;
  }

  void Set(int new_shift) {
    shift = new_shift;
  }

  char* GetStorage() {
    return storage;
  }
};

template <typename T, size_t N>
class StackAllocator {
 public:
  using value_type = T;

  StackStorage<N>* storage;

  StackAllocator()
      : storage(nullptr) {
  }

  StackAllocator(StackStorage<N>& storage_init)
      : storage(&storage_init) {
  }

  StackAllocator(const StackAllocator& init)
      : storage(init.storage) {
  }

  template <typename U>
  StackAllocator(const StackAllocator<U, N>& init_alloc) {
    storage = init_alloc.storage;
  }

  template <typename U>
  StackAllocator& operator=(const StackAllocator<U, N>& init_alloc) {
    this->storage = init_alloc.storage;
    return *this;
  }

  ~StackAllocator() {
  }

  T* allocate(size_t count) {
    void* ptr = reinterpret_cast<void*>(storage->storage + storage->shift);
    size_t free_space = N - storage->shift;
    std::align(alignof(T), count * sizeof(T), ptr, free_space);
    storage->Set(reinterpret_cast<char*>(ptr) - storage->storage);
    size_t old_shift = storage->shift;
    storage->shift += sizeof(T) * count;
    return reinterpret_cast<T*>(storage->storage + old_shift);
  }

  void deallocate(T* ptr, size_t) {
    std::ignore = ptr;
  }

  template <typename... Args>
  void construct(T* ptr, const Args&... args) {
    new (ptr) T(args...);
  }

  void destroy(T* ptr) {
    ptr->~T();
  }

  template <typename U>
  struct rebind {
    using other = StackAllocator<U, N>;
  };

  StackAllocator<T, N> select_on_container_copy_construction() {
    return StackAllocator<T, N>();
  }
};

template <typename T, size_t N>
bool operator==(const StackAllocator<T, N>& a, const StackAllocator<T, N>& b) {
  return a.storage == b.storage;
}

template <typename T, typename Alloc = std::allocator<T>>
class List {
 private:
  size_t capacity = 0;

  struct Node;

  struct BaseNode {
    BaseNode* prev = nullptr;
    BaseNode* next = nullptr;
    BaseNode() = default;
    BaseNode(BaseNode* prev, BaseNode* next)
        : prev(prev),
          next(next){};
  };

  struct Node : BaseNode {
    T value;
    Node() = default;
    Node(const T& value)
        : value(value) {
    }
    Node(BaseNode* prev, BaseNode* next, const T& value)
        : BaseNode(prev, next),
          value(value) {
    }
    Node(BaseNode* prev, BaseNode* next)
        : BaseNode(prev, next) {
    }
  };

  using NodeAlloc =
      typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;
  using BaseNodeAlloc =
      typename std::allocator_traits<Alloc>::template rebind_alloc<BaseNode>;

  NodeAlloc node_alloc;
  BaseNodeAlloc basenode_alloc;

  BaseNode* fake_node;

 public:
  template <bool is_constant>
  struct Iterator {
    using value_type = T;
    using reference = std::conditional_t<is_constant, const T&, T&>;
    using pointer = std::conditional_t<is_constant, const T*, T*>;
    using stor_type = std::conditional<is_constant, const BaseNode*, BaseNode*>;
    using difference_type = ptrdiff_t;
    BaseNode* node = nullptr;

    Iterator() = default;

    // code == 1 -> list.begin()
    // code == 2 -> list.end()
    Iterator(int code, BaseNode* fake_node) {
      if (code == 1) {
        node = fake_node->next;
        return;
      }
      node = fake_node;
    }

    Iterator(const Iterator& init) {
      node = init.node;
    }

    reference operator*() const {
      try {
        return static_cast<Node*>(node)->value;
      } catch (...) {
        throw std::string("wrong iterator dereference!!! durak");
      }
    }

    pointer operator->() const {
      return &static_cast<Node*>(node)->value;
    }

    Iterator& operator++() {
      node = node->next;
      return *this;
    }

    Iterator operator++(int) {
      Iterator copy = *this;
      ++*this;
      return copy;
    }

    Iterator operator--(int) {
      Iterator copy = *this;
      --*this;
      return copy;
    }

    Iterator& operator--() {
      node = node->prev;
      return *this;
    }

    bool operator==(const Iterator& it) const {
      return node == it.node;
    }
    bool operator!=(const Iterator& it) const {
      return node != it.node;
    }

    Iterator& operator=(const Iterator& init_it) {
      node = init_it.node;
      return *this;
    }
    operator Iterator<true>() const {
      return Iterator<true>();
    }
  };

  typedef Iterator<false> iterator;
  typedef Iterator<true> const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  friend iterator operator-(iterator it, int value) {
    for (int i = 0; i < value; ++i) {
      --it;
    }
    return it;
  }

  friend iterator operator+(iterator it, int value) {
    for (int i = 0; i < value; ++i) {
      ++it;
    }
    return it;
  }

  iterator begin() {
    return iterator(1, fake_node);
  }
  iterator end() {
    return iterator(2, fake_node);
  }

  const_iterator begin() const {
    return const_iterator(1, fake_node);
  }
  const_iterator end() const {
    return const_iterator(2, fake_node);
  }

  const_iterator cbegin() const {
    return const_iterator(1, fake_node);
  }
  const_iterator cend() const {
    return const_iterator(2, fake_node);
  }

  reverse_iterator rbegin() {
    return std::reverse_iterator(end());
  }
  reverse_iterator rend() {
    return std::reverse_iterator(begin());
  }

  const_reverse_iterator rbegin() const {
    return std::reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return std::reverse_iterator(begin());
  }

  const_reverse_iterator crbegin() const {
    return std::reverse_iterator(cend());
  }
  const_reverse_iterator crend() const {
    return std::reverse_iterator(cbegin());
  }

  void InitFakeNode() {
    fake_node =
        std::allocator_traits<BaseNodeAlloc>::allocate(basenode_alloc, 1);
    std::allocator_traits<BaseNodeAlloc>::construct(basenode_alloc, fake_node);
  }

  void AllocateInit(int new_capacity) {
    BaseNode* prev = fake_node;
    std::vector<Node*> allocated;
    if constexpr (std::is_default_constructible<T>()) {
      for (int i = 0; i < new_capacity; ++i) {
        Node* new_node =
            std::allocator_traits<NodeAlloc>::allocate(node_alloc, 1);
        try {
          std::allocator_traits<NodeAlloc>::construct(node_alloc, new_node,
                                                      prev, nullptr);
          allocated.push_back(new_node);
        } catch (...) {
          for (int j = 0; j < i; ++j) {
            std::allocator_traits<NodeAlloc>::destroy(node_alloc, allocated[j]);
            std::allocator_traits<NodeAlloc>::deallocate(node_alloc,
                                                         allocated[j], 1);
          }
          std::allocator_traits<BaseNodeAlloc>::destroy(basenode_alloc,
                                                        fake_node);
          std::allocator_traits<BaseNodeAlloc>::deallocate(basenode_alloc,
                                                           fake_node, 1);
          throw;
        }
        prev->next = new_node;
        prev = dynamic_cast<BaseNode*>(new_node);
      }
      prev->next = fake_node;
      fake_node->prev = prev;
      capacity = new_capacity;
    } else {
      throw std::string("no default constructor for this type!! durak");
    }
  }

  List(const Alloc& init_allocator)
      : node_alloc(init_allocator),
        basenode_alloc(init_allocator) {
    InitFakeNode();
  }

  List(int new_capacity, const Alloc& init_allocator)
      : node_alloc(init_allocator),
        basenode_alloc(init_allocator) {
    InitFakeNode();
    AllocateInit(new_capacity);
  }

  List(int new_capacity, const T& value, const Alloc& init_allocator)
      : node_alloc(init_allocator),
        basenode_alloc(init_allocator) {
    InitFakeNode();

    if constexpr (std::is_default_constructible<T>()) {
      for (int i = 0; i < new_capacity; ++i) {
        push_back(value);
      }
    } else {
      throw std::string("no default constructor for this type!! durak ");
    }
  }

  List() {
    InitFakeNode();
  }

  List(const List& init) {
    node_alloc =
        std::allocator_traits<NodeAlloc>::select_on_container_copy_construction(
            init.node_alloc);
    basenode_alloc = std::allocator_traits<BaseNodeAlloc>::
        select_on_container_copy_construction(init.basenode_alloc);
    InitFakeNode();
    auto it = init.begin();
    BaseNode* prev = fake_node;
    std::vector<Node*> allocated;
    for (size_t i = 0; i < init.capacity; ++i) {
      Node* new_node =
          std::allocator_traits<NodeAlloc>::allocate(node_alloc, 1);
      try {
        std::allocator_traits<NodeAlloc>::construct(node_alloc, new_node, prev,
                                                    nullptr, *it);
        allocated.push_back(new_node);
      } catch (...) {
        for (int j = 0; j < i; ++j) {
          std::allocator_traits<NodeAlloc>::destroy(node_alloc, allocated[j]);
          std::allocator_traits<NodeAlloc>::deallocate(node_alloc, allocated[j],
                                                       1);
        }
        std::allocator_traits<BaseNodeAlloc>::destroy(basenode_alloc,
                                                      fake_node);
        std::allocator_traits<BaseNodeAlloc>::deallocate(basenode_alloc,
                                                         fake_node, 1);
        throw;
      }
      prev->next = new_node;
      prev = dynamic_cast<BaseNode*>(new_node);
      ++it;
    }
    prev->next = fake_node;
    fake_node->prev = prev;
    capacity = init.capacity;
  }

  List(int new_capacity) {
    InitFakeNode();
    AllocateInit(new_capacity);
  }

  List(int new_capacity, const T& value) {
    InitFakeNode();

    if constexpr (std::is_default_constructible<T>()) {
      for (int i = 0; i < new_capacity; ++i) {
        push_back(value);
      }
    } else {
      throw std::string("no default constructor for this type!! durak ");
    }
  }

  ~List() {
    while (capacity != 0) {
      pop_back();
    }
    std::allocator_traits<BaseNodeAlloc>::destroy(basenode_alloc, fake_node);
    std::allocator_traits<BaseNodeAlloc>::deallocate(basenode_alloc, fake_node,
                                                     1);
  }

  void swap(List<T, Alloc>& to_swap) {
    std::swap(fake_node, to_swap.fake_node);
    std::swap(capacity, to_swap.capacity);
    std::swap(node_alloc, to_swap.node_alloc);
    std::swap(basenode_alloc, to_swap.basenode_alloc);
  }

  List& operator=(const List<T, Alloc>& init) {
    List<T, Alloc> cp = init;
    swap(cp);
    if (std::allocator_traits<
            NodeAlloc>::propagate_on_container_copy_assignment::value) {
      node_alloc = init.node_alloc;
      basenode_alloc = init.basenode_alloc;
    }
    return *this;
  }

  void push_back(const T& value) {
    insert(end(), value);
  }
  void push_front(const T& value) {
    insert(begin(), value);
  }
  void pop_front() {
    erase(begin());
  }
  void pop_back() {
    erase(end() - 1);
  }

  Alloc get_allocator() const {
    return node_alloc;
  }

  size_t size() const {
    return capacity;
  }

  void insert(const iterator& it, const T& value) {
    if (capacity == 0) {
      Node* new_node =
          std::allocator_traits<NodeAlloc>::allocate(node_alloc, 1);
      std::allocator_traits<NodeAlloc>::construct(node_alloc, new_node,
                                                  fake_node, fake_node, value);

      fake_node->next = dynamic_cast<BaseNode*>(new_node);
      fake_node->prev = dynamic_cast<BaseNode*>(new_node);
      ++capacity;
      return;
    }
    BaseNode* prev_node = (it.node)->prev;
    BaseNode* next_node = it.node;
    Node* new_node = std::allocator_traits<NodeAlloc>::allocate(node_alloc, 1);
    std::allocator_traits<NodeAlloc>::construct(node_alloc, new_node, prev_node,
                                                next_node, value);
    prev_node->next = new_node;
    next_node->prev = new_node;
    ++capacity;
  }

  // I don't know how to make insert by const_iterator
  // and by iterator without copy-paste
  void insert(const const_iterator& it, const T& value) {
    if (capacity == 0) {
      Node* new_node =
          std::allocator_traits<NodeAlloc>::allocate(node_alloc, 1);
      std::allocator_traits<NodeAlloc>::construct(node_alloc, new_node,
                                                  fake_node, fake_node, value);

      fake_node->next = dynamic_cast<BaseNode*>(new_node);
      fake_node->prev = dynamic_cast<BaseNode*>(new_node);
      ++capacity;
      return;
    }
    BaseNode* prev_node = (it.node)->prev;
    BaseNode* next_node = it.node;
    Node* new_node = std::allocator_traits<NodeAlloc>::allocate(node_alloc, 1);
    std::allocator_traits<NodeAlloc>::construct(node_alloc, new_node, prev_node,
                                                next_node, value);
    prev_node->next = new_node;
    next_node->prev = new_node;
    ++capacity;
  }

  void erase(const iterator& it) {
    if (capacity == 0) {
      throw std::string("what do you want to delete??? durak");
    }
    BaseNode* prev_node = (it.node)->prev;
    BaseNode* next_node = (it.node)->next;
    std::allocator_traits<NodeAlloc>::destroy(node_alloc,
                                              static_cast<Node*>(it.node));
    std::allocator_traits<NodeAlloc>::deallocate(
        node_alloc, static_cast<Node*>(it.node), 1);
    prev_node->next = next_node;
    next_node->prev = prev_node;
    --capacity;
  }

  // I don't know how to make erase by const_iterator
  // and by iterator without copy-paste
  void erase(const const_iterator& it) {
    if (capacity == 0) {
      throw std::string("what do you want to delete??? durak");
    }
    BaseNode* prev_node = (it.node)->prev;
    BaseNode* next_node = (it.node)->next;
    std::allocator_traits<NodeAlloc>::destroy(node_alloc,
                                              static_cast<Node*>(it.node));
    std::allocator_traits<NodeAlloc>::deallocate(
        node_alloc, static_cast<Node*>(it.node), 1);
    prev_node->next = next_node;
    next_node->prev = prev_node;
    --capacity;
  }
};