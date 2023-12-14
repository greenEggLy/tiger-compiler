#ifndef TIGER_LIVENESS_LIVENESS_H_
#define TIGER_LIVENESS_LIVENESS_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/util/graph.h"

#include <set>

namespace live {

using INode = graph::Node<temp::Temp>;
using INodePtr = graph::Node<temp::Temp> *;
using INodeList = graph::NodeList<temp::Temp>;
using INodeListPtr = graph::NodeList<temp::Temp> *;
using IGraph = graph::Graph<temp::Temp>;
using IGraphPtr = graph::Graph<temp::Temp> *;

class MoveList {
public:
  MoveList() = default;

  [[nodiscard]] const std::list<std::pair<INodePtr, INodePtr>> &
  GetList() const {
    return move_list_;
  }
  void Append(INodePtr src, INodePtr dst) { move_list_.emplace_back(src, dst); }
  bool Contain(INodePtr src, INodePtr dst);
  void Delete(INodePtr src, INodePtr dst);
  void Prepend(INodePtr src, INodePtr dst) {
    move_list_.emplace_front(src, dst);
  }
  void Clear() {
    while (!move_list_.empty()) {
      move_list_.pop_back();
    }
  }
  void Union(const INodePtr src, const INodePtr dst) {
    if (Contain(src, dst))
      return;
    Append(src, dst);
  }
  MoveList *Union(MoveList *list);
  MoveList *Intersect(MoveList *list);

private:
  std::list<std::pair<INodePtr, INodePtr>> move_list_;
};

struct LiveGraph {
  IGraphPtr interf_graph;
  MoveList *moves;

  LiveGraph(IGraphPtr interf_graph, MoveList *moves)
      : interf_graph(interf_graph), moves(moves) {}
};

class LiveGraphFactory {
public:
  explicit LiveGraphFactory(fg::FGraphPtr flowgraph)
      : flowgraph_(flowgraph), live_graph_(new IGraph(), new MoveList()),
        in_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
        out_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
        temp_node_map_(new tab::Table<temp::Temp, INode>()) {}
  void Liveness();
  LiveGraph GetLiveGraph() { return live_graph_; }
  tab::Table<temp::Temp, INode> *GetTempNodeMap() { return temp_node_map_; }
  std::set<INodePtr> &GetPrecolored() { return precolored_; }
  temp::TempList *LiveOut(graph::Node<assem::Instr> *instr) const;
  temp::TempList *Use(graph::Node<assem::Instr> *instr) const;
  INodePtr GetNode(temp::Temp *temp) const;

private:
  fg::FGraphPtr flowgraph_;
  LiveGraph live_graph_;

  std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> in_;
  std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> out_;
  tab::Table<temp::Temp, INode> *temp_node_map_;
  std::set<INodePtr> precolored_;

  void LiveMap();
  void InterfGraph();
  static bool SetIncludes(const std::set<INodePtr> &set,
                          const INodePtr &member);
  static std::set<temp::Temp *> MakeSet(const temp::TempList *list);
  // std::set<temp::Temp *> MakeSet(temp::TempList *list);
  static std::set<temp::Temp *> UnionSet(const std::set<temp::Temp *> &left,
                                         const std::set<temp::Temp *> &right);
  static std::set<temp::Temp *> DiffSet(const std::set<temp::Temp *> &left,
                                        const std::set<temp::Temp *> &right);
  static void MakeList(const std::set<temp::Temp *> &set,
                       temp::TempList *&list);
};

} // namespace live

#endif