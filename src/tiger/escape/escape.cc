#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) { this->root_->Traverse(env, 1); }

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  auto *escEntry = env->Look(this->sym_);
  if (escEntry && escEntry->depth_ < depth) {
    *escEntry->escape_ = true;
  } else if (!escEntry) {
    auto new_entry = esc::EscapeEntry(depth, new bool(false));
    env->Set(this->sym_, &new_entry);
  }
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  this->var_->Traverse(env, depth);
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  this->var_->Traverse(env, depth);
  this->subscript_->Traverse(env, depth);
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->var_->Traverse(env, depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  //  auto *escEntry = env->Look(this->func_);
  //  if (escEntry && escEntry->depth_ < depth) {
  //    *escEntry->escape_ = true;
  //  }
  auto arg_list = this->args_->GetList();
  for (const auto &arg : arg_list) {
    arg->Traverse(env, depth);
  }
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->left_->Traverse(env, depth);
  this->right_->Traverse(env, depth);
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  //  auto *escEntry = env->Look(this->typ_);
  //  if (escEntry && escEntry->depth_ < depth) {
  //    *escEntry->escape_ = true;
  //  }
  auto fields = this->fields_->GetList();
  for (const auto &field : fields) {
    auto *esc = env->Look(field->name_);
    if (esc && esc->depth_ < depth) {
      *esc->escape_ = true;
    }
  }
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  auto seq_list = this->seq_->GetList();
  for (const auto &seq : seq_list) {
    seq->Traverse(env, depth);
  }
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->var_->Traverse(env, depth);
  this->exp_->Traverse(env, depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->test_->Traverse(env, depth);
  this->then_->Traverse(env, depth);
  if (elsee_)
    this->elsee_->Traverse(env, depth);
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->test_->Traverse(env, depth);
  this->body_->Traverse(env, depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->escape_ = false;
  env->Enter(this->var_, new esc::EscapeEntry(depth, &escape_));
  //  auto *esc = env->Look(this->var_);
  //  if (esc && esc->depth_ < depth) {
  //    *esc->escape_ = true;
  //  }
  this->lo_->Traverse(env, depth);
  this->hi_->Traverse(env, depth);
  this->body_->Traverse(env, depth);
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  auto dec_list = this->decs_->GetList();
  for (const auto &dec : dec_list) {
    dec->Traverse(env, depth);
  }
  this->body_->Traverse(env, depth);
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  //  auto *esc = env->Look(this->typ_);
  //  if (esc && esc->depth_ < depth) {
  //    *esc->escape_ = true;
  //  }
  this->init_->Traverse(env, depth);
  this->size_->Traverse(env, depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  auto func_list = this->functions_->GetList();
  for (const auto &func : func_list) {
    env->BeginScope();
    auto param_list = func->params_->GetList();
    for (const auto &param : param_list) {
      param->escape_ = false;
      env->Enter(param->name_,
                 new esc::EscapeEntry(depth + 1, &param->escape_));
    }
    func->body_->Traverse(env, depth + 1);
    env->EndScope();
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  this->escape_ = false;
  env->Enter(this->var_, new esc::EscapeEntry(depth, &escape_));
  init_->Traverse(env, depth);
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) { return; }

} // namespace absyn
