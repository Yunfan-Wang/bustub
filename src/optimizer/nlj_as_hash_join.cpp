//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nlj_as_hash_join.cpp
//
// Identification: src/optimizer/nlj_as_hash_join.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <memory>
#include "catalog/column.h"
#include "catalog/schema.h"
#include "common/exception.h"
#include "common/macros.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/comparison_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "execution/plans/abstract_plan.h"
#include "execution/plans/filter_plan.h"
#include "execution/plans/hash_join_plan.h"
#include "execution/plans/nested_loop_join_plan.h"
#include "execution/plans/projection_plan.h"
#include "optimizer/optimizer.h"
#include "type/type_id.h"

#include "execution/expressions/logic_expression.h"

namespace bustub {

/**
 * @brief optimize nested loop join into hash join.
 * In the starter code, we will check NLJs with exactly one equal condition. You can further support optimizing joins
 * with multiple eq conditions.
 */

namespace {

auto CollectKeys(const AbstractExpressionRef &expr, std::vector<AbstractExpressionRef> *left,
                 std::vector<AbstractExpressionRef> *right) -> bool {
  if (const auto *logic = dynamic_cast<const LogicExpression *>(expr.get()); logic != nullptr) {
    if (logic->logic_type_ != LogicType::And) {
      return false;
    }
    if (!CollectKeys(expr->GetChildAt(0), left, right)) {
      return false;
    }
    return CollectKeys(expr->GetChildAt(1), left, right);
  }
  
  const auto *comparison = dynamic_cast<const ComparisonExpression *>(expr.get());
  if (comparison == nullptr || comparison->comp_type_ != ComparisonType::Equal) {
    return false;
  }
  
  const auto *lhs = dynamic_cast<const ColumnValueExpression *>(expr->GetChildAt(0).get());
  const auto *rhs = dynamic_cast<const ColumnValueExpression *>(expr->GetChildAt(1).get());
  
  if (lhs == nullptr || rhs == nullptr || lhs->GetTupleIdx() == rhs->GetTupleIdx()) {
    return false;
  }
  
  if (lhs->GetReturnType().GetType() != rhs->GetReturnType().GetType()) {
    return false;
  }
  
  if (lhs->GetTupleIdx() == 1) {
    std::swap(lhs, rhs);
  }
  
  left->push_back(std::make_shared<ColumnValueExpression>(0, lhs->GetColIdx(), lhs->GetReturnType()));
  right->push_back(std::make_shared<ColumnValueExpression>(0, rhs->GetColIdx(), rhs->GetReturnType()));
  return true;
}

}
auto Optimizer::OptimizeNLJAsHashJoin(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  // TODO(student): implement NestedLoopJoin -> HashJoin optimizer rule
  // Note for Spring 2025: You should support join keys of any number of conjunction of equi-conditions:
  // E.g. <column expr> = <column expr> AND <column expr> = <column expr> AND ...
  std::vector<AbstractPlanNodeRef> children;
  
  for (const auto &child : plan->GetChildren()) {
    children.push_back(OptimizeNLJAsHashJoin(child));
  }
  
  auto result = plan->CloneWithChildren(std::move(children));
  const auto *join = dynamic_cast<const NestedLoopJoinPlanNode *>(result.get());
  if (join == nullptr || (join->GetJoinType() != JoinType::INNER && join->GetJoinType() != JoinType::LEFT)) {
    return result;
  }
  
  std::vector<AbstractExpressionRef> left;
  std::vector<AbstractExpressionRef> right;
  // check recursion here if bug: Critl1
  if (!CollectKeys(join->Predicate(), &left, &right)) {
    return result;
  }
  
  return std::make_shared<HashJoinPlanNode>(join->output_schema_, join->GetLeftPlan(), join->GetRightPlan(), std::move(left), std::move(right), join->GetJoinType());

}

}  // namespace bustub
