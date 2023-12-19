#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  FNodePtr prev = nullptr;
  tab::Table<assem::Instr, graph::Node<assem::Instr>> instr_table;
  for (const auto &instr : this->instr_list_->GetList()) {
    const auto node = this->flowgraph_->NewNode(instr);
    instr_table.Enter(instr, node);
    if (prev) {
      flowgraph_->AddEdge(prev,node);
    }
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      const auto label_instr = dynamic_cast<assem::LabelInstr *>(instr);
      label_map_->Enter(label_instr->label_, node);
    } else if (typeid(*instr) == typeid(assem::OperInstr)) {
      if (const auto op_instr = dynamic_cast<assem::OperInstr *>(instr);
          op_instr->jumps_) {
        prev = nullptr;
        continue;
      }
    }
    prev = node;
  }

  for (const auto &instr : instr_list_->GetList()) {
    const auto node = instr_table.Look(instr);
    if (typeid(*instr) == typeid(assem::OperInstr)) {
      const auto op_instr = dynamic_cast<assem::OperInstr *>(instr);
      if (!op_instr->jumps_)
        continue;
      auto it = op_instr->jumps_->labels_->begin();
      while (it != op_instr->jumps_->labels_->end()) {
        const auto dst_node = label_map_->Look(*it);
        flowgraph_->AddEdge(node, dst_node);
        ++it;
      }
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Def() const {
  if (this->dst_) {
    return dst_;
  }
  return new temp::TempList();
}

temp::TempList *OperInstr::Def() const {
  if (dst_)
    return dst_;
  return new temp::TempList();
}

temp::TempList *LabelInstr::Use() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Use() const {
  if (src_)
    return src_;
  return new temp::TempList();
}

temp::TempList *OperInstr::Use() const {
  if (src_)
    return src_;
  return new temp::TempList();
}
} // namespace assem
