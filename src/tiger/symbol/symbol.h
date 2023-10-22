#ifndef TIGER_SYMBOL_SYMBOL_H_
#define TIGER_SYMBOL_SYMBOL_H_

#include <string>

#include "tiger/util/table.h"

/**
 * Forward Declarations
 */
namespace env {
class EnvEntry;
} // namespace env
namespace type {
class Ty;
} // namespace type

namespace sym {
class Symbol {
  template <typename ValueType> friend class Table;

public:
  static Symbol *UniqueSymbol(std::string_view);
  [[nodiscard]] std::string Name() const { return name_; }

private:
  Symbol(std::string name, Symbol *next)
      : name_(std::move(name)), next_(next) {}

  std::string name_;
  Symbol *next_;
};

template <typename ValueType>
class Table : public tab::Table<Symbol, ValueType> {
public:
  Table() : tab::Table<Symbol, ValueType>() {}
  void BeginScope();
  void EndScope();
  void BeginLoop();
  void EndLoop();
  bool IsInLoop();

private:
  Symbol marksym_ = {"<mark>", nullptr};
  int32_t loop_ = 0;
};
template <typename ValueType> bool Table<ValueType>::IsInLoop() {
  return loop_ > 0;
}
template <typename ValueType> void Table<ValueType>::EndLoop() { loop_ -= 1; }
template <typename ValueType> void Table<ValueType>::BeginLoop() { loop_ += 1; }

template <typename ValueType> void Table<ValueType>::BeginScope() {
  this->Enter(&marksym_, nullptr);
}

template <typename ValueType> void Table<ValueType>::EndScope() {
  Symbol *s;
  do
    s = this->Pop();
  while (s != &marksym_);
}

} // namespace sym

#endif // TIGER_SYMBOL_SYMBOL_H_
