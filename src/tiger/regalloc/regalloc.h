#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

#include <map>

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() {}
};

class RegAllocator {
public:
  RegAllocator(frame::Frame *frame,
               std::unique_ptr<cg::AssemInstr> assem_instr);
  void RegAlloc();
  std::unique_ptr<ra::Result> TransferResult();

private:
  void Build();
  void MakeWorkList();
  void Simplify();
  void Coalesce();
  void Freeze();
  void SelectSpill();
  void AssignColors();
  void ReWriteProgram();

  // tool functions
  void ClearAndInit();
  void AddEdge(live::INodePtr src, live::INodePtr dst);
  void DecrementDegree(live::INodePtr node);
  std::set<live::INodePtr> Adjacent(live::INodePtr node);
  live::MoveList *NodeMoves(live::INodePtr node);
  void EnableMoves(const std::set<live::INodePtr> &nodes);
  bool MoveRelated(live::INodePtr node);
  live::INodePtr GetAlias(live::INodePtr);
  void AddWorkList(live::INodePtr);
  bool OK(live::INodePtr t, live::INodePtr r);
  bool Conservative(const std::set<live::INodePtr> &nodes);
  void Combine(live::INodePtr u, live::INodePtr v);
  void FreezeMoves(live::INodePtr u);
  // set operations
  static std::set<live::INodePtr> SetDiff(const std::set<live::INodePtr> &set1,
                                          const std::set<live::INodePtr> &set2);
  static bool SetIncludes(std::set<live::INodePtr> &set, live::INodePtr member);
  static std::set<live::INodePtr>
  SetUnion(const std::set<live::INodePtr> &set1,
           const std::set<live::INodePtr> &set2);

  // members
  int K;
  live::LiveGraphFactory *live_graph_;
  fg::FlowGraphFactory *flow_graph_;
  frame::Frame *frame_;
  std::unique_ptr<cg::AssemInstr> assem_instr_;

  // nodes/temps
  std::set<live::INodePtr> pre_colored_;
  std::set<live::INodePtr> initial_;
  std::set<live::INodePtr>
      simplify_worklist_; /* list of low-degree non-move-related nodes */
  std::set<live::INodePtr> freeze_worklist_; /* low-degree move-related nodes */
  std::set<live::INodePtr> spill_worklist_;  /* high-degree nodes */
  std::set<live::INodePtr>
      spilled_nodes_; /* nodes marked for spilling during this round */
  std::set<live::INodePtr>
      coalesced_nodes_; /* registers that have been coalesced */
  std::set<live::INodePtr> colored_nodes_;
  std::stack<live::INodePtr>
      select_stack_; /* stack containing temporaries removed from the graph */

  // move instructions
  live::MoveList *coalesced_moves_; /* moves that have been coalesced */
  live::MoveList
      *constrained_moves_; /* moves whose source and target interfere */
  live::MoveList
      *frozen_moves_; /* moves that will no longer be considered for coalescing
                       */
  live::MoveList *worklist_moves_; /* moves enabled for possible coalescing */
  live::MoveList *active_moves_;   /* moves not yet ready for coalescing */

  // other data-structures
  live::MoveList *adj_set_; /* set of interference edges */
  std::map<live::INodePtr, std::set<live::INodePtr>>
      adj_list_; /* adjacecy list representation of the graph */
  std::map<live::INodePtr, int> degree_; /* degree of each node */
  std::map<live::INodePtr, live::MoveList>
      move_list_; /* from a node to the list of moves it associated with */
  std::map<live::INodePtr, live::INodePtr>
      alias_;                           /* coalesced (u, v), alias(v) = u */
  std::map<live::INodePtr, int> color_; /* color of the node */
};

} // namespace ra

#endif