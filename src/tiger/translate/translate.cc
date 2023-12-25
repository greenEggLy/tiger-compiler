#include "tiger/translate/translate.h"
#include "tiger/frame/frame.h"
#include "tiger/runtime/gc/roots/roots.h"
extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape, bool in_heap) {
  auto access = level->frame_->AllocLocal(escape, in_heap, level->frame_);
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
  [[nodiscard]] tree::Stm *UnNx() override { return new tree::ExpStm(exp_); }
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
  [[nodiscard]] tree::Stm *UnNx() override { return cx_.stm_; }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override { return cx_; }
};

void ProgTr::Translate() {
  const auto label = temp::LabelFactory::NamedLabel("tigermain");
  const auto frame = new frame::X64Frame(label, {});
  main_level_.reset(new Level(frame, nullptr));
  FillBaseVEnv();
  FillBaseTEnv();
  const auto res = absyn_tree_.get()->Translate(
      venv_.get(), tenv_.get(), main_level_.get(), label, errormsg_.get());
  frags->PushBack(new frame::ProcFrag(res->exp_->UnNx(), main_level_->frame_));
}

Level *Level::NewLevel(tr::Level *parent, temp::Label *name,
                       std::list<bool> formals, std::list<bool> *in_heap) {
  // add the static link to the front of formals
  formals.push_front(true);
#ifdef GC_ENABLED
  assert(in_heap);
  in_heap->emplace_front(false);
#endif
  return new Level(new frame::X64Frame(name, formals, in_heap), parent);
}
} // namespace tr

namespace absyn {

#ifdef GC_ENABLED
inline bool IsPointerType(type::Ty *type) {
  auto ty = type->ActualTy();
  return typeid(*ty) == typeid(type::RecordTy) ||
         typeid(*ty) == typeid(type::ArrayTy);
}
#endif

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
  auto tree_exp = root_->Translate(venv, tenv, level, label, errormsg);
  return tree_exp;
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  type::Ty *type = nullptr;
  tr::Exp *exp = nullptr;
  auto entry = venv->Look(sym_);
  if (!entry || typeid(*entry) != typeid(env::VarEntry))
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());

  auto var_entry = (env::VarEntry *)(entry);
  tree::Exp *static_link = GetStaticLink(level, var_entry->access_->level_);
  static_link = var_entry->access_->access_->ToExp(static_link);
  type = var_entry->ty_->ActualTy();
  exp = new tr::ExExp(static_link);
  return new tr::ExpAndTy(exp, type);
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  auto var_exp = this->var_->Translate(venv, tenv, level, label, errormsg);
  auto type = var_exp->ty_->ActualTy();
  // if (typeid(*type) != typeid(type::RecordTy)) {
  //   errormsg->Error(pos_, "not a record type");
  //   return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  // }
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
  //  type = sub_exp->ty_->ActualTy();
  //  if (typeid(*type) != typeid(type::IntTy)) {
  //    errormsg->Error(pos_, "int type required");
  //    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  //  }

  auto offset = new tree::BinopExp(tree::MUL_OP, sub_exp->exp_->UnEx(),
                                   new tree::ConstExp(reg_manager->WordSize()));
  auto mem_exp = new tree::MemExp(
      new tree::BinopExp(tree::PLUS_OP, var_exp->exp_->UnEx(), offset));
  auto actual_type =
      dynamic_cast<type::ArrayTy *>(var_exp->ty_)->ty_->ActualTy();
  return new tr::ExpAndTy(new tr::ExExp(mem_exp), actual_type);
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
  auto func_entry = dynamic_cast<env::FunEntry *>(function);
  auto type = func_entry->result_ ? func_entry->result_->ActualTy()
                                  : type::VoidTy::Instance();
  auto args_iter = this->args_->GetList().begin();
  auto exp_list = new tree::ExpList();
  for (const auto &_ : func_entry->formals_->GetList()) {
    auto arg_exp =
        args_iter.operator*()->Translate(venv, tenv, level, label, errormsg);
    exp_list->Append(arg_exp->exp_->UnEx());
    args_iter++;
  }
  if (func_entry->level_->parent_) {
    if (func_entry->label_ == label) {
      // recursive call
      //      exp_list->Insert(new tree::TempExp(reg_manager->FramePointer()));
      exp_list->Insert(level->frame_->formals_->front()->ToExp(
          new tree::TempExp(reg_manager->FramePointer())));
    } else {
      exp_list->Insert(GetStaticLink(level, func_entry->level_->parent_));
    }
  }
  auto call_exp = new tree::CallExp(new tree::NameExp(func_), exp_list);
  return new tr::ExpAndTy(new tr::ExExp(call_exp), type);
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  auto left_exp = left_->Translate(venv, tenv, level, label, errormsg);
  auto right_exp = right_->Translate(venv, tenv, level, label, errormsg);
  if (oper_ == Oper::PLUS_OP || oper_ == Oper::MINUS_OP ||
      oper_ == Oper::TIMES_OP || oper_ == Oper::DIVIDE_OP) {
    tree::BinOp binop;
    switch (oper_) {
    case PLUS_OP:
      binop = tree::PLUS_OP;
      break;
    case MINUS_OP:
      binop = tree::MINUS_OP;
      break;
    case TIMES_OP:
      binop = tree::MUL_OP;
      break;
    case DIVIDE_OP:
      binop = tree::DIV_OP;
      break;
    default:
      assert(0);
    }
    return new tr::ExpAndTy{
        new tr::ExExp{new tree::BinopExp{binop, left_exp->exp_->UnEx(),
                                         right_exp->exp_->UnEx()}},
        type::IntTy::Instance()};
  }
  if (oper_ == Oper::GE_OP || oper_ == Oper::GT_OP || oper_ == Oper::LE_OP ||
      oper_ == Oper::LT_OP || oper_ == EQ_OP || oper_ == NEQ_OP) {
    tree::RelOp op;
    switch (oper_) {
    case GE_OP:
      op = tree::GE_OP;
      break;
    case GT_OP:
      op = tree::GT_OP;
      break;
    case LE_OP:
      op = tree::LE_OP;
      break;
    case LT_OP:
      op = tree::LT_OP;
      break;
    case EQ_OP:
      op = tree::EQ_OP;
      break;
    case NEQ_OP:
      op = tree::NE_OP;
      break;
    case ABSYN_OPER_COUNT:
      op = tree::REL_OPER_COUNT;
    default:
      break;
    }
    if ((oper_ == EQ_OP || oper_ == NEQ_OP) &&
        left_exp->ty_->IsSameType(type::StringTy::Instance())) {
      auto arg_list =
          new tree::ExpList{left_exp->exp_->UnEx(), right_exp->exp_->UnEx()};
      auto call_exp = new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("string_equal")),
          arg_list);
      auto stm = new tree::CjumpStm{tree::EQ_OP, tr::ExExp(call_exp).UnEx(),
                                    new tree::ConstExp(1), nullptr, nullptr};
      return new tr::ExpAndTy{new tr::CxExp{tr::PatchList{{&stm->true_label_}},
                                            tr::PatchList{{&stm->false_label_}},
                                            stm},
                              type::IntTy::Instance()};
    }
    auto stm = new tree::CjumpStm{op, left_exp->exp_->UnEx(),
                                  right_exp->exp_->UnEx(), nullptr, nullptr};
    return new tr::ExpAndTy{new tr::CxExp{tr::PatchList{{&stm->true_label_}},
                                          tr::PatchList{{&stm->false_label_}},
                                          stm},
                            type::IntTy::Instance()};
  }
  auto new_label = temp::LabelFactory::NewLabel();
  auto left_cx = left_exp->exp_->UnCx(errormsg);
  auto right_cx = right_exp->exp_->UnCx(errormsg);
  if (oper_ == Oper::AND_OP) {
    auto trues = tr::PatchList{right_cx.trues_};
    auto falses = tr::PatchList::JoinPatch(left_cx.falses_, right_cx.falses_);
    left_cx.trues_.DoPatch(new_label);
    auto stm = new tree::SeqStm(
        left_cx.stm_,
        new tree::SeqStm(new tree::LabelStm(new_label), right_cx.stm_));
    return new tr::ExpAndTy(new tr::CxExp(trues, falses, stm),
                            type::IntTy::Instance());
  }
  if (oper_ == Oper::OR_OP) {
    auto trues = tr::PatchList::JoinPatch(left_cx.trues_, right_cx.trues_);
    auto falses = tr::PatchList{right_cx.falses_};
    left_cx.falses_.DoPatch(new_label);
    auto stm = new tree::SeqStm(
        left_cx.stm_,
        new tree::SeqStm(new tree::LabelStm(new_label), right_cx.stm_));
    return new tr::ExpAndTy(new tr::CxExp(trues, falses, stm),
                            type::IntTy::Instance());
  }
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  auto fields = this->fields_->GetList();
  auto type = tenv->Look(this->typ_)->ActualTy();
  // alloc
  int size = (int)fields.size();
  auto args = new tree::ExpList();
  auto reg = temp::TempFactory::NewTemp();
  args->Insert(new tree::ConstExp(size * reg_manager->WordSize()));
#ifdef GC_ENABLED
  args->Append(new tree::NameExp(
      temp::LabelFactory::NamedLabel(typ_->Name() + "_DESCRIPTOR")));
  tree::Stm *stm = new tree::MoveStm(
      new tree::TempExp(reg),
      new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("alloc_record")),
          args));
#else
  tree::Stm *stm = new tree::MoveStm(
      new tree::TempExp(reg),
      new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("alloc_record")),
          args));

#endif

  // for each field
  int i = 0;
  auto field_iter = fields_->GetList().begin();
  while (field_iter != fields_->GetList().end()) {
    auto mem_exp = new tree::MemExp(
        new tree::BinopExp(tree::PLUS_OP, new tree::TempExp(reg),
                           new tree::ConstExp(i * reg_manager->WordSize())));
    auto field_exp = field_iter.operator*()->exp_->Translate(venv, tenv, level,
                                                             label, errormsg);
    stm = new tree::SeqStm(stm,
                           new tree::MoveStm(mem_exp, field_exp->exp_->UnEx()));
    field_iter++;
    i++;
  }
  auto exp = new tr::ExExp(new tree::EseqExp(stm, new tree::TempExp(reg)));
  return new tr::ExpAndTy(exp, type);
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
    if (!exp)
      exp = new tr::ExExp(curr_exp_ty->exp_->UnEx());
    else
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
  //  if (!var_exp || !src_exp || !var_exp->ty_->IsSameType(src_exp->ty_)) {
  //    errormsg->Error(pos_, "unmatched assign exp");
  //    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  //  }
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
  auto test_exp = this->test_->Translate(venv, tenv, level, label, errormsg);
  //  auto test_cx = test_exp->exp_->UnCx(errormsg);
  auto then_exp = this->then_->Translate(venv, tenv, level, label, errormsg);
  auto else_exp =
      elsee_ ? elsee_->Translate(venv, tenv, level, label, errormsg) : nullptr;
  auto then_label = temp::LabelFactory::NewLabel();
  auto else_label = temp::LabelFactory::NewLabel();
  auto ret_reg = temp::TempFactory::NewTemp();
  //  test_cx.trues_.DoPatch(then_label);
  auto test_stm =
      new tree::CjumpStm(tree::NE_OP, test_exp->exp_->UnEx(),
                         new tree::ConstExp(0), then_label, else_label);
  if (else_exp) {
    //    test_cx.falses_.DoPatch(else_label);
    auto done_label = temp::LabelFactory::NewLabel();
    auto res_stm = new tree::SeqStm(
        test_stm,
        new tree::SeqStm(
            new tree::LabelStm(then_label),
            new tree::SeqStm(
                new tree::MoveStm(new tree::TempExp(ret_reg),
                                  then_exp->exp_->UnEx()),
                new tree::SeqStm(
                    new tree::JumpStm(
                        new tree::NameExp(done_label),
                        new std::vector<temp::Label *>{done_label}),
                    new tree::SeqStm(
                        new tree::LabelStm(else_label),
                        new tree::SeqStm(
                            new tree::MoveStm(new tree::TempExp(ret_reg),
                                              else_exp->exp_->UnEx()),
                            new tree::LabelStm(done_label)))))));
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(res_stm, new tree::TempExp(ret_reg))),
        then_exp->ty_);
    //    return new tr::ExpAndTy(new tr::ExExp(exp), then_exp->ty_);
  } else {
    auto exp = new tree::SeqStm(
        test_stm,
        new tree::SeqStm(new tree::LabelStm(then_label),
                         new tree::SeqStm(then_exp->exp_->UnNx(),
                                          new tree::LabelStm(else_label))));
    return new tr::ExpAndTy(new tr::NxExp(exp), type::VoidTy::Instance());
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  auto test_label = temp::LabelFactory::NewLabel();
  auto body_label = temp::LabelFactory::NewLabel();
  auto end_label = temp::LabelFactory::NewLabel();
  auto test_exp = this->test_->Translate(venv, tenv, level, label, errormsg);
  auto body_exp =
      this->body_->Translate(venv, tenv, level, end_label, errormsg);
  auto test_stm =
      new tree::CjumpStm(tree::NE_OP, test_exp->exp_->UnEx(),
                         new tree::ConstExp(0), body_label, end_label);
  auto stm = new tree::SeqStm(
      new tree::JumpStm(new tree::NameExp(test_label),
                        new std::vector<temp::Label *>{test_label}),
      new tree::SeqStm(
          new tree::LabelStm(body_label),
          new tree::SeqStm(
              body_exp->exp_->UnNx(),
              new tree::SeqStm(
                  new tree::LabelStm(test_label),
                  new tree::SeqStm(test_stm, new tree::LabelStm(end_label))))));
  venv->EndScope();
  return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  const auto loop_var =
      new env::VarEntry(tr::Access::AllocLocal(level, escape_, false),
                        type::IntTy::Instance(), true);
  venv->Enter(var_, loop_var);
  const auto body_label = temp::LabelFactory::NewLabel();
  const auto done_label = temp::LabelFactory::NewLabel();
  const auto test_label = temp::LabelFactory::NewLabel();
  const auto lo_exp_ty = lo_->Translate(venv, tenv, level, label, errormsg);
  const auto hi_exp_ty = hi_->Translate(venv, tenv, level, label, errormsg);
  const auto body_exp_ty =
      body_->Translate(venv, tenv, level, done_label, errormsg);
  const auto limit_value_exp =
      new tr::ExExp(new tree::TempExp(temp::TempFactory::NewTemp()));
  const auto loop_var_exp = new tr::ExExp(new tree::TempExp(
      dynamic_cast<frame::InRegAccess *>(loop_var->access_->access_)->reg));
  const auto init_loop_var_stm =
      new tree::MoveStm(loop_var_exp->UnEx(), lo_exp_ty->exp_->UnEx());
  const auto init_limit_value_stm =
      new tree::MoveStm(limit_value_exp->UnEx(), hi_exp_ty->exp_->UnEx());
  const auto init_stm =
      new tree::SeqStm{init_loop_var_stm, init_limit_value_stm};
  const auto body_label_stm = new tree::SeqStm{new tree::LabelStm(body_label),
                                               body_exp_ty->exp_->UnNx()};
  const auto add_loop_var_stm =
      new tree::MoveStm(loop_var_exp->UnEx(),
                        new tree::BinopExp(tree::PLUS_OP, loop_var_exp->UnEx(),
                                           new tree::ConstExp(1)));
  const auto test_stm = new tree::SeqStm{
      new tree::CjumpStm{tree::LE_OP, loop_var_exp->UnEx(),
                         limit_value_exp->UnEx(), body_label, done_label},
      new tree::LabelStm(done_label)};
  const auto test_label_stm =
      new tree::SeqStm{new tree::LabelStm(test_label), test_stm};
  const auto stm = new tree::SeqStm{
      init_stm,
      new tree::SeqStm{
          new tree::JumpStm{new tree::NameExp{test_label},
                            new std::vector<temp::Label *>{test_label}},
          new tree::SeqStm{body_label_stm, new tree::SeqStm{add_loop_var_stm,
                                                            test_label_stm}}}};
  venv->EndScope();
  return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  auto exp = new tr::NxExp(
      new tree::JumpStm(new tree::NameExp(label), new std::vector{label}));
  return new tr::ExpAndTy(exp, type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();
  tree::Stm *stm = nullptr;
  for (const auto &item : this->decs_->GetList()) {
    const auto tmp_stm =
        item->Translate(venv, tenv, level, label, errormsg)->UnNx();
    stm = stm ? new tree::SeqStm(stm, tmp_stm) : tmp_stm;
  }
  const auto body_exp =
      this->body_->Translate(venv, tenv, level, label, errormsg);

  tree::Exp *exp;
  if (stm)
    exp = new tree::EseqExp(stm, body_exp->exp_->UnEx());
  else
    exp = body_exp->exp_->UnEx();
  tenv->EndScope();
  venv->EndScope();
  return new tr::ExpAndTy(new tr::ExExp(exp), body_exp->ty_->ActualTy());
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  const auto type = tenv->Look(this->typ_)->ActualTy();
  const auto init_exp =
      this->init_->Translate(venv, tenv, level, label, errormsg);
  const auto size_exp =
      this->size_->Translate(venv, tenv, level, label, errormsg);
  const auto exp_list = new tree::ExpList();
  //  exp_list->Append(new tree::TempExp(reg_manager->FramePointer()));
  exp_list->Append(size_exp->exp_->UnEx());
  exp_list->Append(init_exp->exp_->UnEx());
  const auto exp = new tree::CallExp(
      new tree::NameExp(temp::LabelFactory::NamedLabel("init_array")),
      exp_list);
  return new tr::ExpAndTy(new tr::ExExp(exp), type);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::NxExp(nullptr), type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  for (const auto &function : this->functions_->GetList()) {
    // create function entries
    const auto res_type = function->result_
                              ? tenv->Look(function->result_)->ActualTy()
                              : type::VoidTy::Instance();
    auto *ty_list = new type::TyList();
    auto formals = std::list<bool>();
    auto in_heap_list = std::list<bool>();
    for (const auto &param : function->params_->GetList()) {
      const auto para_type = tenv->Look(param->typ_)->ActualTy();
      ty_list->Append(para_type);
      formals.emplace_back(param->escape_);
#ifdef GC_ENABLED
      in_heap_list.emplace_back(IsPointerType(tenv->Look(param->typ_)));
#endif
    }
    auto *new_level =
        tr::Level::NewLevel(level, function->name_, formals, &in_heap_list);
    const auto func_entry =
        new env::FunEntry(new_level, function->name_, ty_list, res_type);
    venv->Enter(function->name_, func_entry);
  }

  for (const auto &function : this->functions_->GetList()) {
    const auto func_entry =
        dynamic_cast<env::FunEntry *>(venv->Look(function->name_));
    const auto ty_list = func_entry->formals_;
    const auto func_level = func_entry->level_;
    // create accesses
    venv->BeginScope();
    auto ty_list_iter = ty_list->GetList().begin();
    auto formal_list_iter = func_entry->level_->frame_->formals_->begin();
    auto param_list_iter = function->params_->GetList().begin();
    ++formal_list_iter;
    while (formal_list_iter != func_entry->level_->frame_->formals_->end()) {
      const auto access = new tr::Access(func_level, *formal_list_iter);
      const auto var_entry =
          new env::VarEntry(access, ty_list_iter.operator*());
      venv->Enter(param_list_iter.operator*()->name_, var_entry);
#ifdef GC_ENABLED
      access->access_->SetInHeap(
          IsPointerType(tenv->Look(param_list_iter.operator*()->typ_)));
#endif
      ++param_list_iter;
      ++ty_list_iter;
      ++formal_list_iter;
    }
    const auto body_exp = function->body_->Translate(
        venv, tenv, func_entry->level_, func_entry->label_, errormsg);
    const auto ret_stm = new tree::MoveStm(
        new tree::TempExp(reg_manager->ReturnValue()), body_exp->exp_->UnEx());
    venv->EndScope();
    //    auto body = ret_stm;
    const auto body = frame::ProcEntryExit1(func_level->frame_, ret_stm);
    frags->PushBack(new frame::ProcFrag(body, func_level->frame_));
  }
  return new tr::ExExp(new tree::ConstExp(0));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  const auto var_exp =
      this->init_->Translate(venv, tenv, level, label, errormsg);
#ifdef GC_ENABLED
  const auto ty = typ_ ? tenv->Look(typ_) : nullptr;
  const auto ty_ptr = ty ? IsPointerType(ty) : false;
  auto var_access = tr::Access::AllocLocal(
      level, this->escape_, IsPointerType(var_exp->ty_) || ty_ptr);
#else
  auto var_access = tr::Access::AllocLocal(level, this->escape_);
#endif
  auto tty = var_exp->ty_;
  if (typeid(*var_exp->ty_) == typeid(type::NilTy)) {
    tty = tenv->Look(this->typ_);
  }
  const auto var_entry = new env::VarEntry(var_access, tty);
  venv->Enter(this->var_, var_entry);
#ifdef GC_ENABLED
  var_access->access_->SetInHeap(IsPointerType(tty));
#endif
  const auto ret_val_stm =
      new tree::MoveStm(var_access->access_->ToExp(
                            new tree::TempExp(reg_manager->FramePointer())),
                        var_exp->exp_->UnEx());
  return new tr::NxExp(ret_val_stm);
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  for (const auto &type : this->types_->GetList()) {
    tenv->Enter(type->name_, new type::NameTy(type->name_, nullptr));
  }
  for (const auto &type : this->types_->GetList()) {
    const auto name_type =
        dynamic_cast<type::NameTy *>(tenv->Look(type->name_));
    name_type->ty_ = type->ty_->Translate(tenv, errormsg);
    tenv->Set(type->name_, name_type);
  }
#ifdef GC_ENABLED
  for (const auto &type : types_->GetList()) {
    const auto name_type =
        dynamic_cast<type::NameTy *>(tenv->Look(type->name_));
    if (auto actual_ty = name_type->ty_->ActualTy();
        typeid(*actual_ty) == typeid(type::RecordTy)) {
      const auto record_ty = dynamic_cast<type::RecordTy *>(name_type->ty_);

      auto des_label = name_type->sym_->Name() + "_DESCRIPTOR";
      std::string des_fields;
      for (const auto &field : record_ty->fields_->GetList()) {
        if (IsPointerType(field->ty_->ActualTy())) {
          des_fields += "1";
        } else {
          des_fields += "0";
        }
      }
      frags->PushBack(new frame::StringFrag(
          temp::LabelFactory::NamedLabel(des_label), des_fields));
    }
  }
#endif
  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  auto type = tenv->Look(this->name_);
  return new type::NameTy(this->name_, type);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  auto fields = new type::FieldList();
  for (const auto &item : this->record_->GetList()) {
    auto type = tenv->Look(item->typ_);
    auto ty_field = new type::Field(item->name_, type);
    fields->Append(ty_field);
  }
  return new type::RecordTy(fields);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  auto type_ = tenv->Look(this->array_);
  if (!type_) {
    errormsg->Error(pos_, "undefined declaration %s", array_->Name().data());
    return type::VoidTy::Instance();
  }
  return new type::ArrayTy(type_);
}

} // namespace absyn
