#include "tiger/liveness/liveness.h"

#include <map>

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  if (move_it == move_list_.end())
    return;
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap() {
  bool done = false;
  for (const auto &node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(node, new temp::TempList());
    out_->Enter(node, new temp::TempList());
  }
  while (!done) {
    done = true;
    auto it = flowgraph_->Nodes()->GetList().rbegin();
    while (it != flowgraph_->Nodes()->GetList().rend()) {
      const auto node = *it;
      const auto instr = node->NodeInfo();
      // auto node_in = MakeSet(in_->Look(node));
      std::set<temp::Temp *> node_out, node_in;
      const auto old_node_in = MakeSet(in_->Look(node));
      const auto old_node_out = MakeSet(out_->Look(node));
      auto node_def = MakeSet(instr->Def());
      auto node_use = MakeSet(instr->Use());
      auto diff_set = DiffSet(old_node_out, node_def);
      node_in = UnionSet(node_use, diff_set);
      for (const auto &succ_node : node->Succ()->GetList()) {
        auto succ_in = MakeSet(in_->Look(succ_node));
        node_out = UnionSet(node_out, succ_in);
      }
      if (node_in != old_node_in || node_out != old_node_out) {
        done = false;
        auto list_in = new temp::TempList();
        MakeList(node_in, list_in);
        auto list_out = new temp::TempList();
        MakeList(node_out, list_out);

        in_->Set(node, list_in);
        out_->Set(node, list_out);
      }
      ++it;
    }
  }
}

void LiveGraphFactory::InterfGraph() {

  for (const auto &node : flowgraph_->Nodes()->GetList()) {
    for (const auto &def : node->NodeInfo()->Def()->GetList()) {
      if (def && !temp_node_map_->Look(def)) {
        temp_node_map_->Enter(def, live_graph_.interf_graph->NewNode(def));
      }
    }
    for (const auto &use : node->NodeInfo()->Use()->GetList()) {
      if (use && !temp_node_map_->Look(use)) {
        temp_node_map_->Enter(use, live_graph_.interf_graph->NewNode(use));
      }
    }
  }

  for (const auto &reg : reg_manager->Registers()->GetList()) {
    if (!temp_node_map_->Look(reg)) {
      temp_node_map_->Enter(reg, live_graph_.interf_graph->NewNode(reg));
    }
    precolored_.insert(temp_node_map_->Look(reg));
  }

  for (const auto &reg : reg_manager->Registers()->GetList()) {
    const auto src_node = temp_node_map_->Look(reg);
    for (const auto &dst_reg : reg_manager->Registers()->GetList()) {
      if (dst_reg == reg)
        continue;
      const auto dst_node = temp_node_map_->Look(dst_reg);
      live_graph_.interf_graph->AddEdge(src_node, dst_node);
    }
  }
  for (const auto &node : flowgraph_->Nodes()->GetList()) {
    auto instr = node->NodeInfo();
    const auto def_list = instr->Def();
    const auto use_list = instr->Use();
    if (def_list->GetList().empty()) {
      continue;
    }
    for (const auto &def : def_list->GetList()) {
      if (!def || def == reg_manager->GetRegister(14) ||
          def == reg_manager->GetRegister(15)) {
        continue;
      }
      const auto def_node = temp_node_map_->Look(def);
      for (const auto &out : out_->Look(node)->GetList()) {
        if (out == reg_manager->GetRegister(14) ||
            out == reg_manager->GetRegister(15)) {
          continue;
        }
        const auto out_node = temp_node_map_->Look(out);
        if (typeid(*instr) != typeid(assem::MoveInstr)) {
          live_graph_.interf_graph->AddEdge(def_node, out_node);
          live_graph_.interf_graph->AddEdge(out_node, def_node);
        } else if (!use_list->Contains(out)) {
          live_graph_.interf_graph->AddEdge(def_node, out_node);
          live_graph_.interf_graph->AddEdge(out_node, def_node);
        }
      }
      if (typeid(*instr) == typeid(assem::MoveInstr)) {
        for (const auto &use : use_list->GetList()) {
          if (!use || use == reg_manager->GetRegister(14) ||
              use == reg_manager->GetRegister(15))
            continue;
          const auto use_node = temp_node_map_->Look(use);
          live_graph_.moves->Union(use_node, def_node);
        }
      }
    }
  }
  auto show_info = [](temp::Temp *t) {
    auto reg = reg_manager->temp_map_->Look(t);
    if (reg) {
      printf("%s", reg->c_str());
    } else {
      printf("%d", t->Int());
    }
  };
  // live_graph_.interf_graph->Show(stdout, live_graph_.interf_graph->Nodes(),
  // show_info);
  return;

  for (const auto &node : flowgraph_->Nodes()->GetList()) {
    auto instr = node->NodeInfo();
    if (typeid(*instr) != typeid(assem::MoveInstr)) {
      continue;
    }
    const auto src_list = instr->Use()->GetList();
    const auto dst_list = instr->Def()->GetList();
    if (src_list.empty() || dst_list.empty()) {
      continue;
    }
    const auto src = instr->Use()->NthTemp(0);
    const auto dst = instr->Def()->NthTemp(0);
    const INodePtr node_src = temp_node_map_->Look(src);
    const INodePtr node_dst = temp_node_map_->Look(dst);
    live_graph_.moves->Append(node_src, node_dst);
    // live_graph_.interf_graph->AddEdge(node_src, node_dst);
    // dst_cnt[node_dst]++;
    // dst_src[node_dst] = node_src;
  }
  // for (const auto &[dst, cnt] : dst_cnt) {
  //   if (cnt == 1) {
  //     live_graph_.moves->Delete(dst_src[dst], dst);
  //     replacable.insert(dst);
  //   }
  // }

  // add interference edges
  for (const auto &node : flowgraph_->Nodes()->GetList()) {
    auto instr = node->NodeInfo();
    if (instr->Def()->GetList().empty())
      continue;
    const auto live_out = out_->Look(node);
    temp::Temp *src = nullptr;
    if (typeid(*instr) == typeid(assem::MoveInstr) &&
        !instr->Use()->GetList().empty()) {
      src = instr->Use()->NthTemp(0);
    }
    const INodePtr node_dst = temp_node_map_->Look(instr->Def()->NthTemp(0));

    for (const auto &tmp : live_out->GetList()) {
      if (tmp != src) {
        const INodePtr node_out = temp_node_map_->Look(tmp);
        live_graph_.interf_graph->AddEdge(node_dst, node_out);
        live_graph_.interf_graph->AddEdge(node_out, node_dst);
      }
    }
  }
}
bool LiveGraphFactory::SetIncludes(const std::set<INodePtr> &set,
                                   const INodePtr &member) {
  if (set.find(member) != set.end())
    return true;
  return false;
}
std::set<temp::Temp *> LiveGraphFactory::MakeSet(const temp::TempList *list) {
  std::set<temp::Temp *> ret_set;
  if (!list) {
    return ret_set;
  }
  for (const auto &tmp : list->GetList()) {
    ret_set.insert(tmp);
  }
  return ret_set;
}
std::set<temp::Temp *>
LiveGraphFactory::UnionSet(const std::set<temp::Temp *> &left,
                           const std::set<temp::Temp *> &right) {
  std::set<temp::Temp *> ret;
  std::set_union(left.begin(), left.end(), right.begin(), right.end(),
                 std::inserter(ret, ret.begin()));
  return ret;
}

std::set<temp::Temp *>
LiveGraphFactory::DiffSet(const std::set<temp::Temp *> &left,
                          const std::set<temp::Temp *> &right) {
  std::set<temp::Temp *> ret;
  std::set_difference(left.begin(), left.end(), right.begin(), right.end(),
                      std::inserter(ret, ret.begin()));
  return ret;
}
void LiveGraphFactory::MakeList(const std::set<temp::Temp *> &set,
                                temp::TempList *&list) {
  for (const auto &tmp : set) {
    list->Append(tmp);
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}
temp::TempList *
LiveGraphFactory::LiveOut(graph::Node<assem::Instr> *instr) const {
  return out_->Look(instr);
}
INodePtr LiveGraphFactory::GetNode(temp::Temp *temp) const {
  return temp_node_map_->Look(temp);
}

} // namespace live
