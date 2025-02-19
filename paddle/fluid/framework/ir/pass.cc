/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/fluid/framework/ir/pass.h"

#include "paddle/fluid/framework/ir/graph_helper.h"

namespace paddle {
namespace framework {
namespace ir {
class Graph;
}  // namespace ir
}  // namespace framework
}  // namespace paddle
#ifdef PADDLE_WITH_MKLDNN
#include "paddle/fluid/platform/mkldnn_helper.h"
#endif

namespace paddle {
namespace framework {
namespace ir {

Graph* Pass::Apply(Graph* graph) const {
  CheckPrevPass();
  PADDLE_ENFORCE_NOT_NULL(
      graph, platform::errors::InvalidArgument("Graph cannot be nullptr."));
  for (const std::string& attr : required_pass_attrs_) {
    PADDLE_ENFORCE_NE(
        attrs_.find(attr), attrs_.end(),
        platform::errors::InvalidArgument(
            "Required atrribute %s for pass < %s > is not set.", attr, Type()));
  }
  for (const std::string& attr : required_graph_attrs_) {
    PADDLE_ENFORCE_EQ(graph->Has(attr), true,
                      platform::errors::InvalidArgument(
                          "Required atrribute %s for graph is not set.", attr));
  }
  ApplyImpl(graph);
  // TODO(panyx0718): Add more verifications.
  PADDLE_ENFORCE_EQ(
      HasCircle(*graph), false,
      platform::errors::InvalidArgument(
          "Illegal pass %s. Generated graph shouldn't contain cycle.", Type()));
  PADDLE_ENFORCE_EQ(
      VarDescIsConsistency(*graph), true,
      platform::errors::InvalidArgument(
          "The VarDescs of persistable variable are not consistency."));
  applied_ = true;
  if (!graph->Has(kPassRecorder)) {
    graph->Set<PassRecorder>(kPassRecorder, new PassRecorder);
  }
  graph->Get<PassRecorder>(kPassRecorder).insert(Type());
#ifdef PADDLE_WITH_MKLDNN
  // Clear mkl-dnn cache,
  // Passes can change params, tensors, so caching need to be discarded
  ClearMKLDNNCache(paddle::platform::CPUPlace());
#endif
  return graph;
}

void Pass::Apply(ProgramDesc* main_program,
                 ProgramDesc* startup_program) const {
  PADDLE_ENFORCE_NOT_NULL(main_program, platform::errors::InvalidArgument(
                                            "main program must be provided"));
  PADDLE_ENFORCE_NOT_NULL(
      startup_program,
      platform::errors::InvalidArgument("startup program must be provided"));

  Graph graph(*main_program);
  Apply(&graph);

  // TODO(zjl): support details::kStartupProgramDescs and details::kProgramDescs
  ProgramDesc new_main_program;
  GraphToProgram(graph, &new_main_program);
  main_program->CopyFrom(*new_main_program.Proto());

  startup_program->Flush();
  main_program->Flush();
}

PassRegistry& PassRegistry::Instance() {
  static PassRegistry g_pass_info_map;
  return g_pass_info_map;
}
}  // namespace ir
}  // namespace framework
}  // namespace paddle
