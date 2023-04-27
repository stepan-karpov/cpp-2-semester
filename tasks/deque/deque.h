
#pragma once

#include <iostream>
#include <vector>
#include <utility>

template <typename T>
class Deque {
 private:
  void DeleteElements() {
    for (size_t i = 0; i < deque_size; ++i) {
      (chain_array[row(i + first_element)] + column(i + first_element))->~T();
    }
  }

  void DeleteChunks() {
    for (size_t i = 0; i < chain_size; ++i) {
      delete[] reinterpret_cast<char*>(chain_array[i]);
    }
  }

  void IncreaseBackCapacity(size_t new_chain_size) {
    if (chain_size >= new_chain_size)
      return;

    T** new_chain_array = new T*[new_chain_size];

    for (size_t i = 0; i < chain_size; ++i) {
      new_chain_array[i] = chain_array[i];
    }

    int added_chunks = 0;
    try {
      for (size_t i = chain_size; i < new_chain_size; ++i) {
        new_chain_array[i] = allocate_raw_memory();
        ++added_chunks;
      }
    } catch (...) {
      for (size_t i = chain_size; i < chain_size + added_chunks; ++i) {
        delete[] reinterpret_cast<char*>(new_chain_array[i]);
      }
      delete[] new_chain_array;
      throw;
    }

    delete[] chain_array;
    chain_array = new_chain_array;
    chain_size = new_chain_size;
  }

  void swap(Deque init) {
    std::swap(chain_array, init.chain_array);
    std::swap(chain_size, init.chain_size);
    std::swap(first_element, init.first_element);
    std::swap(deque_size, init.deque_size);
  }

  void IncreaseFrontCapacity(size_t new_chain_size) {
    if (chain_size > new_chain_size) {
      throw(
          "you're doing something strange "
          "why do you want to decrease capacity"
          "with increasing method???");
    } else if (chain_size == new_chain_size)
      return;

    T** new_chain_array = new T*[new_chain_size];

    size_t start_elements = new_chain_size - chain_size;

    int added_chunks = 0;
    try {
      for (size_t i = 0; i < start_elements; ++i) {
        new_chain_array[i] =
            reinterpret_cast<T*>(new char[CHUNK_SIZE * sizeof(T)]);
        ++added_chunks;
      }
    } catch (...) {
      for (size_t i = 0; i < added_chunks; ++i) {
        delete[] reinterpret_cast<char*>(new_chain_array[i]);
      }
      delete[] new_chain_array;
      throw;
    }

    for (size_t i = start_elements; i < new_chain_size; ++i) {
      new_chain_array[i] = chain_array[i - start_elements];
    }

    delete[] chain_array;
    chain_array = new_chain_array;
    chain_size = new_chain_size;
    first_element = start_elements * CHUNK_SIZE + first_element;
  }

  T* allocate_raw_memory() {
    return reinterpret_cast<T*>(new char[CHUNK_SIZE * sizeof(T)]);
  }

  void Clear() {
    DeleteElements();
    DeleteChunks();
  }

  static const size_t CHUNK_SIZE = 32;
  size_t chain_size = 0;
  T** chain_array = nullptr;
  size_t deque_size = 0;
  int first_element = 0;

 public:
  static size_t row(size_t index) {
    return index / CHUNK_SIZE;
  };

  static size_t column(size_t index) {
    return index % CHUNK_SIZE;
  };

  template <bool is_constant>
  class Iterator {
   private:
    size_t position;
    T** object_chain;

   public:
    using value_type = T;
    using stor_type = std::conditional<is_constant, const T**, T**>;
    using pointer = std::conditional_t<is_constant, const T*, T*>;
    using reference = std::conditional_t<is_constant, const T&, T&>;
    using difference_type = ptrdiff_t;

    Iterator(size_t position, T** object_chain)
        : position(position),
          object_chain(object_chain) {
    }

    Iterator() = default;

    reference operator*() const {
      return *(object_chain[row(position)] + column(position));
    }
    pointer operator->() const {
      return object_chain[row(position)] + column(position);
    }

    Iterator& operator++() {
      ++position;
      return *this;
    }

    Iterator operator++(int) {
      Iterator copy = *this;
      ++*this;
      return copy;
    }

    Iterator& operator--() {
      --position;
      return *this;
    }

    Iterator operator--(int) {
      Iterator copy = *this;
      --*this;
      return copy;
    }

    bool operator==(const Iterator& it) const {
      return position == it.position;
    }
    bool operator!=(const Iterator& it) const {
      return position != it.position;
    }
    bool operator<(const Iterator& it) const {
      return position < it.position;
    }
    bool operator<=(const Iterator& it) const {
      return position <= it.position;
    }
    bool operator>(const Iterator& it) const {
      return position > it.position;
    }
    bool operator>=(const Iterator& it) const {
      return position >= it.position;
    }

    reference operator[](size_t delta) const {
      int new_position = position + delta;
      return object_chain[row(new_position)][column(new_position)];
    }

    Iterator& operator-=(difference_type delta) {
      position -= delta;
      return *this;
    }

    Iterator& operator+=(difference_type delta) {
      position += delta;
      return *this;
    }

    Iterator& operator-=(const Iterator& it) {
      position -= it.position;
      return *this;
    }

    Iterator& operator+=(const Iterator& it) {
      position += it.position;
      return *this;
    }

    difference_type operator-(const Iterator& other) const {
      return position - other.position;
    }

    friend Iterator operator+(Iterator other, difference_type delta) {
      other.position += delta;
      return other;
    }

    friend Iterator operator-(Iterator other, difference_type delta) {
      other.position -= delta;
      return other;
    }

    friend Iterator operator+(difference_type delta, Iterator other) {
      return (other + delta);
    }

    friend Iterator operator-(difference_type delta, Iterator other) {
      return (other - delta);
    }

    operator Iterator<true>() const {
      return Iterator<true>(position, object_chain);
    }

    friend void Deque::insert(Iterator<false> it, const T& value);
    friend void Deque::erase(Iterator<false> it);
  };

  typedef Iterator<false> iterator;
  typedef Iterator<true> const_iterator;

  iterator begin() {
    return Iterator<false>(first_element, chain_array);
  }
  iterator end() {
    return Iterator<false>(first_element + deque_size, chain_array);
  }

  const_iterator begin() const {
    return Iterator<true>(first_element, chain_array);
  }
  const_iterator end() const {
    return Iterator<true>(first_element + deque_size, chain_array);
  }

  const_iterator cbegin() {
    return Iterator<true>(first_element, chain_array);
  }
  const_iterator cend() {
    return Iterator<true>(first_element + deque_size, chain_array);
  }

  std::reverse_iterator<iterator> rbegin() {
    return std::reverse_iterator(end());
  }
  std::reverse_iterator<iterator> rend() {
    return std::reverse_iterator(begin());
  }

  std::reverse_iterator<const_iterator> crbegin() const {
    return std::reverse_iterator(cend());
  }
  std::reverse_iterator<const_iterator> crend() const {
    return std::reverse_iterator(cbegin());
  }

  void insert(iterator it, const T& value) {
    T current_value = value;
    while (it != this->end()) {
      std::swap(current_value, *(it));
      ++it;
    }
    this->push_back(current_value);
  }

  void erase(iterator it) {
    while ((it + 1) < this->end()) {
      std::swap(*it, *(it + 1));
      ++it;
    }
    pop_back();
  }

  Deque() = default;

  Deque(size_t new_size)
      : chain_size((new_size + CHUNK_SIZE - 1) / CHUNK_SIZE),
        chain_array(new T*[chain_size]) {
    for (size_t i = 0; i < chain_size; ++i) {
      chain_array[i] = allocate_raw_memory();
    }

    // if constexpr (std::is_default_constructible<T>()) {
    try {
      first_element = 0;
      for (size_t i = 0; i < new_size; ++i) {
        new (chain_array[row(i)] + column(i)) T();
        ++deque_size;
      }
    } catch (...) {
      Clear();
      delete[] chain_array;
      chain_size = deque_size = 0;
      first_element = -1;
      throw;
    }
    // }
  }

  Deque(size_t new_size, const T& value)
      : chain_size((new_size + CHUNK_SIZE - 1) / CHUNK_SIZE),
        chain_array(new T*[chain_size]) {
    for (size_t i = 0; i < chain_size; ++i) {
      chain_array[i] = allocate_raw_memory();
    }

    try {
      first_element = 0;
      for (size_t i = 0; i < new_size; ++i) {
        new (chain_array[row(i)] + column(i)) T(value);
        ++deque_size;
      }
    } catch (...) {
      Clear();
      delete[] chain_array;
      chain_size = deque_size = 0;
      first_element = -1;
      throw;
    }
  }

  Deque(const Deque& init)
      : chain_size(init.chain_size),
        chain_array(new T*[chain_size]),
        first_element(init.first_element) {
    for (size_t i = 0; i < chain_size; ++i) {
      chain_array[i] = allocate_raw_memory();
    }

    try {
      first_element = 0;
      for (size_t i = 0; i < init.deque_size; ++i) {
        new (chain_array[row(i + first_element)] + column(i + first_element))
            T(*(init.chain_array[row(i + first_element)] +
                column(i + first_element)));
        ++deque_size;
      }
    } catch (...) {
      Clear();
      delete[] chain_array;
      chain_size = deque_size = 0;
      first_element = -1;
      throw;
    }
  }

  ~Deque() {
    Clear();
    delete[] chain_array;
  }

  Deque& operator=(const Deque& init) {
    Deque copy(init);
    swap(init);
    return *this;
  }

  size_t size() const {
    return deque_size;
  }

  T& operator[](size_t index) {
    return chain_array[row(index + first_element)]
                      [column(index + first_element)];
  }

  const T& operator[](size_t index) const {
    return chain_array[row(index + first_element)]
                      [column(index + first_element)];
  }

  T& at(size_t index) {
    if (!(0 <= index && index < deque_size)) {
      throw std::out_of_range("durak");
    }
    return chain_array[row(index + first_element)]
                      [column(index + first_element)];
  }

  const T& at(size_t index) const {
    if (!(0 <= index && index < deque_size)) {
      throw std::out_of_range("loh");
    }
    return chain_array[row(index)][column(index)];
  }

  void push_back(const T& value) {
    size_t capacity = chain_size * CHUNK_SIZE;
    size_t to_insert = first_element + deque_size;
    if (to_insert >= capacity) {
      IncreaseBackCapacity((chain_size == 0) ? 1 : 2 * chain_size);
    }
    new (chain_array[row(to_insert)] + column(to_insert)) T(value);
    ++deque_size;
  }

  void push_front(const T& value) {
    if (first_element == 0) {
      IncreaseFrontCapacity((chain_size == 0) ? 1 : 2 * chain_size);
    }
    size_t to_insert = first_element - 1;
    new (chain_array[row(to_insert)] + column(to_insert)) T(value);
    ++deque_size;
    --first_element;
  }

  void pop_back() {
    size_t to_delete = first_element + deque_size - 1;
    (chain_array[row(to_delete)] + column(to_delete))->~T();
    --deque_size;
  }

  void pop_front() {
    size_t to_delete = first_element;
    (chain_array[row(to_delete)] + column(to_delete))->~T();
    --deque_size;
    ++first_element;
  }
};