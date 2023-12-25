#include "derived_heap.h"

namespace gc {

char *DerivedHeap::Allocate(const uint64_t size) {
  return heap_manger_.Alloc(size);
}
char *DerivedHeap::AllocRecord(const uint64_t size, unsigned char *descriptor,
                               const uint64_t descriptor_size) {
  const auto start = Allocate(size);
  if (start == nullptr) {
    return nullptr;
  }
  auto record = RecordInfo(start, size, descriptor, descriptor_size);
  records_.emplace_back(record);
  return start;
}
char *DerivedHeap::AllocArray(const uint64_t size) {
  const auto start = Allocate(size);
  if (!start) {
    return nullptr;
  }
  auto array = ArrayInfo(start, size);
  arrays_.emplace_back(array);
  return start;
}

uint64_t DerivedHeap::Used() const { return heap_manger_.Used(); }
uint64_t DerivedHeap::MaxFree() const {
  const auto max_size = heap_manger_.MaxFree();
  if (heap_size_ / 2 > max_size) {
    fprintf(stderr, "max free size too small\n");
  }
  return max_size;
}
void DerivedHeap::Initialize(const uint64_t size) {
  heap_ = static_cast<char *>(malloc(size));
  heap_size_ = size;
  heap_manger_.SetFree(reinterpret_cast<uint64_t>(heap_), size);
}
void DerivedHeap::GC() {
  GET_TIGER_STACK(this->stack);
  Mark(this->stack);
  Sweep();
}
void DerivedHeap::Mark(uint64_t *sp) {
  const std::vector<uint64_t> roots_address = pm_manager.GetRootAddress(sp);
  for (auto &address : roots_address) {
    DFS(address);
  }
}
void DerivedHeap::Sweep() {
  auto array_iter = arrays_.begin();
  while (array_iter != arrays_.end()) {
    if (array_iter->Marked()) {
      array_iter->Unmark();
      ++array_iter;
    } else {
      heap_manger_.SetFree(reinterpret_cast<uint64_t>(array_iter->start),
                           array_iter->size);
      array_iter = arrays_.erase(array_iter);
    }
  }
  auto record_iter = records_.begin();
  while (record_iter != records_.end()) {
    if (record_iter->Marked()) {
      record_iter->Unmark();
      ++record_iter;
    } else {
      heap_manger_.SetFree(reinterpret_cast<uint64_t>(record_iter->start),
                           record_iter->size);
      record_iter = records_.erase(record_iter);
    }
  }
}
bool DerivedHeap::InHeap(const uint64_t x) const {
  for (const auto &record : records_) {
    if (record.InRecord(x))
      return true;
  }
  for (const auto &array : arrays_) {
    if (array.InArray(x))
      return true;
  }
  return false;
}
bool DerivedHeap::InHeapAndMark(const uint64_t x, RecordInfo **info) {
  for (auto &&record : records_) {
    if (record.IsRecordStart(x)) {
      if (record.Marked()) {
        return false;
      }
      record.Mark();
      *info = &record;
      return true;
    }
  }
  for (auto &&array : arrays_) {
    if (array.InArray(x)) {
      if (array.Marked())
        return false;
      array.Mark();
      return true;
    }
  }
  return false;
}
void DerivedHeap::DFS(const uint64_t x) {
  RecordInfo *record_info = nullptr;
  if (InHeapAndMark(x, &record_info)) {
    if (record_info) {
      const auto start = reinterpret_cast<uint64_t>(record_info->start);
      const auto size = record_info->descriptor_size;
      for (uint64_t i = 0; i < size; ++i) {
        const auto f = reinterpret_cast<uint64_t *>(start + WORD_SIZE * i);
        if (const uint64_t field = start + WORD_SIZE * i; record_info->descriptor_name[i] == '1') {
          DFS(*f);
        }
      }
    }
  }
}

} // namespace gc
