#include "tiger/codegen/codegen.h"

#include "tiger/absyn/absyn.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

void CodeGen::Codegen() {
  fs_ = frame_->GetLabel() + "_framesize";
  auto instr_list = new assem::InstrList();
  for (const auto stm : traces_->GetStmList()->GetList()) {
    stm->Munch(*instr_list, fs_);
  }
  //
  frame::ProcEntryExit2(instr_list);
  assem_instr_->SetInstrList(instr_list);
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
void PushQ(assem::InstrList &instr_list, std::string_view fs, tree::Exp *exp) {
  auto exp_tmp = exp->Munch(instr_list, fs);
  instr_list.Append(new assem::MoveInstr(
      "subq $" + std::to_string(reg_manager->WordSize()) + " `d0",
      new temp::TempList(reg_manager->StackPointer()), nullptr));
  instr_list.Append(new assem::OperInstr(
      "movq `s0, (`d0)", new temp::TempList(reg_manager->StackPointer()),
      new temp::TempList(exp_tmp), nullptr));
}
temp::Temp *PopQ(assem::InstrList &instr_list, std::string_view fs) {
  auto ret_val = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr(
      "movq (`s0) `d0", new temp::TempList(ret_val),
      new temp::TempList(reg_manager->StackPointer()), nullptr));
  instr_list.Append(new assem::OperInstr(
      "addq $" + std::to_string(reg_manager->WordSize()) + " `d0",
      new temp::TempList(reg_manager->StackPointer()), nullptr, nullptr));
  return ret_val;
}

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  this->left_->Munch(instr_list, fs);
  this->right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::LabelInstr(
      temp::LabelFactory::LabelString(label_), this->label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::OperInstr("jmp `j0", nullptr, nullptr,
                                         new assem::Targets(this->jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *left_tmp;
  std::string instr;
  if (typeid(*left_) == typeid(tree::MemExp)) {
    left_tmp = dynamic_cast<tree::MemExp *>(left_)->Munch(instr_list, fs);
  } else {
    left_tmp = left_->Munch(instr_list, fs);
  }
  temp::Temp *right_tmp = right_->Munch(instr_list, fs);
  instr_list.Append(
      new assem::OperInstr("cmpq `s1, `s0", nullptr,
                           new temp::TempList{left_tmp, right_tmp}, nullptr));
  switch (this->op_) {
  case UGT_OP:
  case LE_OP:
    instr = "jle `j0";
    break;
  case EQ_OP:
    instr = "je `j0";
    break;
  case NE_OP:
    instr = "jne `j0";
    break;
  case UGE_OP:
  case LT_OP:
    instr = "jl `j0";
    break;
  case ULE_OP:
  case GT_OP:
    instr = "jg `j0";
    break;
  case ULT_OP:
  case GE_OP:
    instr = "jge `j0";
    break;
  case REL_OPER_COUNT:
    assert(0);
  }
  instr_list.Append(new assem::OperInstr(
      instr, nullptr, nullptr,
      new assem::Targets(new std::vector{true_label_, false_label_})));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  const auto src_tmp = src_->Munch(instr_list, fs);
  if (typeid(*dst_) == typeid(tree::MemExp)) {
    const auto dst_exp_tmp =
        dynamic_cast<tree::MemExp *>(dst_)->exp_->Munch(instr_list, fs);
    instr_list.Append(new assem::OperInstr(
        "movq `s0, (`s1)", nullptr, new temp::TempList{src_tmp, dst_exp_tmp},
        nullptr));
  } else {
    const auto dst_tmp = dst_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList(dst_tmp),
                                           new temp::TempList(src_tmp)));
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  this->exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  const auto ret_val = temp::TempFactory::NewTemp();
  temp::Temp *left_tmp = left_->Munch(instr_list, fs);
  temp::Temp *right_tmp = right_->Munch(instr_list, fs);
  switch (this->op_) {
  case PLUS_OP:
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList(ret_val),
                                           new temp::TempList(left_tmp)));
    instr_list.Append(
        new assem::OperInstr("addq `s0, `d0", new temp::TempList(ret_val),
                             new temp::TempList(right_tmp), nullptr));
    return ret_val;
  case MINUS_OP:
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList(ret_val),
                                           new temp::TempList(left_tmp)));
    instr_list.Append(
        new assem::OperInstr("subq `s0, `d0", new temp::TempList(ret_val),
                             new temp::TempList(right_tmp), nullptr));
    return ret_val;
  case MUL_OP:
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList(reg_manager->GetRegister(0)),
        new temp::TempList(left_tmp)));
    instr_list.Append(new assem::OperInstr(
        "imulq `s0",
        new temp::TempList{reg_manager->GetRegister(0),
                           reg_manager->GetRegister(3)},
        new temp::TempList{right_tmp, reg_manager->GetRegister(0)}, nullptr));
    instr_list.Append(
        new assem::MoveInstr("movq `s0, `d0", new temp::TempList(ret_val),
                             new temp::TempList(reg_manager->GetRegister(0))));
    return ret_val;
  case DIV_OP:
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList(reg_manager->GetRegister(0)),
        new temp::TempList(left_tmp)));
    instr_list.Append(new assem::OperInstr(
        "cqto",
        new temp::TempList{reg_manager->GetRegister(0),
                           reg_manager->GetRegister(3)},
        new temp::TempList{reg_manager->GetRegister(0)}, nullptr));
    instr_list.Append(new assem::OperInstr(
        "idivq `s0",
        new temp::TempList{reg_manager->GetRegister(0),
                           reg_manager->GetRegister(3)},
        new temp::TempList{right_tmp, reg_manager->GetRegister(0),
                           reg_manager->GetRegister(3)},
        nullptr));
    instr_list.Append(
        new assem::MoveInstr("movq `s0, `d0", new temp::TempList(ret_val),
                             new temp::TempList(reg_manager->GetRegister(0))));
    return ret_val;
  case AND_OP:
    break;
  case OR_OP:
    break;
  case LSHIFT_OP:
    break;
  case RSHIFT_OP:
    break;
  case ARSHIFT_OP:
    break;
  case XOR_OP:
    break;
  case BIN_OPER_COUNT:
    break;
  }
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto res = this->exp_->Munch(instr_list, fs);
  auto ret_val = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr("movq (`s0), `d0",
                                         new temp::TempList(ret_val),
                                         new temp::TempList{res}, nullptr));
  return ret_val;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (temp_ != reg_manager->FramePointer())
    return this->temp_;
  auto ret_val = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr(
      "leaq " + std::string(fs) + "(`s0), `d0", new temp::TempList(ret_val),
      new temp::TempList(reg_manager->StackPointer()), nullptr));
  return ret_val;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  this->stm_->Munch(instr_list, fs);
  return this->exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto ret_val = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr(
      "leaq " + temp::LabelFactory::LabelString(name_) + "(%rip), `d0",
      new temp::TempList(ret_val), nullptr, nullptr));
  return ret_val;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto ret_val = temp::TempFactory::NewTemp();
  instr_list.Append(
      new assem::OperInstr("movq $" + std::to_string(this->consti_) + ", `d0",
                           new temp::TempList(ret_val), nullptr, nullptr));
  return ret_val;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // auto ret_val = temp::TempFactory::NewTemp();
  this->args_->MunchArgs(instr_list, fs);
  auto function = dynamic_cast<tree::NameExp *>(fun_);
  instr_list.Append(new assem::OperInstr("callq " + function->name_->Name(),
                                         reg_manager->CallerSaves(), nullptr,
                                         nullptr));
  // instr_list.Append(
  //     new assem::MoveInstr("movq `s0, `d0", new temp::TempList(ret_val),
  //                          new temp::TempList(reg_manager->ReturnValue())));
  int size = args_->GetList().size() - reg_manager->ArgRegs()->GetList().size();
  if (size > 0) {
    instr_list.Append(new assem::OperInstr(
        "addq $" + std::to_string((size)*reg_manager->WordSize()) + ", `d0",
        new temp::TempList(reg_manager->StackPointer()), nullptr, nullptr));
  }
  return reg_manager->ReturnValue();
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  const auto ret_val = new temp::TempList();
  auto exp_iter = this->exp_list_.begin();
  const auto arg_regs = reg_manager->ArgRegs();
  for (int i = 0; i < 6 && exp_iter != exp_list_.end(); i++, ++exp_iter) {
    const auto reg = arg_regs->NthTemp(i);
    const auto exp_tmp = exp_iter.operator*()->Munch(instr_list, fs);
    ret_val->Append(reg);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList(reg), new temp::TempList(exp_tmp)));
  }
  auto r_exp_iter = this->exp_list_.end();
  --exp_iter;
  --r_exp_iter;
  while (r_exp_iter != exp_iter) {
    const auto exp = r_exp_iter.operator*();
    instr_list.Append(new assem::MoveInstr(
        "subq $" + std::to_string(reg_manager->WordSize()) + ", `d0",
        new temp::TempList(reg_manager->StackPointer()), nullptr));
    instr_list.Append(
        new assem::OperInstr("movq `s0, (`s1)", nullptr,
                             new temp::TempList{exp->Munch(instr_list, fs),
                                                reg_manager->StackPointer()},
                             nullptr));
    --r_exp_iter;
  }
  return ret_val;
}

} // namespace tree
