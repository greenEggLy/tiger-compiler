#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
RegAllocator::RegAllocator(frame::Frame *frame,
                           std::unique_ptr<cg::AssemInstr> assem_instr)
    : K(reg_manager->Registers()->GetList().size() - 2), frame_(frame),
      assem_instr_(std::move(assem_instr)) {}
void RegAllocator::RegAlloc() {
  bool done = false;
  while (!done) {
    ClearAndInit();
    Build();
    MakeWorkList();
    do {
      if (!simplify_worklist_.empty()) {
        Simplify();
      } else if (!worklist_moves_->GetList().empty()) {
        Coalesce();
      } else if (!freeze_worklist_.empty()) {
        Freeze();
      } else if (!spill_worklist_.empty()) {
        SelectSpill();
      }
    } while (!(simplify_worklist_.empty() &&
               worklist_moves_->GetList().empty() && freeze_worklist_.empty() &&
               spill_worklist_.empty()));
    AssignColors();
    if (!spilled_nodes_.empty()) {
      ReWriteProgram();
    } else {
      done = true;
      SimplifyProgram();
    }
  }
}
std::unique_ptr<ra::Result> RegAllocator::TransferResult() {
  // transfer std::map to temp::Map
  temp::Map *coloring_ = temp::Map::Empty();
  for (const auto &[node, color] : color_) {
    coloring_->Enter(node->NodeInfo(), reg_manager->temp_map_->Look(
                                           reg_manager->GetRegister(color)));
  }
  return std::make_unique<Result>(coloring_, assem_instr_->GetInstrList());
}
void RegAllocator::Build() {
  for (const auto &node :
       live_graph_->GetLiveGraph().interf_graph->Nodes()->GetList()) {
    const auto adjs = node->Adj();
    for (const auto &adj : adjs->GetList()) {
      AddEdge(node, adj);
    }
  }
  for (const auto &[src, dst] : worklist_moves_->GetList()) {
    move_list_[src].Append(src, dst);
    move_list_[dst].Append(src, dst);
  }
}
void RegAllocator::MakeWorkList() {
  auto node_iter = initial_.begin();
  while (node_iter != initial_.end()) {
    auto node = *node_iter;
    if (degree_[node] >= K) {
      spill_worklist_.insert(node);
    } else if (MoveRelated(node)) {
      freeze_worklist_.insert(node);
    } else {
      simplify_worklist_.insert(node);
    }
    ++node_iter;
  }
  initial_.clear();
}
void RegAllocator::Simplify() {
  auto node_iter = simplify_worklist_.begin();
  while (node_iter != simplify_worklist_.end()) {
    auto node = *node_iter;
    node_iter = simplify_worklist_.erase(node_iter);
    select_stack_.push(node);
    auto adjacent = Adjacent(node);
    for (const auto &adj_node : adjacent) {
      DecrementDegree(adj_node);
    }
  }
}
void RegAllocator::Coalesce() {
  auto list_iter = worklist_moves_->GetList().begin();
  while (list_iter != worklist_moves_->GetList().end()) {
    auto [x, y] = *list_iter;
    x = GetAlias(x);
    y = GetAlias(y);
    std::pair<live::INodePtr, live::INodePtr> uv;
    if (SetIncludes(pre_colored_, y)) {
      uv = {y, x};
    } else {
      uv = {x, y};
    }
    // worklist_moves_->Delete(src, dst);
    auto [u, v] = uv;
    if (u == v) {
      coalesced_moves_->Union(x, y);
      AddWorkList(u);
    } else if (SetIncludes(pre_colored_, v) || adj_set_->Contain(u, v)) {
      constrained_moves_->Union(x, y);
      AddWorkList(u);
      AddWorkList(v);
    } else if ((SetIncludes(pre_colored_, u) &&
                [&] {
                  const auto adjacent = Adjacent(v);
                  for (const auto &adj_node : adjacent) {
                    if (!OK(adj_node, u))
                      return false;
                  }
                  return true;
                }()) ||
               (!SetIncludes(pre_colored_, u) &&
                Conservative(SetUnion(Adjacent(u), Adjacent(v))))) {
      coalesced_moves_->Union(x, y);
      Combine(u, v);
      AddWorkList(u);
    } else {
      if (!active_moves_->Contain(x, y))
        active_moves_->Append(x, y);
    }
    ++list_iter;
  }
  worklist_moves_->Clear();
}
void RegAllocator::Freeze() {
  auto node_iter = freeze_worklist_.begin();
  while (node_iter != freeze_worklist_.end()) {
    auto node = *node_iter;
    node_iter = freeze_worklist_.erase(node_iter);
    simplify_worklist_.insert(node);
    FreezeMoves(node);
  }
}
void RegAllocator::SelectSpill() {
  if (spill_worklist_.empty())
    return;
  auto selected = spill_worklist_.begin();
  for (auto node_iter = spill_worklist_.begin();
       node_iter != spill_worklist_.end(); ++node_iter) {
    if ((*node_iter)->Degree() > (*selected)->Degree()) {
      selected = node_iter;
    }
  }
  spill_worklist_.erase(*selected);
  simplify_worklist_.insert(*selected);
  FreezeMoves(*selected);
}
void RegAllocator::AssignColors() {
  for (int i = 0; i < K; ++i) {
    color_[live_graph_->GetNode(reg_manager->GetRegister(i))] = i;
  }
  while (!select_stack_.empty()) {
    auto node = select_stack_.top();
    select_stack_.pop();
    auto ok_colors = std::set<int>{};
    for (int i = 0; i < K; ++i) {
      ok_colors.insert(i);
    }
    for (const auto &adj_node : adj_list_[node]) {
      if (auto union_set = SetUnion(colored_nodes_, pre_colored_);
          SetIncludes(union_set, GetAlias(adj_node))) {
        ok_colors.erase(color_[GetAlias(adj_node)]);
      }
    }
    if (ok_colors.empty()) {
      spilled_nodes_.insert(node);
    } else {
      colored_nodes_.insert(node);
      color_[node] = *ok_colors.begin();
    }
  }
  for (const auto &node : coalesced_nodes_) {
    color_[node] = color_[GetAlias(node)];
  }
}
void RegAllocator::ReWriteProgram() {
  std::set<live::INodePtr> new_temps;
  for (const auto &node : spilled_nodes_) {
    const auto v = node->NodeInfo();
    const auto vi = temp::TempFactory::NewTemp();
    new_temps.insert(live_graph_->GetLiveGraph().interf_graph->NewNode(vi));
    const auto access =
        dynamic_cast<frame::InFrameAccess *>(frame_->AllocLocal(true, false));
    const auto load_instr = new assem::OperInstr(
        "movq " + frame_->GetLabel() + "_framesize" +
            std::to_string(access->offset) + "(`s0), `d0",
        new temp::TempList(vi), new temp::TempList(reg_manager->StackPointer()),
        nullptr);
    const auto store_instr =
        new assem::OperInstr("movq `s0, " + frame_->GetLabel() + "_framesize" +
                                 std::to_string(access->offset) + "(`d0)",
                             new temp::TempList(reg_manager->StackPointer()),
                             new temp::TempList(vi), nullptr);
    auto &instr_list = assem_instr_->GetInstrList()->GetRef();
    auto instr_iter = instr_list.begin();
    while (instr_iter != instr_list.end()) {
      if (instr_iter.operator*()->Use()->Replace(v, vi)) {
        instr_list.insert(instr_iter, load_instr);
      } else if (instr_iter.operator*()->Def()->Replace(v, vi)) {
        // def of the in-frame-access
        instr_iter.operator++();
        instr_list.insert(instr_iter, store_instr);
        instr_iter.operator--();
      }
      instr_iter.operator++();
    }
  }
  assem_instr_->Print(
      stdout, temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name()));
  spilled_nodes_.clear();
  initial_ = SetUnion(colored_nodes_, coalesced_nodes_);
  initial_ = SetUnion(initial_, new_temps);
  colored_nodes_.clear();
  coalesced_nodes_.clear();
}
void RegAllocator::SimplifyProgram() {
  auto &instr_list = assem_instr_->GetInstrList()->GetRef();
  auto iter = instr_list.begin();
  while (iter != instr_list.end()) {
    if (auto &instr = *iter; typeid(*instr) == typeid(assem::MoveInstr)) {
      const auto use = instr->Use()->GetList().front();
      const auto def = instr->Def()->GetList().front();
      if(!use || !def) {
        ++iter;
        continue;
      }
      if (color_[live_graph_->GetNode(use)] ==
          color_[live_graph_->GetNode(def)]) {
        iter = instr_list.erase(iter);
        continue;
      } else {
        ++iter;
        continue;
      }
    }
    ++iter;
  }
}
void RegAllocator::ClearAndInit() {
  pre_colored_.clear();
  initial_.clear();
  simplify_worklist_.clear();
  freeze_worklist_.clear();
  spill_worklist_.clear();
  spilled_nodes_.clear();
  coalesced_nodes_.clear();
  colored_nodes_.clear();
  while (!select_stack_.empty())
    select_stack_.pop();
  coalesced_moves_ = new live::MoveList();
  constrained_moves_ = new live::MoveList();
  frozen_moves_ = new live::MoveList();
  worklist_moves_ = new live::MoveList();
  active_moves_ = new live::MoveList();
  adj_set_ = new live::MoveList();
  adj_list_.clear();
  degree_.clear();
  move_list_.clear();
  alias_.clear();
  color_.clear();
  flow_graph_ = new fg::FlowGraphFactory(assem_instr_->GetInstrList());
  flow_graph_->AssemFlowGraph();
  live_graph_ = new live::LiveGraphFactory(flow_graph_->GetFlowGraph());
  live_graph_->Liveness();
  pre_colored_ = live_graph_->GetPrecolored();
  for (const auto &node :
       live_graph_->GetLiveGraph().interf_graph->Nodes()->GetList()) {
    if (pre_colored_.find(node) == pre_colored_.end()) {
      initial_.insert(node);
    }
  }
}
void RegAllocator::AddEdge(const live::INodePtr src, const live::INodePtr dst) {
  if (!adj_set_->Contain(src, dst) && src != dst) {
    adj_set_->Append(src, dst);
    adj_set_->Append(dst, src);
    if (!SetIncludes(pre_colored_, src)) {
      adj_list_[src].insert(dst);
      degree_[src]++;
    }
    if (!SetIncludes(pre_colored_, dst)) {
      adj_list_[dst].insert(src);
      degree_[dst]++;
    }
  }
}
void RegAllocator::DecrementDegree(const live::INodePtr node) {
  if (SetIncludes(pre_colored_, node))
    return;
  const auto d = degree_[node];
  degree_[node] = d - 1;
  if (d == K) {
    auto new_set = Adjacent(node);
    new_set.insert(node);
    EnableMoves(new_set);
    spill_worklist_.erase(node);
    if (MoveRelated(node)) {
      freeze_worklist_.insert(node);
    } else {
      simplify_worklist_.insert(node);
    }
  }
}
std::set<live::INodePtr> RegAllocator::Adjacent(const live::INodePtr node) {
  const auto nodelist = adj_list_[node];
  auto stackk = select_stack_;
  std::set<live::INodePtr> stack_set;
  while (!stackk.empty()) {
    stack_set.insert(stackk.top());
    stackk.pop();
  }
  const auto diff = SetUnion(stack_set, coalesced_nodes_);
  auto ret = SetDiff(nodelist, diff);
  return ret;
}
live::MoveList *RegAllocator::NodeMoves(const live::INodePtr node) {
  return move_list_[node].Intersect(active_moves_->Union(worklist_moves_));
}
void RegAllocator::EnableMoves(const std::set<live::INodePtr> &nodes) {
  for (const auto &node : nodes) {
    for (const auto &[src, dst] : NodeMoves(node)->GetList()) {
      if (active_moves_->Contain(src, dst)) {
        active_moves_->Delete(src, dst);
        if (!worklist_moves_->Contain(src, dst))
          worklist_moves_->Append(src, dst);
      }
    }
  }
}
bool RegAllocator::MoveRelated(const live::INodePtr node) {
  return !NodeMoves(node)->GetList().empty();
}
auto RegAllocator::GetAlias(const live::INodePtr node) -> live::INodePtr {
  if (SetIncludes(coalesced_nodes_, node)) {
    return GetAlias(alias_[node]);
  }
  return node;
}
void RegAllocator::AddWorkList(const live::INodePtr u) {
  if (!SetIncludes(pre_colored_, u) && !MoveRelated(u) && degree_[u] < K) {
    freeze_worklist_.erase(u);
    simplify_worklist_.insert(u);
  }
}
bool RegAllocator::OK(const live::INodePtr t, const live::INodePtr r) {
  return degree_[t] < K || SetIncludes(pre_colored_, t) ||
         adj_set_->Contain(t, r);
}
bool RegAllocator::Conservative(const std::set<live::INodePtr> &nodes) {
  int k = 0;
  for (const auto &node : nodes)
    if (degree_[node] >= K)
      ++k;
  return k < K;
}
void RegAllocator::Combine(live::INodePtr u, live::INodePtr v) {
  if (SetIncludes(freeze_worklist_, v)) {
    freeze_worklist_.erase(v);
  } else {
    spill_worklist_.erase(v);
  }
  coalesced_nodes_.insert(v);
  alias_[v] = u;
  move_list_[u] = *NodeMoves(u)->Union(NodeMoves(v));
  for (const auto &t : Adjacent(v)) {
    AddEdge(t, u);
    DecrementDegree(t);
  }
  if (degree_[u] >= K && SetIncludes(freeze_worklist_, u)) {
    freeze_worklist_.erase(u);
    spill_worklist_.insert(u);
  }
}
void RegAllocator::FreezeMoves(const live::INodePtr u) {
  live::INodePtr v;
  for (auto [x, y] : NodeMoves(u)->GetList()) {
    if (GetAlias(y) == GetAlias(x)) {
      v = GetAlias(x);
    } else {
      v = GetAlias(y);
    }
    active_moves_->Delete(x, y);
    frozen_moves_->Union(x, y);
    if (NodeMoves(v)->GetList().empty() && degree_[v] < K) {
      freeze_worklist_.erase(v);
      simplify_worklist_.insert(v);
    }
  }
}
std::set<live::INodePtr>
RegAllocator::SetDiff(const std::set<live::INodePtr> &set1,
                      const std::set<live::INodePtr> &set2) {
  std::vector<live::INodePtr> vec(set1.size());
  auto iter = vec.begin();
  iter = std::set_difference(set1.begin(), set1.end(), set2.begin(), set2.end(),
                             vec.begin());
  vec.resize(iter - vec.begin());
  return {vec.begin(), vec.end()};
}
bool RegAllocator::SetIncludes(std::set<live::INodePtr> &set,
                               const live::INodePtr member) {
  if (set.find(member) != set.end())
    return true;
  return false;
}
std::set<live::INodePtr>
RegAllocator::SetUnion(const std::set<live::INodePtr> &set1,
                       const std::set<live::INodePtr> &set2) {
  std::vector<live::INodePtr> vec(set1.size() + set2.size());
  auto iter = vec.begin();
  iter = std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(),
                        vec.begin());
  vec.resize(iter - vec.begin());
  return {vec.begin(), vec.end()};
}
} // namespace ra