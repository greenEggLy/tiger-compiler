#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  auto access = level->frame_->AllocLocal(escape);
  return new Access(level, access);
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override { return exp_; }
  [[nodiscard]] tree::Stm *UnNx() override { new tree::ExpStm(exp_); }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    auto stm = new tree::CjumpStm(tree::NE_OP, exp_, new tree::ConstExp(0),
                                  nullptr, nullptr);
    auto trues = PatchList();
    auto falses = PatchList();
    return {trues, falses, stm};
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() override { return stm_; }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    errormsg->Error(0, "cannot translate NxExp to CxExp!");
    auto true_labels = PatchList(), false_labels = PatchList();
    return {true_labels, false_labels, stm_};
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    auto reg = temp::TempFactory::NewTemp();
    auto true_label = temp::LabelFactory::NewLabel(),
         false_label = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(true_label);
    cx_.falses_.DoPatch(false_label);
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(reg), new tree::ConstExp(1)),
        new tree::EseqExp(
            cx_.stm_, new tree::EseqExp(
                          new tree::LabelStm(false_label),
                          new tree::EseqExp(
                              new tree::MoveStm(new tree::TempExp(reg),
                                                new tree::ConstExp(0)),
                              new tree::EseqExp(new tree::LabelStm(true_label),
                                                new tree::TempExp(reg))))));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    auto label = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(label);
    cx_.falses_.DoPatch(label);
    return new tree::SeqStm(cx_.stm_, new tree::LabelStm(label));
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override { return cx_; }
};

void ProgTr::Translate() { /* TODO: Put your lab5 code here */
}

Level *Level::NewLevel(tr::Level *parent, temp::Label *name,
                       std::list<bool> formals) {
  // add the static link to the front of formals
  formals.push_front(true);
  return new Level(new frame::X64Frame(name, formals), parent);
}
} // namespace tr

namespace absyn {

tree::Exp *GetStaticLink(tr::Level *curr, tr::Level *target) {
  tree::Exp *static_link = new tree::TempExp(reg_manager->FramePointer());
  while (curr != target) {
    static_link = curr->frame_->formals_->front()->ToExp(static_link);
    curr = curr->parent_;
  }
  return static_link;
}

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  type::IntTy *type = nullptr;
  tr::Exp *exp = nullptr;
  auto entry = venv->Look(sym_);
  if (!entry || typeid(*entry) != typeid(env::VarEntry))
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());

  auto var_entry = (env::VarEntry *)(entry);
  tree::Exp *static_link = GetStaticLink(level, var_entry->access_->level_);
  static_link = var_entry->access_->access_->ToExp(static_link);
  type = dynamic_cast<type::IntTy *>(var_entry->ty_->ActualTy());
  exp = new tr::ExExp(static_link);
  return new tr::ExpAndTy(exp, type);
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  auto var_exp = this->var_->Translate(venv, tenv, level, label, errormsg);
  auto type = var_exp->ty_->ActualTy();
  if (typeid(*type) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  }
  auto fields =
      dynamic_cast<type::RecordTy *>(var_exp->ty_)->fields_->GetList();
  int cnt = 0;
  for (const auto &field : fields) {
    if (field->name_ == this->sym_) {
      auto tree =
          new tree::BinopExp(tree::PLUS_OP, var_exp->exp_->UnEx(),
                             new tree::ConstExp(cnt * reg_manager->WordSize()));
      auto exp = new tr::ExExp(new tree::MemExp(tree));
      return new tr::ExpAndTy(exp, type);
    }
    cnt++;
  }
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  auto var_exp = this->var_->Translate(venv, tenv, level, label, errormsg);
  auto type = var_exp->ty_->ActualTy();
  if (typeid(*type) != typeid(type::ArrayTy)) {
    errormsg->Error(subscript_->pos_, "%s: array type required",
                    typeid(*type).name());
    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  }
  auto sub_exp =
      this->subscript_->Translate(venv, tenv, level, label, errormsg);
  type = sub_exp->ty_->ActualTy();
  if (typeid(*type) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "int type required");
    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  }

  auto offset = new tree::BinopExp(tree::MUL_OP, sub_exp->exp_->UnEx(),
                                   new tree::ConstExp(reg_manager->WordSize()));
  auto mem_exp = new tree::MemExp(
      new tree::BinopExp(tree::PLUS_OP, var_exp->exp_->UnEx(), offset));
  return new tr::ExpAndTy(new tr::ExExp(mem_exp), var_exp->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return this->var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)),
                          type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  auto str_label = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(str_label, str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(str_label)),
                          type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  auto function = venv->Look(this->func_);
  if (!function || typeid(*function) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::NilTy::Instance());
  }
  auto func_entry = dynamic_cast<env::FunEntry *>(function);
  auto type = func_entry->result_ ? func_entry->result_->ActualTy()
                                  : type::VoidTy::Instance();
  auto args_iter = this->args_->GetList().begin();
  auto arg_size = args_->GetList().size();
  auto formal_size = func_entry->formals_->GetList().size();
  auto exp_list = new tree::ExpList();
  if (arg_size > formal_size) {
    errormsg->Error(pos_, "too many params in function %s",
                    func_->Name().data());
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::NameExp(func_entry->label_)), type);
  }
  for (const auto &formal : func_entry->formals_->GetList()) {
    auto arg_exp =
        args_iter.operator*()->Translate(venv, tenv, level, label, errormsg);
    if (!arg_exp->ty_->ActualTy()->IsSameType(formal)) {
      errormsg->Error(args_iter.operator*()->pos_, "para type mismatch");
      return new tr::ExpAndTy(
          new tr::ExExp(new tree::NameExp(func_entry->label_)), type);
    }
    exp_list->Append(arg_exp->exp_->UnEx());
    args_iter++;
  }
  if (func_entry->level_->parent_) {
    exp_list->Insert(GetStaticLink(level, func_entry->level_->parent_));
    auto call_exp =
        new tree::CallExp(new tree::NameExp(func_entry->label_), exp_list);
    auto exp = new tr::ExExp(call_exp);
    return new tr::ExpAndTy(exp, type);
  } else {
    auto call_exp = new tree::CallExp(
        new tree::NameExp(temp::LabelFactory::NamedLabel(this->func_->Name())),
        exp_list);
    auto exp = new tr::ExExp(call_exp);
    return new tr::ExpAndTy(exp, type);
  }
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::Exp *exp = nullptr;

  auto left_exp = this->left_->Translate(venv, tenv, level, label, errormsg);
  auto right_exp = this->right_->Translate(venv, tenv, level, label, errormsg);
  switch (this->oper_) {
  case Oper::PLUS_OP:
  case Oper::MINUS_OP:
  case Oper::TIMES_OP:
  case Oper::DIVIDE_OP:
    if (!left_exp->ty_->IsSameType(type::IntTy::Instance()) ||
        !right_exp->ty_->IsSameType(type::IntTy::Instance())) {
      errormsg->Error(pos_, "integer required");
      exp = new tr::ExExp(new tree::BinopExp(
          tree::PLUS_OP, left_exp->exp_->UnEx(), right_exp->exp_->UnEx()));
      return new tr::ExpAndTy(exp, type::IntTy::Instance());
    } else {
      if (oper_ == Oper::PLUS_OP)
        exp = new tr::ExExp(new tree::BinopExp(
            tree::PLUS_OP, left_exp->exp_->UnEx(), right_exp->exp_->UnEx()));
      if (oper_ == Oper::MINUS_OP)
        exp = new tr::ExExp(new tree::BinopExp(
            tree::MINUS_OP, left_exp->exp_->UnEx(), right_exp->exp_->UnEx()));
      if (oper_ == Oper::TIMES_OP)
        exp = new tr::ExExp(new tree::BinopExp(
            tree::MUL_OP, left_exp->exp_->UnEx(), right_exp->exp_->UnEx()));
      if (oper_ == Oper::DIVIDE_OP)
        exp = new tr::ExExp(new tree::BinopExp(
            tree::DIV_OP, left_exp->exp_->UnEx(), right_exp->exp_->UnEx()));
      return new tr::ExpAndTy(exp, type::IntTy::Instance());
    }
  case AND_OP: {
    auto left_cx = left_exp->exp_->UnCx(errormsg);
    auto right_cx = right_exp->exp_->UnCx(errormsg);
    auto true_label = temp::LabelFactory::NewLabel();
    auto falses = tr::PatchList::JoinPatch(left_cx.falses_, right_cx.falses_);
    auto trues = tr::PatchList(right_cx.trues_);
    auto seq = new tree::SeqStm(
        left_cx.stm_,
        new tree::SeqStm(new tree::LabelStm(true_label), right_cx.stm_));
    exp = new tr::CxExp(trues, falses, seq);
    return new tr::ExpAndTy(exp, type::IntTy::Instance());
  }
  case OR_OP: {
    auto left_cx = left_exp->exp_->UnCx(errormsg);
    auto right_cx = right_exp->exp_->UnCx(errormsg);
    auto false_label = temp::LabelFactory::NewLabel();
    auto trues = tr::PatchList::JoinPatch(left_cx.trues_, right_cx.trues_);
    auto falses = tr::PatchList(right_cx.falses_);
    left_cx.falses_.DoPatch(false_label);
    auto seq = new tree::SeqStm(
        left_cx.stm_,
        new tree::SeqStm(new tree::LabelStm(false_label), right_cx.stm_));
    exp = new tr::CxExp(trues, falses, seq);
    return new tr::ExpAndTy(exp, type::IntTy::Instance());
  }
  case EQ_OP:
  case NEQ_OP:
  case LT_OP:
  case LE_OP:
  case GT_OP:
  case GE_OP: {
    auto trues = tr::PatchList();
    auto falses = tr::PatchList();
    tree::CjumpStm *stm = nullptr;
    if (!left_exp->ty_->IsSameType(right_exp->ty_)) {
      errormsg->Error(pos_, "integer required");
      exp = new tr::CxExp(
          trues, falses,
          new tree::CjumpStm(tree::EQ_OP, left_exp->exp_->UnEx(),
                             right_exp->exp_->UnEx(), nullptr, nullptr));
      return new tr::ExpAndTy(exp, type::IntTy::Instance());
    } else {
      if (oper_ == EQ_OP) {
        if (typeid(*left_exp->ty_) == typeid(type::StringTy)) {
          auto expList = new tree::ExpList();
          expList->Append(new tree::TempExp(reg_manager->FramePointer()));
          expList->Append(left_exp->exp_->UnEx());
          expList->Append(right_exp->exp_->UnEx());
          stm = new tree::CjumpStm(
              tree::EQ_OP,
              new tree::CallExp(
                  new tree::NameExp(
                      temp::LabelFactory::NamedLabel("string_equal")),
                  expList),
              new tree::ConstExp(1), nullptr, nullptr);
        } else {
          stm = new tree::CjumpStm(tree::EQ_OP, left_exp->exp_->UnEx(),
                                   right_exp->exp_->UnEx(), nullptr, nullptr);
        }
      }
      if (oper_ == NEQ_OP) {
        stm = new tree::CjumpStm(tree::EQ_OP, left_exp->exp_->UnEx(),
                                 right_exp->exp_->UnEx(), nullptr, nullptr);
      }
      if (oper_ == LT_OP) {
        stm = new tree::CjumpStm(tree::EQ_OP, left_exp->exp_->UnEx(),
                                 right_exp->exp_->UnEx(), nullptr, nullptr);
      }
      if (oper_ == LE_OP) {
        stm = new tree::CjumpStm(tree::EQ_OP, left_exp->exp_->UnEx(),
                                 right_exp->exp_->UnEx(), nullptr, nullptr);
      }
      if (oper_ == GT_OP) {
        stm = new tree::CjumpStm(tree::EQ_OP, left_exp->exp_->UnEx(),
                                 right_exp->exp_->UnEx(), nullptr, nullptr);
      }
      if (oper_ == GE_OP) {
        stm = new tree::CjumpStm(tree::EQ_OP, left_exp->exp_->UnEx(),
                                 right_exp->exp_->UnEx(), nullptr, nullptr);
      }
      trues.DoPatch(stm->true_label_);
      falses.DoPatch(stm->false_label_);
      exp = new tr::CxExp(trues, falses, stm);
      return new tr::ExpAndTy(exp, type::IntTy::Instance());
    }
  }
  case ABSYN_OPER_COUNT:
    return nullptr;
  }
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  auto fields = this->fields_->GetList();
  int size = (int)fields.size();
  auto r_field_iter = fields.rbegin();
  auto type = dynamic_cast<type::RecordTy *>(tenv->Look(this->typ_));
  auto act_type = type->ActualTy();
  if (!type || typeid(*act_type) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return new tr::ExpAndTy(nullptr, type ? type::IntTy::Instance() : act_type);
  }
  auto type_fields = type->fields_->GetList();
  int type_size = (int)type_fields.size();
  auto r_type_field_iter = type_fields.rbegin();
  if (size > type_size) {
    errormsg->Error(pos_, "too many fields");
  }
  type_size = size = size < type_size ? size : type_size;
  // alloc
  auto args = new tree::ExpList();
  auto reg = temp::TempFactory::NewTemp();
  args->Insert(new tree::ConstExp(size * reg_manager->WordSize()));
  args->Insert(new tree::ConstExp(0));
  auto stm = new tree::MoveStm(
      new tree::TempExp(reg),
      new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("alloc_record")),
          args));

  // for each field
  tree::Stm *curr_stm = nullptr;
  while (size--) {
    auto field_exp = r_field_iter.operator*()->exp_->Translate(
        venv, tenv, level, label, errormsg);
    if (field_exp->ty_->IsSameType(
            r_type_field_iter.operator*()->ty_->ActualTy())) {
    }
    auto new_stm = new tree::MoveStm(
        new tree::TempExp(temp::TempFactory::NewTemp()),
        new tree::MemExp(new tree::BinopExp(
            tree::BinOp::PLUS_OP, new tree::TempExp(reg),
            new tree::ConstExp(size * reg_manager->WordSize()))));
    if (!curr_stm) {
      curr_stm = new_stm;
    } else {
      curr_stm = new tree::SeqStm(new_stm, curr_stm);
    }
    r_field_iter++;
    r_type_field_iter++;
  }
  auto exp = new tr::ExExp(new tree::EseqExp(new tree::SeqStm(stm, curr_stm),
                                             new tree::TempExp(reg)));
  return new tr::ExpAndTy(exp, act_type);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  tr::ExExp *exp = nullptr;
  tr::ExpAndTy *curr_exp_ty = nullptr;
  auto r_exp_iter = this->seq_->GetList().begin();
  while (r_exp_iter != seq_->GetList().end()) {
    curr_exp_ty =
        r_exp_iter.operator*()->Translate(venv, tenv, level, label, errormsg);
    exp = new tr::ExExp(
        new tree::EseqExp(exp->UnNx(), curr_exp_ty->exp_->UnEx()));
    r_exp_iter++;
  }
  return new tr::ExpAndTy(exp, curr_exp_ty->ty_->ActualTy());
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  auto var_exp = this->var_->Translate(venv, tenv, level, label, errormsg);
  auto src_exp = this->exp_->Translate(venv, tenv, level, label, errormsg);
  if (!var_exp || !src_exp || !var_exp->ty_->IsSameType(src_exp->ty_)) {
    errormsg->Error(pos_, "unmatched assign exp");
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }
  if (typeid(*var_) == typeid(SimpleVar)) {
    auto entry = venv->Look(dynamic_cast<SimpleVar *>(var_)->sym_);
    if (entry->readonly_) {
      errormsg->Error(this->pos_, "loop variable can't be assigned");
      return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
    }
  }
  return new tr::ExpAndTy(new tr::NxExp(new tree::MoveStm(
                              var_exp->exp_->UnEx(), src_exp->exp_->UnEx())),
                          type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto test_exp = this->test_->Translate(venv, tenv, level, label, errormsg);
  auto test_cx = test_exp->exp_->UnCx(errormsg);
  auto then_exp = this->then_->Translate(venv, tenv, level, label, errormsg);
  auto else_exp =
      elsee_ ? elsee_->Translate(venv, tenv, level, label, errormsg) : nullptr;
  auto then_label = temp::LabelFactory::NewLabel();
  auto else_label = temp::LabelFactory::NewLabel();
  auto ret_reg = temp::TempFactory::NewTemp();
  test_cx.trues_.DoPatch(then_label);
  if (else_label) {
    test_cx.falses_.DoPatch(else_label);
    auto exp = new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(ret_reg), then_exp->exp_->UnEx()),
        new tree::EseqExp(
            test_cx.stm_,
            new tree::EseqExp(
                new tree::LabelStm(else_label),
                new tree::EseqExp(
                    new tree::MoveStm(new tree::TempExp(ret_reg),
                                      else_exp->exp_->UnEx()),
                    new tree::EseqExp(new tree::LabelStm(then_label),
                                      new tree::TempExp(ret_reg))))));

    return new tr::ExpAndTy(new tr::ExExp(exp), then_exp->ty_);
  } else {
    auto exp = new tree::SeqStm(
        test_cx.stm_,
        new tree::SeqStm(new tree::LabelStm(then_label),
                         new tree::SeqStm(then_exp->exp_->UnNx(),
                                          new tree::LabelStm(else_label))));
    return new tr::ExpAndTy(new tr::NxExp(exp), type::VoidTy::Instance());
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto test_exp = this->test_->Translate(venv, tenv, level, label, errormsg);
  auto test_cx = test_exp->exp_->UnCx(errormsg);
  auto body_exp = this->body_->Translate(venv, tenv, level, label, errormsg);
  auto test_label = temp::LabelFactory::NewLabel();
  auto body_label = temp::LabelFactory::NewLabel();
  auto end_label = temp::LabelFactory::NewLabel();
  test_cx.trues_.DoPatch(body_label);
  test_cx.falses_.DoPatch(end_label);
  //  auto exp = new tree::SeqStm(test_cx.stm_,
  //                              new tree::SeqStm(new
  //                              tree::LabelStm(body_label)))
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

} // namespace absyn
