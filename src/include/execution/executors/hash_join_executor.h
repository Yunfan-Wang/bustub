//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// hash_join_executor.h
//
// Identification: src/include/execution/executors/hash_join_executor.h
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <vector>

#include <array>
#include <unordered_map>

#include "execution/executor_context.h"
#include "execution/executors/abstract_executor.h"
#include "execution/plans/hash_join_plan.h"
#include "storage/table/tuple.h"
#include "execution/plans/aggregation_plan.h"

namespace bustub {

/**
 * HashJoinExecutor executes a nested-loop JOIN on two tables.
 */
class HashJoinExecutor : public AbstractExecutor {
 public:
  HashJoinExecutor(ExecutorContext *exec_ctx, const HashJoinPlanNode *plan,
                   std::unique_ptr<AbstractExecutor> &&left_child, std::unique_ptr<AbstractExecutor> &&right_child);

  ~HashJoinExecutor() override;

  void Init() override;

  auto Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch, size_t batch_size)
      -> bool override;

  /** @return The output schema for the join */
  auto GetOutputSchema() const -> const Schema & override { return plan_->OutputSchema(); };

 private:
  /** The HashJoin plan node to be executed. */

  static constexpr size_t PARTITIONS = 16;
  static constexpr size_t BUILD_BYTES = 4096;

  using Partition = std::vector<page_id_t>;
  using Partitions = std::array<Partition, PARTITIONS>;

  void PartitionInput(AbstractExecutor *child, const std::vector<AbstractExpressionRef> &keys, bool keep_nulls,
                      Partitions *partitions);
  void ClearPages();
  auto LoadLeftPage() -> bool;
  auto BuildChunk() -> bool;


  const HashJoinPlanNode *plan_;
  std::unique_ptr<AbstractExecutor> left_;
  std::unique_ptr<AbstractExecutor> right_;
  
  Partitions left_pages_;
  Partitions right_pages_;
  std::vector<Tuple> left_rows_;
  std::vector<bool> matched_;
  
  std::unordered_map<AggregateKey, std::vector<Tuple>> build_;
  const std::vector<Tuple> *matches_{nullptr};
  
  size_t partition_{0};
  size_t left_page_{0};
  size_t right_page_{0};
  uint32_t right_slot_{0};
  
  size_t left_row_{0};
  size_t match_row_{0};
  
  bool emit_unmatched_{false};
  
};

}  // namespace bustub
