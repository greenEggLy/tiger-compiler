#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override {
    return new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, frame_ptr,
                                               new tree::ConstExp(offset)));
  }
};

class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }
};

frame::Access *X64Frame::AllocLocal(const bool escape) {
  if (escape) {
    offset_ -= reg_manager->WordSize();
    return new InFrameAccess(offset_);
  } else {
    auto reg = temp::TempFactory::NewTemp();
    return new InRegAccess(reg);
  }
}

temp::TempList *X64RegManager::Registers() {
  auto temp_list = new temp::TempList();
  temp_list->Append(rax);
  temp_list->Append(rbx);
  temp_list->Append(rcx);
  temp_list->Append(rdx);
  temp_list->Append(rsi);
  temp_list->Append(rdi);
  temp_list->Append(rbp);
  temp_list->Append(rsp);
  temp_list->Append(r8);
  temp_list->Append(r9);
  temp_list->Append(r10);
  temp_list->Append(r11);
  temp_list->Append(r12);
  temp_list->Append(r13);
  temp_list->Append(r14);
  temp_list->Append(r15);
  return temp_list;
}
temp::TempList *X64RegManager::ArgRegs() {
  auto temp_list = new temp::TempList();
  temp_list->Append(rdi);
  temp_list->Append(rsi);
  temp_list->Append(rdx);
  temp_list->Append(rcx);
  temp_list->Append(r8);
  temp_list->Append(r9);
  return temp_list;
}
temp::TempList *X64RegManager::CallerSaves() {
  auto temp_list = new temp::TempList();
  temp_list->Append(rax);
  temp_list->Append(rdi);
  temp_list->Append(rsi);
  temp_list->Append(rdx);
  temp_list->Append(rcx);
  temp_list->Append(r8);
  temp_list->Append(r9);
  temp_list->Append(r10);
  temp_list->Append(r11);
  return temp_list;
}
temp::TempList *X64RegManager::CalleeSaves() {
  auto temp_list = new temp::TempList();
  temp_list->Append(rbx);
  temp_list->Append(rbp);
  temp_list->Append(r12);
  temp_list->Append(r13);
  temp_list->Append(r14);
  temp_list->Append(r15);
  return temp_list;
}
// todo: what's this
temp::TempList *X64RegManager::ReturnSink() { return nullptr; }
temp::Temp *X64RegManager::FramePointer() { return rbp; }
temp::Temp *X64RegManager::StackPointer() { return rsp; }
temp::Temp *X64RegManager::ReturnValue() { return rax; }
X64RegManager::X64RegManager() {
  //%rax，%rbx，%rcx，%rdx，%esi，%edi，%rbp，%rsp，%r8，%r9，%r10，%r11，%r12，%r13，%r14，%r15
  rax = temp::TempFactory::NewTemp();
  rbx = temp::TempFactory::NewTemp();
  rcx = temp::TempFactory::NewTemp();
  rdx = temp::TempFactory::NewTemp();
  rsi = temp::TempFactory::NewTemp();
  rdi = temp::TempFactory::NewTemp();
  rbp = temp::TempFactory::NewTemp();
  rsp = temp::TempFactory::NewTemp();
  r8 = temp::TempFactory::NewTemp();
  r9 = temp::TempFactory::NewTemp();
  r10 = temp::TempFactory::NewTemp();
  r11 = temp::TempFactory::NewTemp();
  r12 = temp::TempFactory::NewTemp();
  r13 = temp::TempFactory::NewTemp();
  r14 = temp::TempFactory::NewTemp();
  r15 = temp::TempFactory::NewTemp();
  temp_map_->Enter(rax, new std::string("rax"));
  temp_map_->Enter(rbx, new std::string("rbx"));
  temp_map_->Enter(rcx, new std::string("rcx"));
  temp_map_->Enter(rdx, new std::string("rdx"));
  temp_map_->Enter(rsi, new std::string("esi"));
  temp_map_->Enter(rdi, new std::string("edi"));
  temp_map_->Enter(rbp, new std::string("rbp"));
  temp_map_->Enter(rsp, new std::string("rsp"));
  temp_map_->Enter(r8, new std::string("r8"));
  temp_map_->Enter(r9, new std::string("r9"));
  temp_map_->Enter(r10, new std::string("r10"));
  temp_map_->Enter(r11, new std::string("r11"));
  temp_map_->Enter(r12, new std::string("r12"));
  temp_map_->Enter(r13, new std::string("r13"));
  temp_map_->Enter(r14, new std::string("r14"));
  temp_map_->Enter(r15, new std::string("r15"));
}
} // namespace frame