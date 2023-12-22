#ifndef TIGER_RUNTIME_GC_ROOTS_H
#define TIGER_RUNTIME_GC_ROOTS_H

#include "tiger/frame/frame.h"
#include <iostream>
#include <sstream>
#include <vector>

#define GC_ENABLED
// namespace frame {
// class Frame;
// }
namespace gc {

const std::string GC_ROOTS = "GLOBAL_GC_ROOTS";

class PointerMap {
public:
  std::string label;
  std::string next_label;
  std::string ret_add;
  std::string frame_size;
  bool in_main;
  std::vector<std::string> offsets;
  std::string end;

  PointerMap() = default;
};

class PointerMapList {
  std::vector<PointerMap> roots;

public:
  PointerMapList() = default;
  std::vector<PointerMap> *GetRoots() { return &roots; }
  void AddRoot(PointerMap &content) { roots.emplace_back(content); }
  void AddRoots(PointerMapList *content) {
    const auto vec = content->GetRoots();
    roots.insert(roots.end(), vec->begin(), vec->end());
  }
  void UpdateNext() {
    auto iter = roots.begin();
    for (auto next = iter + 1; next != roots.end(); ++iter, ++next) {
      iter->next_label = next->label;
    }
  }
  void Print(FILE *out) const {
    std::stringstream ss;
    for (const auto &[label, next_label, ret_add, frame_size, in_main, offsets,
                      end] : roots) {
      ss << label << ":\n";
      ss << ".quad " << next_label << "\n";
      ss << ".quad " << ret_add << "\n";
      ss << ".quad " << frame_size << "\n";
      ss << ".quad " << in_main << "\n";
      for (const auto &offset : offsets) {
        ss << ".quad " << offset << "\n";
      }
      ss << ".quad " << end << "\n";
    }
    const auto str = ss.str();
    fprintf(out, "%s", str.c_str());
  }
};

class Roots {
  // Todo(lab7): define some member and methods here to keep track of gc roots;
  PointerMapList *pointer_map_list_;
  frame::Frame *frame_;
  assem::InstrList *instr_list_;

public:
  Roots(frame::Frame *frame, assem::InstrList *instr)
      : frame_(frame), instr_list_(instr) {
    pointer_map_list_ = new PointerMapList();
  }
  PointerMapList *GetList() const { return pointer_map_list_; }
  void FillList() {
    bool reted = false;
    for (const auto &instr : instr_list_->GetList()) {
      if (!reted && typeid(*instr) == typeid(assem::OperInstr)) {
        if (auto content = static_cast<assem::OperInstr *>(instr)->assem_;
            content.find("callq") != content.npos) {
          continue;
        }
        reted = true;
        continue;
      }
      if (reted && typeid(*instr) == typeid(assem::LabelInstr)) {
        PointerMap pmap;
        pmap.ret_add = static_cast<assem::LabelInstr *>(instr)->label_->Name();
        pmap.label = "L" + pmap.ret_add;
        pmap.frame_size = frame_->name_->Name() + "_framesize";
        pmap.next_label = "0";
        if (frame_->name_->Name() == "tigermain") {
          pmap.in_main = true;
        } else {
          pmap.in_main = false;
        }
        for (const int64_t offset : frame_->GetOffsets()) {
          pmap.offsets.emplace_back(std::to_string(offset));
        }
        pmap.end = "-1";
        pointer_map_list_->AddRoot(pmap);
      }
    }
  }
};

} // namespace gc

#endif // TIGER_RUNTIME_GC_ROOTS_H