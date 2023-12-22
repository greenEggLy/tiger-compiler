#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <string>

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
// #include "tiger/runtime/gc/roots/roots.h"
#include "tiger/translate/tree.h"

namespace gc {
class PointerMapList;
}
namespace frame {

class RegManager {
public:
  RegManager() : temp_map_(temp::Map::Empty()) {}

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }

  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  [[nodiscard]] virtual temp::TempList *Registers() = 0;

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;

  /**
   * Get word size
   */
  [[nodiscard]] virtual int WordSize() = 0;

  [[nodiscard]] virtual temp::Temp *FramePointer() = 0;

  [[nodiscard]] virtual temp::Temp *StackPointer() = 0;

  [[nodiscard]] virtual temp::Temp *ReturnValue() = 0;

  temp::Map *temp_map_;

protected:
  std::vector<temp::Temp *> regs_;
};

class Access {
public:
  /* TODO: Put your lab5 code here */
  virtual tree::Exp *ToExp(tree::Exp *framePtr) const = 0;
  virtual ~Access() = default;
  virtual void SetInHeap(bool in_heap) = 0;
};

class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }
  void SetInHeap(bool in_heap) override{};
};

class InFrameAccess : public Access {
public:
  int offset;
  bool in_heap = false;
  explicit InFrameAccess(const int offset, const bool in_heap = false)
      : offset(offset), in_heap(in_heap) {}
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override {
    return new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, frame_ptr,
                                               new tree::ConstExp(offset)));
  }
  void SetInHeap(const bool in_heap) override { this->in_heap = in_heap; }
};

class Frame {
public:
  /* TODO: Put your lab5 code here */
  temp::Label *name_;
  //  denoting the locations where the formal parameters will be kept at run
  //  time as seen from inside the callee
  std::list<Access *> *formals_ = nullptr;
  int offset_ = 0;
  std::list<Access *> *heap_accesses_ = nullptr;

  explicit Frame(temp::Label *name) : name_(name) {
    heap_accesses_ = new std::list<Access *>();
  };
  virtual frame::Access *AllocLocal(bool escape, bool in_heap = false,
                                    Frame *frame = nullptr) = 0;
  virtual std::vector<int64_t> GetOffsets() const = 0;
  [[nodiscard]] std::list<Access *> *Formals() const { return formals_; }
  [[nodiscard]] std::string GetLabel() const { return name_->Name(); }
};

/**
 * Fragments
 */

class Frag {
public:
  virtual ~Frag() = default;

  enum OutputPhase {
    Proc,
    String,
    PointerMap,
  };

  /**
   *Generate assembly for main program
   * @param out FILE object for output assembly file
   */
  virtual void OutputAssem(FILE *out, OutputPhase phase, bool need_ra,
                           Frag **pointer_frag) const = 0;
};

class StringFrag : public Frag {
public:
  temp::Label *label_;
  std::string str_;

  StringFrag(temp::Label *label, std::string str)
      : label_(label), str_(std::move(str)) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra,
                   Frag **pointer_frag) const override;
};

class ProcFrag : public Frag {
public:
  tree::Stm *body_;
  Frame *frame_;

  ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra,
                   Frag **pointer_frag) const override;
};

class PointerMapFrag : public Frag {
public:
  gc::PointerMapList *pointer_map_list;
  PointerMapFrag(gc::PointerMapList *list) { pointer_map_list = list; }
  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra,
                   Frag **pointer_frag) const override;
};

class Frags {
public:
  Frags() = default;
  void PushBack(Frag *frag) { frags_.emplace_back(frag); }
  const std::list<Frag *> &GetList() { return frags_; }
  PointerMapFrag *UpdateNextLink();

private:
  std::list<Frag *> frags_;
};

/* TODO: Put your lab5 code here */

} // namespace frame

#endif