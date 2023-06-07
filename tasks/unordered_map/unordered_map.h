#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <vector>
#include <stdexcept>
#include <cmath>

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
 public:
  size_t capacity = 0;
  struct Node;
  struct BaseNode;

  using TAlloc = 
      typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
  using NodeAlloc =
      typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;
  using BaseNodeAlloc =
      typename std::allocator_traits<Alloc>::template rebind_alloc<BaseNode>;


  struct BaseNode {
    BaseNode* prev = nullptr;
    BaseNode* next = nullptr;
    BaseNode() {
      prev = this;
      next = this;
    };
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
    Node(BaseNode* prev, BaseNode* next)
      : BaseNode(prev, next) {
    }
    template <typename... Args>
    Node(BaseNode* prev, BaseNode* next, Args&&... args)
      : BaseNode(prev, next),
        value(std::forward<Args>(args)...) {
    }
  };

  using NodeAllocTraits = typename std::allocator_traits<NodeAlloc>;
  using BaseNodeAllocTraits = typename std::allocator_traits<BaseNode>;

  NodeAlloc node_alloc;
  BaseNodeAlloc basenode_alloc;
  TAlloc t_alloc;

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
      Iterator<true> tmp;
      tmp.node = node;
      return tmp;
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
            NodeAllocTraits::allocate(node_alloc, 1);
        try {
          NodeAllocTraits::construct(node_alloc, new_node,
                                                      prev, nullptr);
          allocated.push_back(new_node);
        } catch (...) {
          for (int j = 0; j < i; ++j) {
            NodeAllocTraits::destroy(node_alloc, allocated[j]);
            NodeAllocTraits::deallocate(node_alloc,
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

  Node* AllocNode() {
    Node* new_node = NodeAllocTraits::allocate(node_alloc, 1);
    return new_node;
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
        NodeAllocTraits::select_on_container_copy_construction(
            init.node_alloc);
    basenode_alloc = std::allocator_traits<BaseNodeAlloc>::
        select_on_container_copy_construction(init.basenode_alloc);
    InitFakeNode();
    auto it = init.begin();
    BaseNode* prev = fake_node;
    std::vector<Node*> allocated;
    for (size_t i = 0; i < init.capacity; ++i) {
      Node* new_node =
          NodeAllocTraits::allocate(node_alloc, 1);
      try {
        NodeAllocTraits::construct(node_alloc, new_node, prev,
                                                    nullptr, *it);
        allocated.push_back(new_node);
      } catch (...) {
        for (int j = 0; j < i; ++j) {
          NodeAllocTraits::destroy(node_alloc, allocated[j]);
          NodeAllocTraits::deallocate(node_alloc, allocated[j],
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

  List(List&& init) 
    : capacity(std::move(init.capacity)),
      node_alloc(std::move(init.node_alloc)),
      basenode_alloc(std::move(init.basenode_alloc)),
      fake_node(std::move(init.fake_node)) {
    init.capacity = 0;
    init.fake_node = nullptr;
  }

  List& operator=(List&& init) {
    List new_list(std::move(init));
    swap(new_list);
    return *this;
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
    emplace(end(), value);
  }
  void push_front(const T& value) {
    emplace(begin(), value);
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

  void ChangeForwardLink(iterator& node_to_change, iterator& node_to_point) {
    node_to_change.node->next = node_to_point.node;
  }

  void ChangeBackLink(iterator& node_to_change, iterator& node_to_point) {
    node_to_change.node->prev = node_to_point.node;
  }

  void insert(const const_iterator& it, const T& value) {
    emplace(it, value);
  }

  void insert(const const_iterator& it, T&& value) {
    emplace(it, std::move(value));
  }

  // initializes new node with 'args' parameters
  // and inserts new node before 'it' iterator
  template <typename... Args>
  void emplace(const const_iterator& it, Args&&... args) {
    if (capacity == 0) {

      Node* new_node =
          NodeAllocTraits::allocate(node_alloc, 1);
      NodeAllocTraits::construct(node_alloc, new_node,
                                                  fake_node, fake_node, std::forward<Args>(args)...);
      
      fake_node->next = dynamic_cast<BaseNode*>(new_node);
      fake_node->prev = dynamic_cast<BaseNode*>(new_node);
      ++capacity;
      return;
    }
    BaseNode* prev_node = (it.node)->prev;
    BaseNode* next_node = it.node;
    Node* new_node = NodeAllocTraits::allocate(node_alloc, 1);
    NodeAllocTraits::construct(node_alloc, new_node,
                                                prev_node, next_node, std::forward<Args>(args)...);
    prev_node->next = new_node;
    next_node->prev = new_node;
    ++capacity;
  }

  void emplaceForUM(const const_iterator& it, Node* new_node) {
    if (capacity == 0) {
      new_node->prev = fake_node;
      new_node->next = fake_node;

      fake_node->next = dynamic_cast<BaseNode*>(new_node);
      fake_node->prev = dynamic_cast<BaseNode*>(new_node);
      ++capacity;
      return;
    }
    BaseNode* prev_node = (it.node)->prev;
    BaseNode* next_node = it.node;

    new_node->prev = prev_node;
    new_node->next = next_node;
    prev_node->next = new_node;
    next_node->prev = new_node;
    ++capacity;
  }


  void erase(const const_iterator& it) {
    if (capacity == 0) {
      throw std::string("what do you want to delete??? durak");
    }
    BaseNode* prev_node = (it.node)->prev;
    BaseNode* next_node = (it.node)->next;
    NodeAllocTraits::destroy(node_alloc,
                                              static_cast<Node*>(it.node));
    NodeAllocTraits::deallocate(
        node_alloc, static_cast<Node*>(it.node), 1);
    prev_node->next = next_node;
    next_node->prev = prev_node;
    --capacity;
  }
};

const double EPS = 1e-6;

template <typename Key, typename Value, typename Hash=std::hash<Key>,
          typename Equal=std::equal_to<Key>,
          typename Alloc=std::allocator<std::pair<const Key, Value>>>
class UnorderedMap {
 public:
  using NodeType = std::pair<Key, Value>;
  struct Node {
    NodeType kv;
    size_t hash;
    Node(const NodeType& kv, size_t hash) : kv(kv), hash(hash) {}
    Node(NodeType&& kv, size_t hash) : kv(std::move(kv)), hash(hash) {
    }
    Node(const Key& key, const Value& value, size_t hash)
      : kv(std::make_pair(key, value)), hash(hash) {}
    Node(Key&& key, Value&& value, size_t hash)
      : kv(std::make_pair(std::move(key), std::move(value))),
        hash(hash) {}
  };
  using ListType = List<Node, Alloc>;
  using ListTypeIterator = typename ListType::iterator;
  using BucketAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<ListTypeIterator>;
  using NodeAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;

  template <bool is_constant>
  struct Iterator {
    using value_type = NodeType;
    using reference = std::conditional_t<is_constant, const NodeType&, NodeType&>;
    using pointer = std::conditional_t<is_constant, const NodeType*, NodeType*>;
    using stor_type = std::conditional<is_constant, const ListTypeIterator*, ListTypeIterator*>;
    using difference_type = ptrdiff_t;

    typename List<Node, Alloc>:: template Iterator<is_constant> it;
    
    Iterator() = default;

    Iterator(bool is_begin, UnorderedMap& map) {
      if (is_begin) {
        it = map.main_list_.begin();
      } else {
        it = map.main_list_.end();
      }
    }

    Iterator(bool is_begin, const UnorderedMap& map) {
      if (is_begin) {
        it = map.main_list_.begin();
      } else {
        it = map.main_list_.end();
      }
    }

    Iterator(const ListTypeIterator& init_iterator) {
      it = init_iterator;
    }

    Iterator(const Iterator& init) {
      it = init.it;
    }

    reference operator*() const {
      return (*it).kv;
    }

    pointer operator->() const {
      return &(it->kv);
    }

    Iterator& operator++() {
      ++it;
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
      --it;
      return *this;
    }

    bool operator==(const Iterator& other) const {
      return it == other.it;
    }
    bool operator!=(const Iterator& other) const {
      return it != other.it;
    }
    
    Iterator& operator=(const Iterator& init) {
      it = init.it;
      return *this;
    }

    operator Iterator<true>() const {
      Iterator<true> tmp;
      tmp.it = it;
      return tmp;
    }
  };

  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator> ;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

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
    return iterator(true, *this);
  }
  iterator end() { return iterator(false, *this); }
  
  const_iterator begin() const { return const_iterator(true, *this); }
  const_iterator end() const {  return const_iterator(false, *this); }

  const_iterator cbegin() const { return const_iterator(true, *this); }
  const_iterator cend() const { return const_iterator(false, *this); }

  reverse_iterator rbegin() { return std::reverse_iterator(end()); }
  reverse_iterator rend() { return std::reverse_iterator(begin()); }

  const_reverse_iterator rbegin() const { return std::reverse_iterator(end()); }
  const_reverse_iterator rend() const { return std::reverse_iterator(begin()); }

  const_reverse_iterator crbegin() const { return std::reverse_iterator(cend()); }
  const_reverse_iterator crend() const { return std::reverse_iterator(cbegin()); }

  void swap(UnorderedMap& other) {
    std::swap(max_load_factor_, other.max_load_factor_);
    std::swap(pair_allocator_, other.pair_allocator_);
    std::swap(buckets_, other.buckets_);
    std::swap(find_hash_, other.find_hash_);
    std::swap(bucket_number_, other.bucket_number_);
    std::swap(main_list_, other.main_list_);
    std::swap(is_equal_, other.is_equal_);
  }

  // default constructor
  UnorderedMap() {
    AllocateBuckets(bucket_number_);
  }
  
  // copy constructor
  UnorderedMap(const UnorderedMap& init) {
    AllocateBuckets(init.bucket_number_);
    for (auto it = init.begin(); it != init.end(); ++it) {
      emplace(*it);
    }
  }

  // copy assignment operator
  UnorderedMap& operator=(const UnorderedMap& init) {
    UnorderedMap new_map(init);
    swap(new_map);
    return *this;
  }
  
  // move assignment operator
  UnorderedMap& operator=(UnorderedMap&& init) {
    UnorderedMap new_map(std::move(init));
    swap(new_map);
    return *this;
  }

  // move copy constructor (== default)
  UnorderedMap(UnorderedMap&& init)
    : max_load_factor_(std::move(init.max_load_factor_)),
      pair_allocator_(std::move(init.pair_allocator_)),
      buckets_(std::move(init.buckets_)),
      find_hash_(std::move(init.find_hash_)),
      bucket_number_(std::move(init.bucket_number_)),
      main_list_(std::move(init.main_list_)),
      is_equal_(std::move(init.is_equal_)) {
    init.max_load_factor_ = 0;
    init.buckets_ = nullptr;
    init.bucket_number_ = 0;
  }

  // destructor
  ~UnorderedMap() {
    std::allocator_traits<BucketAlloc>::destroy(bucket_alloc, buckets_);
  }

  size_t size() const {
    return main_list_.size();
  }
  void max_load_factor(float new_factor) {
    max_load_factor_ = new_factor;
  }
  double max_load_factor() const {
    return max_load_factor_;
  }

  void rehash(size_t count) {
    std::allocator_traits<BucketAlloc>::destroy(bucket_alloc, buckets_);
    AllocateBuckets(count);
    std::vector<std::vector<int>> table(count);
    std::vector<ListTypeIterator> iterators(size());

    int i = 0;
    for (auto it = main_list_.begin(); it != main_list_.end(); ++it, ++i) {
      int new_hash = find_hash_(it->kv.first) % bucket_number_;
      table[new_hash].push_back(i);
      it->hash = new_hash;
      iterators[i] = it;
    }

    std::vector<std::pair<int, int>> order;

    for (size_t i = 0; i < count; ++i) {
      for (size_t j = 0; j < table[i].size(); ++j) {
        order.push_back({table[i][j], i});
      }
    }
    auto last_it = main_list_.end();
    for (size_t i = 0; i < order.size(); ++i) {
      main_list_.ChangeForwardLink(last_it, iterators[order[i].first]);
      main_list_.ChangeBackLink(iterators[order[i].first], last_it);
      last_it = iterators[order[i].first];
      size_t current_bucket = order[i].second;
      if (buckets_[current_bucket] == main_list_.end()) {
        buckets_[current_bucket] = last_it;
      }
    }
    auto tmp = main_list_.end();
    main_list_.ChangeForwardLink(last_it, tmp);
    main_list_.ChangeBackLink(tmp, last_it);
  }

  iterator find(const Key& key) {
    size_t current_bucket = find_hash_(key) % bucket_number_;
    auto it = Iterator<false>(buckets_[current_bucket]);
    while (it != end() && it.it->hash == current_bucket) {
      if (is_equal_(key, it->first)) {
        return it;
      }
      ++it;
    }
    return end();
  }

  const_iterator find(const Key& key) const {
    UnorderedMap* temp = this;
    return find(temp);
  }

  template <class InputIt>
  void insert(InputIt first, InputIt last) {
    while (first != last) {
      insert(*first);
      ++first;
    }
  }

  std::pair<iterator, bool> insert(const std::pair<Key, Value>& to_insert) {
    auto it = find(to_insert.first);
    if (it != end()) {
      try {
        it->second = to_insert.second;
        return std::make_pair(it, true);
      } catch (...) {
        return std::make_pair(it, false);
      }
    }
    try {
      auto it = emplace(to_insert).first;
      return std::make_pair(it, true);
    } catch (...) {
      return std::make_pair(end(), false);
    }
  }

  std::pair<iterator, bool> insert(std::pair<Key, Value>&& to_insert) {
    auto it = find(to_insert.first);
    if (it != end()) {
      try {
        it->second = std::move(to_insert.second);
        return std::make_pair(it, true);
      } catch (...) {
        return std::make_pair(it, false);
      }
    }
    try {
      auto it = emplace(std::move(to_insert)).first;
      return std::make_pair(it, true);
    } catch (...) {
      return std::make_pair(end(), false);
    }
  }

  Value& operator[](const Key& key) {
    auto it = find(key);
    if (it == end()) {
      it = insert({key, Value()}).first;
    }
    return it->second;
  }

  Value& at(const Key& key) {
    auto it = find(key);
    if (it == end()) {
      throw std::out_of_range("durak sam znaesh pochemu");
    }
    return it->second;
  }

  const Value& at(const Key& key) const {
    auto it = find(key);
    if (it == end()) {
      throw std::out_of_range("durak sam znaesh pochemu");
    }
    return it->second;
  }

  // inserts <key, value>, ignores and existence or absence of key
  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    try {
      /*
        Yes, this function is very controversial.
        I've tried to get rid of this poor code, but every time
        I did so, I had CE on the last test (because somehow a wrong allocator
        calls a constructor of object)
        I'm not sure I'll be able to fix it (even on review), so I 
        really hope that you'll let me keep it
      */
     
      // ------ start of COSTYL ------
      auto temp = main_list_.AllocNode();

      using PairAlloc = typename std::allocator_traits<typename ListType::NodeAlloc>::template rebind_alloc<NodeType>;
      PairAlloc p_alloc = PairAlloc();


      std::allocator_traits<PairAlloc>::construct(p_alloc, &(temp->value.kv), 
                  std::forward<Args>(args)...);
                
      temp->value.hash = 0;
      main_list_.emplaceForUM(main_list_.begin(), temp);
      // ---- end of COSTYL -------

      // this should be a proper line, but it fails
      // main_list_.emplace(main_list_.begin(), std::forward<Args>(args)..., 0);

      int bucket_to_insert = find_hash_(main_list_.begin()->kv.first) % bucket_number_;
      main_list_.begin()->hash = bucket_to_insert;
      if (buckets_[bucket_to_insert] == main_list_.end()) {
        buckets_[bucket_to_insert] = main_list_.begin();
      } else {
        // here you need to change a structure of a list
        // by tying vertexes
        // ------- putting iterators on places
        auto new_begin = main_list_.begin();
        ++new_begin;
        auto to_insert = main_list_.begin();
        auto last_inserted = buckets_[bucket_to_insert];
        auto vertex_before_last = buckets_[bucket_to_insert];
        --vertex_before_last;
        auto root = main_list_.end();
        // ----
        if (vertex_before_last != to_insert) {
          main_list_.ChangeForwardLink(root, new_begin);
          main_list_.ChangeBackLink(to_insert, vertex_before_last);
          main_list_.ChangeForwardLink(to_insert, last_inserted);
          main_list_.ChangeBackLink(new_begin, root);
          main_list_.ChangeForwardLink(vertex_before_last, to_insert);
          main_list_.ChangeBackLink(last_inserted, to_insert);
        }

        buckets_[bucket_to_insert] = to_insert;
      }
      double new_load_factor = double((size() + 1)) / bucket_number_;
      if (new_load_factor >= max_load_factor_ + EPS) {
        rehash(bucket_number_ * 2);
      }
      return {buckets_[bucket_to_insert], true};
    } catch (...) {
      return std::make_pair(end(), false);
    }
  }

  void erase(const Key& key) {
    size_t current_bucket = find_hash_(key) % bucket_number_;
    auto it = Iterator<true>(buckets_[current_bucket]);
    while (it != end() && it.it->hash == current_bucket) {
      if (is_equal_(key, it->first)) {
        main_list_.erase(it.it);
        ++it;
        buckets_[current_bucket] = it.it;
        if (it.it->hash != current_bucket) {
          buckets_[current_bucket] = main_list_.end();
        }
        return;
      }
      ++it;
    }
  }

  void erase(iterator it) {
    size_t current_bucket = it.it->hash;
    auto next_it = it;
    ++next_it;
    main_list_.erase(it.it);
    it = next_it;
    buckets_[current_bucket] = it.it;
    if (buckets_[current_bucket] == main_list_.end()) { return; }
    if (it.it->hash != current_bucket) {
      buckets_[current_bucket] = main_list_.end();
    }
  }

  // deletes all elements in a half-interval
  // [ begin, end )
  void erase(iterator begin, iterator end) {
    while (begin != end) {
      iterator next = begin;
      ++next;
      erase(begin);
      begin = next;
    }
  }


  void reserve(size_t count) {
    rehash(std::ceil(count / max_load_factor()));
  }

 private:
  void AllocateBuckets(int number) {
    bucket_number_ = number;
    buckets_ = std::allocator_traits<BucketAlloc>::allocate(bucket_alloc, number);
    for (int i = 0; i < number; ++i) {
      buckets_[i] = main_list_.end();
    }
  }

  double max_load_factor_ = 1;
  Alloc pair_allocator_;
  BucketAlloc bucket_alloc;
  NodeAlloc node_alloc;
  ListTypeIterator* buckets_;
  Hash find_hash_;
  size_t bucket_number_ = 5;
  ListType main_list_;
  Equal is_equal_;
};
