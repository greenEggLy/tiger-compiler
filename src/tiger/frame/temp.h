#ifndef TIGER_FRAME_TEMP_H_
#define TIGER_FRAME_TEMP_H_

#include "tiger/symbol/symbol.h"

#include <list>

namespace temp {

using Label = sym::Symbol;

class LabelFactory {
public:
  static Label *NewLabel();
  static Label *NamedLabel(std::string_view name);
  static std::string LabelString(Label *s);

private:
  int label_id_ = 0;
  static LabelFactory label_factory;
};

class Temp {
  friend class TempFactory;

public:
  [[nodiscard]] int Int() const;

private:
  int num_;
  explicit Temp(int num) : num_(num) {}
};

class TempFactory {
public:
  static Temp *NewTemp();

private:
  int temp_id_ = 100;
  static TempFactory temp_factory;
};

class Map {
public:
  void Enter(Temp *t, std::string *s);
  std::string *Look(Temp *t);
  void DumpMap(FILE *out);

  static Map *Empty();
  static Map *Name();
  static Map *LayerMap(Map *over, Map *under);

private:
  tab::Table<Temp, std::string> *tab_;
  Map *under_;

  Map() : tab_(new tab::Table<Temp, std::string>()), under_(nullptr) {}
  Map(tab::Table<Temp, std::string> *tab, Map *under)
      : tab_(tab), under_(under) {}
};

class TempList {
public:
  explicit TempList(Temp *t) : temp_list_({t}) {}
  TempList(std::initializer_list<Temp *> list) : temp_list_(list) {}
  TempList() = default;
  void Append(Temp *t) { temp_list_.push_back(t); }
  [[nodiscard]] Temp *NthTemp(int i) const;
  [[nodiscard]] const std::list<Temp *> &GetList() const { return temp_list_; }
  temp::TempList *Difference(const temp::TempList *diff_list) {
    for (const auto &tmp : diff_list->GetList()) {
      auto iter = temp_list_.begin();
      while (iter != temp_list_.end()) {
        if (*iter == tmp) {
          break;
        }
        ++iter;
      }
      if (iter == temp_list_.end())
        continue;
      temp_list_.erase(iter);
    }
    return this;
  }
  TempList *Union(const TempList *union_list) const {
    const auto new_list = new TempList();
    for (const auto &item : this->temp_list_) {
      new_list->Append(item);
    }
    for (const auto &item : union_list->GetList()) {
      if (!new_list->Contains(item))
        new_list->Append(item);
    }
    return new_list;
  }
  bool Contains(const Temp *temp) const {
    const auto result =
        std::any_of(temp_list_.begin(), temp_list_.end(),
                    [&](const temp::Temp *tmp) { return tmp == temp; });
    return result;
    for (const auto &tmp : temp_list_) {
      if (temp == tmp)
        return true;
    }
    return false;
  }
  bool Replace(const Temp *src, Temp *dst) {
    bool ret = false;
    for (auto &&temp : temp_list_) {
      if (temp == src) {
        temp = dst;
        ret = true;
      }
    }
    return ret;
  }

private:
  std::list<Temp *> temp_list_;
};

} // namespace temp

#endif