//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// seqscan_as_indexscan.cpp
//
// Identification: src/optimizer/seqscan_as_indexscan.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "optimizer/optimizer.h"

#include <unordered_map>

#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/comparison_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "execution/expressions/logic_expression.h"
#include "execution/plans/index_scan_plan.h"
#include "execution/plans/seq_scan_plan.h"

namespace bustub {

namespace {

using Equalities = std::unordered_map<uint32_t, std::vector<AbstractExpressionRef>>;

auto Collect(const AbstractExpressionRef &expr, LogicType split, Equalities *keys) -> bool {
  const auto *logic = dynamic_cast<const LogicExpression *>(expr.get());
  if (logic != nullptr && logic->logic_type_ == split) {
    const bool left = Collect(expr->GetChildAt(0), split, keys);
    const bool right = Collect(expr->GetChildAt(1), split, keys);
    return left && right;
  }
  const auto *comparison = dynamic_cast<const ComparisonExpression *>(expr.get());
  if (comparison == nullptr || comparison->comp_type_ != ComparisonType::Equal) {
    return false;
  }
  for (size_t i = 0; i < 2; i++) {
    const auto *column = dynamic_cast<const ColumnValueExpression *>(expr->GetChildAt(i).get());
    const auto &other = expr->GetChildAt(1 - i);
    if (column != nullptr && dynamic_cast<const ConstantValueExpression *>(other.get()) != nullptr &&
        column->GetReturnType().GetType() == other->GetReturnType().GetType()) {
      (*keys)[column->GetColIdx()].push_back(other);
      return true;
    }
  }
  return false;
}

}


/**
 * @brief Optimizes seq scan as index scan if there's an index on a table
 */
auto Optimizer::OptimizeSeqScanAsIndexScan(const bustub::AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  // TODO(P3): implement seq scan with predicate -> index scan optimizer rule
  // The Filter Predicate Pushdown has been enabled for you in optimizer.cpp when forcing starter rule
  std::vector<AbstractPlanNodeRef> children;
  for (const auto &child : plan->GetChildren()) {
    children.push_back(OptimizeSeqScanAsIndexScan(child));
  }
  auto result = plan->CloneWithChildren(std::move(children));
  const auto *scan = dynamic_cast<const SeqScanPlanNode *>(result.get());
  if (scan == nullptr || scan->filter_predicate_ == nullptr) {
    return result;
  }

  const auto *logic = dynamic_cast<const LogicExpression *>(scan->filter_predicate_.get());
  bool is_or = logic != nullptr && logic->logic_type_ == LogicType::Or;
  Equalities equalities;
  bool all_equalities = Collect(scan->filter_predicate_, is_or ? LogicType::Or : LogicType::And, &equalities);
  if (is_or && (!all_equalities || equalities.size() != 1)) {
    return result;
  }
  std::vector<AbstractExpressionRef> best;
  index_oid_t oid{};
  auto table = catalog_.GetTable(scan->GetTableOid());
  for (const auto &index : catalog_.GetTableIndexes(table->name_)) {
    if (index->index_type_ != IndexType::BPlusTreeIndex) {
      continue;
    }
    const auto &attrs = index->index_->GetKeyAttrs();
    if (is_or && attrs.size() != 1) {
      continue;
    }
    std::vector<AbstractExpressionRef> keys;
    for (auto attr : attrs) {
      auto it = equalities.find(attr);
      if (it == equalities.end()) {
        break;
      }
      if (is_or) {
        keys = it->second;
      } else {
        keys.push_back(it->second.front());
      }
    }
    if (keys.size() > best.size()) {
      best = std::move(keys);
      oid = index->index_oid_;
    }
  }
  if (best.empty()) {
    return result;
  }
  return std::make_shared<IndexScanPlanNode>(scan->output_schema_, scan->GetTableOid(), oid, scan->filter_predicate_, std::move(best));}

}  // namespace bustub
