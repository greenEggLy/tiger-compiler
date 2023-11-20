#include "straightline/slp.h"

#include <iostream>

namespace A {

int A::CompoundStm::MaxArgs() const {
  int max1 = stm1->MaxArgs();
  int max2 = stm2->MaxArgs();
  if (max1 > max2) {
    return max1;
  }
  return max2;
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  auto t1 = this->stm1->Interp(t);
  return this->stm2->Interp(t1);
}

int A::AssignStm::MaxArgs() const { return this->exp->MaxArgs(); }

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  auto key = this->id;
  auto val = this->exp->Interp(t)->i;
  return t->Update(key, val);
}

int A::PrintStm::MaxArgs() const { return this->exps->MaxArgs(); }

Table *A::PrintStm::Interp(Table *t) const {
  auto times = this->exps->NumExps();
  auto pList = this->exps;
  for (int i = 0; i < times; ++i) {
    printf("%d ", pList->GetExp()->Interp(t)->i);
    pList = pList->GetNext();
  }
  printf("\n");
  return t;
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
int Exp::MaxArgs() const { return 1; }
int EseqExp::MaxArgs() const {
  int max1 = this->stm->MaxArgs();
  int max2 = this->exp->MaxArgs();
  return max1 > max2 ? max1 : max2;
}
IntAndTable *EseqExp::Interp(Table *table) const {
  auto t1 = this->stm->Interp(table);
  return this->exp->Interp(t1);
}
int PairExpList::MaxArgs() const { return 1 + this->tail->MaxArgs(); }
int PairExpList::NumExps() const { return this->tail->NumExps() + 1; }
IntAndTable *PairExpList::Interp(Table *table) const {
  return this->tail->Interp(table);
}
int LastExpList::MaxArgs() const { return this->exp->MaxArgs(); }
int LastExpList::NumExps() const { return 1; }
IntAndTable *LastExpList::Interp(Table *table) const {
  return this->exp->Interp(table);
}
IntAndTable *IdExp::Interp(Table *table) const {
  int val = table->Lookup(this->id);
  return new IntAndTable(val, table);
}
IntAndTable *NumExp::Interp(Table *table) const {
  return new IntAndTable(this->num, table);
}
IntAndTable *OpExp::Interp(Table *table) const {
  auto val1 = this->left->Interp(table)->i;
  auto val2 = this->right->Interp(table)->i;
  switch (this->oper) {
  case A::BinOp::PLUS:
    return new IntAndTable(val1 + val2, table);
  case A::BinOp::MINUS:
    return new IntAndTable(val1 - val2, table);
  case A::BinOp::TIMES:
    return new IntAndTable(val1 * val2, table);
  case A::BinOp::DIV:
    return new IntAndTable(val1 / val2, table);
  }
}
} // namespace A
