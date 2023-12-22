#include "derived_heap.h"

namespace gc {

char *DerivedHeap::Allocate(const uint64_t size) {
  return heap_manger_.Alloc(size);
}
char *DerivedHeap::AllocRecord(const uint64_t size, unsigned char *descriptor,
                               const uint64_t descriptor_size) {
  fprintf(stderr, "alloc record: %zd\n", size);
  const auto start = Allocate(size);
  if (start == nullptr) {
    fprintf(stderr, "nullptr\n");
    return nullptr;
  }
  auto record = RecordInfo(start, size, descriptor, descriptor_size);
  records_.emplace_back(record);
  fprintf(stderr, "end alloc record\n");
  return start;
}
char *DerivedHeap::AllocArray(const uint64_t size) {
  // fprintf(stderr, "alloc array: %zd\n", size);
  const auto start = Allocate(size);
  if (!start) {
    // fprintf(stderr, "nullptr\n");
    return nullptr;
  }
  auto array = ArrayInfo(start, size);
  arrays_.emplace_back(array);
  // fprintf(stderr, "end alloc array\n");
  return start;
}

uint64_t DerivedHeap::Used() const { return heap_manger_.Used(); }
uint64_t DerivedHeap::MaxFree() const {
  const auto max_size = heap_manger_.MaxFree();
  if (heap_size_ / 2 > max_size) {
    fprintf(stderr, "max free size too small\n");
  }
  // fprintf(stderr, "max free size is %zd\n", max_size);
  return max_size;
}
void DerivedHeap::Initialize(const uint64_t size) {
  // fprintf(stderr, "initialize: %zd\n", size);
  heap_ = static_cast<char *>(malloc(size));
  heap_size_ = size;
  heap_manger_.SetFree(reinterpret_cast<uint64_t>(heap_), size);
}
void DerivedHeap::GC() {
  Mark();
  Sweep();
}
void DerivedHeap::Mark() {
  const std::vector<uint64_t> roots_address = pm_manager.GetRootAddress(stack);
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
    if (record.InRecord(x)) {
      if (record.Marked())
        return false;
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
        const uint64_t field = start + WORD_SIZE * i;
        DFS(field);
      }
    }
  }
}

} // namespace gc
