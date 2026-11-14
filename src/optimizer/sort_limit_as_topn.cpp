//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// sort_limit_as_topn.cpp
//
// Identification: src/optimizer/sort_limit_as_topn.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "optimizer/optimizer.h"
#include "execution/plans/limit_plan.h"
#include "execution/plans/sort_plan.h"
#include "execution/plans/topn_plan.h"

namespace bustub {

/**
 * @brief optimize sort + limit as top N
 */
auto Optimizer::OptimizeSortLimitAsTopN(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  // TODO(student): implement sort + limit -> top N optimizer rule
  std::vector<AbstractPlanNodeRef> children;
  
  for (const auto &child : plan->GetChildren()) {
    children.push_back(OptimizeSortLimitAsTopN(child));
  }
  
  auto result = plan->CloneWithChildren(std::move(children));
  
  const auto *limit = dynamic_cast<const LimitPlanNode *>(result.get());
  if (limit == nullptr) {
    return result;
  }
  
  const auto *sort = dynamic_cast<const SortPlanNode *>(limit->GetChildAt(0).get());
  if (sort == nullptr) {
    return result;
  }
  return std::make_shared<TopNPlanNode>(limit->output_schema_, sort->GetChildAt(0), sort->GetOrderBy(), limit->GetLimit());
}

}  // namespace bustub
