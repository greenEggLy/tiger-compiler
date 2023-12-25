#include "tiger/frame/x64frame.h"

#include "tiger/absyn/absyn.h"

#include <sstream>

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */

frame::Access *X64Frame::AllocLocal(const bool escape, const bool in_heap,
                                    Frame *frame) {
#ifdef GC_ENABLED
  if (!escape && in_heap) {
    assert(frame != nullptr);
    auto access = new InFrameAccess(AllocLocal(), true);
    frame->heap_accesses_->emplace_back(access);
    return access;
  }
#endif
  if (escape) {
    offset_ -= reg_manager->WordSize();
    const auto in_frame_acc = new InFrameAccess(offset_);
    if (in_heap) {
      frame->heap_accesses_->push_back(in_frame_acc);
    }
    return in_frame_acc;
  } else {
    auto reg = temp::TempFactory::NewTemp();
    return new InRegAccess(reg);
  }
}
int X64Frame::AllocLocal() {
  offset_ -= reg_manager->WordSize();
  return offset_;
}
std::vector<int64_t> X64Frame::GetOffsets() const {
  std::vector<int64_t> ret;
  for (auto &access : *heap_accesses_) {
    if (typeid(*access) == typeid(InFrameAccess)) {
      if (const auto inf_acc = dynamic_cast<InFrameAccess *>(access);
          inf_acc->in_heap) {
        ret.emplace_back(inf_acc->offset);
      }
    }
  }
  return ret;
}
tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm) {

  tree::Stm *callee_stm = new tree::ExpStm(new tree::ConstExp(0));
  const auto callee_saves = new temp::TempList();
  for (const auto &reg : reg_manager->CalleeSaves()->GetList()) {
    const auto dst = temp::TempFactory::NewTemp();
    const auto move_stm =
        new tree::MoveStm(new tree::TempExp(dst), new tree::TempExp(reg));
    callee_stm = new tree::SeqStm(callee_stm, move_stm);
    callee_saves->Append(dst);
  }

  const auto arg_reg_list = reg_manager->ArgRegs()->GetList();
  const auto frame_formal_list = frame->formals_;
  auto formal_it = frame_formal_list->begin();
  const auto size = arg_reg_list.size() < frame_formal_list->size()
                        ? arg_reg_list.size()
                        : frame_formal_list->size();
  tree::Stm *total_stm = nullptr;
  for (int i = 0; i < size; ++i) {
    const auto formal = formal_it.operator*();
    const auto formal_exp =
        formal->ToExp(new tree::TempExp(reg_manager->FramePointer()));
    const auto ith_reg = reg_manager->ArgRegs()->NthTemp(i);
    //    auto move_stm = new tree::MoveStm(new tree::TempExp(ith_reg),
    //    formal_exp);
    const auto move_stm =
        new tree::MoveStm(formal_exp, new tree::TempExp(ith_reg));
    if (!total_stm)
      total_stm = move_stm;
    else
      total_stm = new tree::SeqStm(total_stm, move_stm);
    ++formal_it;
  }
  int i = 1;
  while (formal_it != frame_formal_list->end()) {
    // frame access
    const auto formal = formal_it.operator*();
    const auto formal_mem = new tree::MemExp(new tree::BinopExp(
        tree::BinOp::PLUS_OP, new tree::TempExp(reg_manager->FramePointer()),
        new tree::ConstExp((++i) * reg_manager->WordSize())));
    //    auto move_stm = new tree::MoveStm(
    //        formal_mem,
    //        formal->ToExp(new tree::TempExp(reg_manager->FramePointer())));
    const auto move_stm = new tree::MoveStm(
        formal->ToExp(new tree::TempExp(reg_manager->FramePointer())),
        formal_mem);
    if (!total_stm)
      total_stm = move_stm;
    else
      total_stm = new tree::SeqStm(total_stm, move_stm);
    ++formal_it;
  }
  auto res_stm = new tree::SeqStm(callee_stm, new tree::SeqStm(total_stm, stm));

  auto it = callee_saves->GetList().begin();
  for (const auto &reg : reg_manager->CalleeSaves()->GetList()) {
    res_stm =
        new tree::SeqStm(res_stm, new tree::MoveStm(new tree::TempExp(reg),
                                                    new tree::TempExp(*it)));
    ++it;
  }
  return res_stm;
}
assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  body->Append(new assem::OperInstr("", new temp::TempList(),
                                    reg_manager->ReturnSink(), nullptr));
  return body;
}
assem::Proc *ProcEntryExit3(Frame *frame, assem::InstrList *body) {
  const auto name = frame->name_->Name();
  auto word_size = std::to_string(reg_manager->WordSize());
  std::string prolog =
      ".set " + name + "_framesize, " + std::to_string(-frame->offset_) + "\n";

  prolog += name +
            ":\n"
            "subq $" +
            std::to_string(-frame->offset_) + ", %rsp\n";
  const std::string epilog =
      "addq $" + std::to_string(-frame->offset_) + ",%rsp\n" + "retq\n";
  return new assem::Proc(prolog, body, epilog);
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
temp::TempList *X64RegManager::ReturnSink() {
  auto list = CalleeSaves();
  list->Append(StackPointer());
  list->Append(ReturnValue());
  return list;
}
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
  temp_map_->Enter(rax, new std::string("%rax"));
  temp_map_->Enter(rbx, new std::string("%rbx"));
  temp_map_->Enter(rcx, new std::string("%rcx"));
  temp_map_->Enter(rdx, new std::string("%rdx"));
  temp_map_->Enter(rsi, new std::string("%rsi"));
  temp_map_->Enter(rdi, new std::string("%rdi"));
  temp_map_->Enter(rbp, new std::string("%rbp"));
  temp_map_->Enter(rsp, new std::string("%rsp"));
  temp_map_->Enter(r8, new std::string("%r8"));
  temp_map_->Enter(r9, new std::string("%r9"));
  temp_map_->Enter(r10, new std::string("%r10"));
  temp_map_->Enter(r11, new std::string("%r11"));
  temp_map_->Enter(r12, new std::string("%r12"));
  temp_map_->Enter(r13, new std::string("%r13"));
  temp_map_->Enter(r14, new std::string("%r14"));
  temp_map_->Enter(r15, new std::string("%r15"));
  regs_.emplace_back(rax);
  regs_.emplace_back(rbx);
  regs_.emplace_back(rcx);
  regs_.emplace_back(rdx);
  regs_.emplace_back(rsi);
  regs_.emplace_back(rdi);
  regs_.emplace_back(r8);
  regs_.emplace_back(r9);
  regs_.emplace_back(r10);
  regs_.emplace_back(r11);
  regs_.emplace_back(r12);
  regs_.emplace_back(r13);
  regs_.emplace_back(r14);
  regs_.emplace_back(r15);
  regs_.emplace_back(rbp);
  regs_.emplace_back(rsp);
}
} // namespace frame