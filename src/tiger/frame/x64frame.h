//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {

using TempPtr = temp::Temp *;
class X64RegManager : public RegManager {
public:
  X64RegManager();
  temp::TempList *Registers() override;
  temp::TempList *ArgRegs() override;
  temp::TempList *CallerSaves() override;
  temp::TempList *CalleeSaves() override;
  temp::TempList *ReturnSink() override;
  int WordSize() override { return 8; }
  temp::Temp *FramePointer() override;
  temp::Temp *StackPointer() override;
  temp::Temp *ReturnValue() override;

public:
  //%rax，%rbx，%rcx，%rdx，%esi，
  // %edi，%rbp，%rsp，%r8，%r9，
  // %r10，%r11，%r12，%r13，%r14，%r15
  TempPtr rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8, r9, r10, r11, r12, r13,
      r14, r15;
};

class X64Frame : public Frame {
public:
  const int word_size_ = 8;

public:
  X64Frame(temp::Label *name, const std::list<bool> &formals)
      : Frame(name, formals) {
    formals_ = new std::list<Access *>();
    for (const auto &formal : formals) {
      formals_->emplace_back(this->AllocLocal(formal));
    }
  }
  frame::Access *AllocLocal(bool escape) override;
};

tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm);
assem::InstrList *ProcEntryExit2(assem::InstrList *body);
assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body);

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
