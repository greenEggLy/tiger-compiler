#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  this->root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  auto entry = venv->Look(this->sym_);
  if (!entry) {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    return type::IntTy::Instance();
  }
  if (typeid(*entry) == typeid(env::VarEntry)) {
    auto var_entry = dynamic_cast<env::VarEntry *>(entry);
    return var_entry->ty_->ActualTy();
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
  }
  return type::VoidTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  auto var_type_ = this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*var_type_) != typeid(type::RecordTy)) {
    errormsg->Error(this->var_->pos_, "not a record type");
    return type::VoidTy::Instance();
  }
  auto fields = dynamic_cast<type::RecordTy *>(var_type_)->fields_->GetList();
  for (const auto &field : fields) {
    if (field->name_ == this->sym_) {
      return field->ty_;
    }
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return type::VoidTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  auto sub_type_ =
      this->subscript_->SemAnalyze(venv, tenv, labelcount, errormsg)
          ->ActualTy();
  if (typeid(*sub_type_) != typeid(type::IntTy)) {
    errormsg->Error(this->subscript_->pos_, "array type required");
    return type::IntTy::Instance();
  }
  auto var_type_ =
      this->var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*var_type_) != typeid(type::ArrayTy)) {
    errormsg->Error(this->var_->pos_, "%s: array type required",
                    typeid(*var_type_).name());
    return type::IntTy::Instance();
  }
  return dynamic_cast<type::ArrayTy *>(var_type_)->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  auto function = venv->Look(this->func_);
  if (!function || typeid(*function) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::IntTy::Instance();
  }
  auto call_func = dynamic_cast<env::FunEntry *>(function);
  // check params
  auto args_iter = this->args_->GetList().begin();
  auto arg_size = args_->GetList().size();
  auto formal_size = call_func->formals_->GetList().size();
  if (arg_size > formal_size) {
    errormsg->Error(pos_, "too many params in function %s",
                    func_->Name().data());
    return type::IntTy::Instance();
  }
  //  if (arg_size < formal_size) {
  //    errormsg->Error(pos_, "too few arguments in function %s",
  //                    func_->Name().data());
  //    return type::IntTy::Instance();
  //  }
  for (const auto &formal_type : call_func->formals_->GetList()) {
    auto param_type =
        args_iter.operator*()->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!param_type->IsSameType(formal_type)) {
      errormsg->Error(args_iter.operator*()->pos_, "para type mismatch");
      return type::IntTy::Instance();
    }
    args_iter++;
  }
  return call_func->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  auto left_type_ =
      this->left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  auto right_type_ =
      this->right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (this->oper_ == absyn::PLUS_OP || this->oper_ == absyn::MINUS_OP ||
      this->oper_ == absyn::DIVIDE_OP || this->oper_ == absyn::TIMES_OP) {
    if (typeid(*left_type_) != typeid(type::IntTy)) {
      errormsg->Error(this->left_->pos_, "integer required");
    }
    if (typeid(*right_type_) != typeid(type::IntTy)) {
      errormsg->Error(this->right_->pos_, "integer required");
    }
    return type::IntTy::Instance();
  } else {
    if (!left_type_->IsSameType(right_type_)) {
      errormsg->Error(pos_, "same type required");
    }
    return type::IntTy::Instance();
  }
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  auto type = tenv->Look(this->typ_);
  if (!type) {
    errormsg->Error(pos_, "undefined type %s", this->typ_->Name().data());
    return type::IntTy::Instance();
  }
  return type->ActualTy();
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *result_;
  for (const auto &exp : this->seq_->GetList()) {
    result_ = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return result_;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  auto exp_type_ = this->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto var_type_ = this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*var_) == typeid(absyn::SimpleVar)) {
    auto entry = venv->Look(dynamic_cast<SimpleVar *>(var_)->sym_);
    if (entry->readonly_) {
      errormsg->Error(this->pos_, "loop variable can't be assigned");
      return type::VoidTy::Instance();
    }
  }
  if (!exp_type_->IsSameType(var_type_)) {
    errormsg->Error(pos_, "unmatched assign exp");
  }
  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  auto test_type_ = this->test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*test_type_) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required");
    return type::VoidTy::Instance();
  }
  venv->BeginScope();
  tenv->BeginScope();
  auto then_type = this->then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!elsee_) {
    if (typeid(*then_type) != typeid(type::VoidTy)) {
      errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
      return type::IntTy::Instance();
    }
  } else {
    auto else_type = this->elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!else_type->IsSameType(then_type)) {
      errormsg->Error(this->pos_, "then exp and else exp type mismatch");
      return type::VoidTy::Instance();
    }
    return else_type->ActualTy();
  }
  tenv->EndScope();
  venv->EndScope();
  return then_type->ActualTy();
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  auto test_type_ = this->test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*test_type_) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required");
    return type::VoidTy::Instance();
  }
  labelcount++;
  venv->BeginScope();
  tenv->BeginScope();
  auto while_type = this->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  tenv->EndScope();
  venv->EndScope();
  labelcount--;
  if (typeid(*while_type) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "while body must produce no value");
  }
  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  auto high_ = this->hi_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto low_ = this->lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*high_) != typeid(type::IntTy)) {
    errormsg->Error(this->hi_->pos_, "for exp's range type is not integer");
  }
  if (typeid(*low_) != typeid(type::IntTy)) {
    errormsg->Error(this->lo_->pos_, "for exp's range type is not integer");
  }
  labelcount++;
  venv->BeginScope();
  tenv->BeginScope();
  venv->Enter(this->var_, new env::VarEntry(type::IntTy::Instance(), true));
  body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  tenv->EndScope();
  venv->EndScope();
  labelcount--;
  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  if (labelcount == 0) {
    errormsg->Error(pos_, "break is not inside any loop");
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();
  for (const auto &dec : this->decs_->GetList()) {
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  type::Ty *result;
  if (!this->body_) {
    result = type::VoidTy::Instance();
  } else {
    result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  tenv->EndScope();
  venv->EndScope();
  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  auto name_type = tenv->Look(this->typ_);
  if (!name_type) {
    errormsg->Error(this->pos_, "no such type");
    return type::VoidTy::Instance();
  }
  auto inner_type = name_type->ActualTy();
  if (typeid(*inner_type) != typeid(type::ArrayTy)) {
    errormsg->Error(this->pos_, "not an array type");
    return type::VoidTy::Instance();
  }
  auto size_type = this->size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*size_type) != typeid(type::IntTy)) {
    errormsg->Error(this->size_->pos_, "not an int size");
    return type::VoidTy::Instance();
  }
  auto init_type = this->init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto array_type = dynamic_cast<type::ArrayTy *>(inner_type);
  if (!init_type->IsSameType(array_type->ty_)) {
    errormsg->Error(this->init_->pos_, "type mismatch");
    return type::VoidTy::Instance();
  }
  return inner_type;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  // first: register name
  for (const auto &function : functions_->GetList()) {
    type::Ty *res_type;
    if (function->result_ == nullptr) {
      res_type = type::VoidTy::Instance();
    } else {
      auto result = tenv->Look(function->result_);
      if (result) {
        res_type = result->ActualTy();
      } else {
        res_type = type::VoidTy::Instance();
      }
    }
    auto count = 0;
    for (const auto &item : functions_->GetList()) {
      if (item->name_ == function->name_)
        count++;
    }
    if (count > 1) {
      errormsg->Error(function->pos_, "two functions have the same name");
      return;
    }
    auto params_ = function->params_;
    auto formals = params_->MakeFormalTyList(tenv, errormsg);
    venv->Enter(function->name_, new env::FunEntry(formals, res_type));
  }
  // second: into body
  for (const auto &function : functions_->GetList()) {
    auto one_func = venv->Look(function->name_);
    if (!one_func)
      return;
    auto res_type = dynamic_cast<env::FunEntry *>(one_func)->result_;
    auto params = function->params_;
    auto formals = params->MakeFormalTyList(tenv, errormsg);
    venv->BeginScope();
    auto formal_it = formals->GetList().begin();
    for (const auto &param : params->GetList()) {
      venv->Enter(param->name_, new env::VarEntry(*formal_it));
      formal_it++;
    }
    type::Ty *body_type;
    body_type = function->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    // check result_type and type
    if (!body_type->IsSameType(res_type)) {
      errormsg->Error(pos_, "procedure returns value");
    }
    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  auto init_type_ = this->init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (this->typ_) {
    auto ty_ = tenv->Look(this->typ_);
    if (!ty_) {
      errormsg->Error(this->pos_, "undefined type %s", typ_->Name().data());
      return;
    }
    if (!ty_->IsSameType(init_type_)) {
      errormsg->Error(pos_, "type mismatch");
    }
  } else {
    if (typeid(*init_type_) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
    }
  }
  venv->Enter(this->var_, new env::VarEntry(init_type_));
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  // first: register type
  for (const auto &type : this->types_->GetList()) {
    int count = 0;
    for (const auto &item : types_->GetList()) {
      if (item->name_ == type->name_)
        count++;
    }
    if (count > 1) {
      errormsg->Error(type->ty_->pos_, "two types have the same name");
      return;
    }
    tenv->Enter(type->name_, new type::NameTy(type->name_, nullptr));
  }
  // second: fill type content
  for (const auto &type : this->types_->GetList()) {
    auto name_type = dynamic_cast<type::NameTy *>(tenv->Look(type->name_));
    name_type->ty_ = type->ty_->SemAnalyze(tenv, errormsg);
  }
  // check cycle
  for (const auto &type : this->types_->GetList()) {
    auto this_type = tenv->Look(type->name_);
    if (typeid(*this_type) == typeid(type::NameTy)) {
      this_type = dynamic_cast<type::NameTy *>(this_type)->ty_;
    }
    while (typeid(*this_type) == typeid(type::NameTy)) {
      if (dynamic_cast<type::NameTy *>(this_type)->sym_->Name() ==
          type->name_->Name()) {
        errormsg->Error(pos_, "illegal type cycle");
        return;
      }
      this_type = dynamic_cast<type::NameTy *>(this_type)->ty_;
    }
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  auto type = tenv->Look(this->name_);
  if (!type) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return type::VoidTy::Instance();
  }
  return new type::NameTy(this->name_, type);
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  auto fields = new type::FieldList();
  for (const auto &field : this->record_->GetList()) {
    auto type_ = tenv->Look(field->typ_);
    if (!type_) {
      errormsg->Error(field->pos_, "undefined type %s",
                      field->typ_->Name().data());
      delete fields;
      return type::VoidTy::Instance();
    }
    auto ty_field = new type::Field(field->name_, type_);
    fields->Append(ty_field);
  }
  return new type::RecordTy(fields);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  auto type_ = tenv->Look(this->array_);
  if (!type_) {
    errormsg->Error(pos_, "undefined declaration %s", array_->Name().data());
    return type::VoidTy::Instance();
  }
  return new type::ArrayTy(type_);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
