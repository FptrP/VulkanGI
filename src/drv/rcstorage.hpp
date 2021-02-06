#ifndef RCSTORAGE_HPP_INCLUDED
#define RCSTORAGE_HPP_INCLUDED

#include <vector>
#include <list>

template <typename T>
struct RCId;

template <typename T>
struct RCStorage {

  RCId<T> create(const T& handle) {
    u32 index;
    if (empty_cells.size()) {
      index = empty_cells.front();
      empty_cells.pop_front();
      elems[index].handle = handle;
    } else {
      index = elems.size();
      elems.push_back({handle, 0});
    }

    elems[index].references++;
    return {this, index};
  }

  RCId<T> create(T&& handle) {
    u32 index;
    if (empty_cells.size()) {
      index = empty_cells.front();
      empty_cells.pop_front();
      elems[index].handle = std::move(handle);
    } else {
      index = elems.size();
      elems.push_back({std::move(handle), 0});
    }

    elems[index].references++;
    return {this, index};
  }

  template <typename... Args> 
  void collect(Args&... args) {
    while (delayed_free.size()) {
      auto index = delayed_free.front();
      elems[index].handle.release(args...);
      delayed_free.pop_front();
      empty_cells.push_front(index);
    }
  }

private:
  friend RCId<T>;

  void inc_ref(const RCId<T> &id) {
    elems[id.index].references++;
  }
  
  void dec_ref(const RCId<T> &id) {
    elems[id.index].references--;
    if (elems[id.index].references == 0) {
      delayed_free.push_front(id.index);
    }
  }

  const T& get(const RCId<T> &id) const {
    return elems[id.index].handle;
  }

  T& get(const RCId<T> &id) {
    return elems[id.index].handle;
  }

  struct Elem {
    T handle;
    u32 references;
  };

  std::vector<Elem> elems;
  std::list<u32> empty_cells;
  std::list<u32> delayed_free;
};

template <typename T>
struct RCId {
  RCId() {}
  RCId(const RCId &id) : storage {id.storage}, index {id.index} {
    storage->inc_ref(id);
  }

  RCId(RCId &&id) : storage {id.storage}, index {id.index} {
    id.storage = nullptr;
  }

  const RCId& operator=(const RCId<T> &id) {
    if (storage) {
      storage->dec_ref(*this);
    }

    storage = id.storage;
    index = id.index;
    storage->inc_ref(*this);
    return *this;
  }

  const RCId& operator=(RCId<T> &&id) {
    if (storage) {
      storage->dec_ref(*this);
    }

    storage = id.storage;
    index = id.index;
    id.storage = nullptr; 
    return *this;
  }

  ~RCId() {
    if (storage) storage->dec_ref(*this);
    storage = nullptr;
  }

  bool operator==(const RCId<T> &id) {
    return (storage == id.storage) && (index == id.index);
  }

  const T& operator*() const {
    return storage->get(*this);
  }

  T& operator*() {
    return storage->get(*this);
  }

  T* operator->() { return &storage->get(*this); }
  const T* operator->() const { return &storage->get(*this); }
  
  void release() {
    if (storage) storage->dec_ref(*this);
    storage = nullptr;
  }
  u32 debug_index() const { return index; }
private:
  friend RCStorage<T>;
  
  RCId(RCStorage<T> *s, u32 i) : storage{s}, index {i} {}

  RCStorage<T> *storage = nullptr;
  u32 index = ~0;
};

#endif