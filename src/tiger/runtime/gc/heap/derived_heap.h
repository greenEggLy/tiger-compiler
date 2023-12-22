#pragma once

#include "heap.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

namespace gc {
class PointerMapNode {
public:
  uint64_t label = 0;
  uint64_t next_label = 0;
  uint64_t ret_add = 0;
  uint64_t frame_size = 0;
  uint64_t in_main = 0;
  std::vector<int64_t> offsets;
  PointerMapNode() = default;
};
class PointerMapManager {
  std::vector<PointerMapNode> pointer_map_;

public:
  PointerMapManager() { Init(); }
  void Init() {
    uint64_t *cur = &GLOBAL_GC_ROOTS;
    while (true) {
      PointerMapNode node;
      node.label = reinterpret_cast<uint64_t>(cur);
      node.next_label = *(cur++);
      node.ret_add = *(cur++);
      node.frame_size = *(cur++);
      node.in_main = *(cur++);
      while (true) {
        int64_t offset = *(cur++);
        if (offset == -1)
          break;
        node.offsets.emplace_back(offset);
      }
      pointer_map_.emplace_back(node);
      if (node.next_label == 0)
        break;
    }
  }

  std::vector<uint64_t> GetRootAddress(uint64_t *sp) {
    std::vector<uint64_t> address;
    bool in_main = false;
    while (!in_main) {
      uint64_t ret_add = *(sp - 1);
      for (auto &&pm : pointer_map_) {
        if (pm.ret_add == ret_add) {
          for (const int64_t offset : pm.offsets) {
            const auto pointer_add = reinterpret_cast<uint64_t *>(
                offset + reinterpret_cast<int64_t>(sp) +
                static_cast<int64_t>(pm.frame_size));
            address.emplace_back(*pointer_add);
          }
          sp += (pm.frame_size / 8 + 1);
          in_main = pm.in_main;
          break;
        }
      }
    }
    return address;
  }
};

class HeapManger {
public:
  HeapManger() = default;
  void SetFree(uint64_t loc, uint64_t size) { free_map_[loc] = size; }
  char *Alloc(uint64_t size) {
    auto iter = free_map_.begin();
    for (; iter != free_map_.end(); ++iter) {
      // auto [loc, free_size] = *iter;
      const auto loc = iter->first;
      const auto free_size = iter->second;
      // fprintf(stderr, "free size: %zd, size: %zd\n", free_size, size);
      if (free_size > size) {
        // can have a new free space
        free_map_[free_size + size] = free_size - size;
        used_map_[loc] = size;
        break;
      }
      if (free_size == size) {
        used_map_[loc] = size;
        break;
      }
    }
    if (iter != free_map_.end()) {
      free_map_.erase(iter);
      return reinterpret_cast<char *>(iter.operator*().first);
    }
    return nullptr;
  }
  uint64_t Used() const {
    uint64_t used = 0;
    for (const auto &map : used_map_) {
      const auto used_size = map.second;
      used += used_size;
    }
    return used;
  }
  uint64_t MaxFree() const {
    uint64_t max_size = 0;
    for (const auto &map : free_map_) {
      const auto free_size = map.second;
      max_size = std::max(max_size, free_size);
    }
    return max_size;
  }

private:
  std::map<uint64_t, uint64_t> free_map_;
  std::map<uint64_t, uint64_t> used_map_;
};

class RecordInfo {
public:
  char *start;
  uint64_t size;
  bool marked = false;
  unsigned char *descriptor_name = nullptr;
  uint64_t descriptor_size = 0;

  RecordInfo() {
    start = nullptr;
    size = 0;
  }
  RecordInfo(char *start, uint64_t size, unsigned char *des_name,
             uint64_t des_size)
      : start(start), size(size), descriptor_name(des_name),
        descriptor_size(des_size) {}
  bool InRecord(const uint64_t address) const {
    if (size <= 0)
      return false;
    const auto words =
        (static_cast<int64_t>(address) - reinterpret_cast<int64_t>(start));
    return words >= 0 && words <= size;
  }
  bool Marked() const { return marked; }
  void Mark() { marked = true; }
  void Unmark() { marked = false; }
};

class ArrayInfo {
public:
  char *start;
  uint64_t size;
  bool marked = false;

  ArrayInfo() {
    start = nullptr;
    size = 0;
  }
  ArrayInfo(char *start, uint64_t size) : start(start), size(size) {}
  bool InArray(const uint64_t address) const {
    if (size <= 0)
      return false;
    const auto words =
        (static_cast<int64_t>(address) - reinterpret_cast<int64_t>(start));
    return words >= 0 && words <= size;
  }
  bool Marked() const { return marked; }
  void Mark() { marked = true; }
  void Unmark() { marked = false; }
};

class DerivedHeap : public TigerHeap {
public:
  DerivedHeap() = default;

  char *Allocate(uint64_t size) override;

  char *AllocRecord(uint64_t size, unsigned char *descriptor,
                    uint64_t descriptor_size) override;
  char *AllocArray(uint64_t size) override;

  uint64_t Used() const override;

  uint64_t MaxFree() const override;

  void Initialize(uint64_t size) override;

  void GC() override;

  void Mark();

  void Sweep();

  bool InHeap(uint64_t x) const;

  bool InHeapAndMark(uint64_t x, RecordInfo **info);

  void DFS(uint64_t x);

private:
  char *heap_ = nullptr;
  uint64_t heap_size_ = 0;
  HeapManger heap_manger_;
  std::vector<RecordInfo> records_;
  std::vector<ArrayInfo> arrays_;
  PointerMapManager pm_manager;
};

} // namespace gc
