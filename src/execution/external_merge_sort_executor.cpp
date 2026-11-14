//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// external_merge_sort_executor.cpp
//
// Identification: src/execution/external_merge_sort_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/external_merge_sort_executor.h"
#include <vector>
#include "common/macros.h"
#include "execution/plans/sort_plan.h"

namespace bustub {

MergeSortRun::~MergeSortRun() {
  for (auto pid : pages_) {
    bpm_->DeletePage(pid);
  }
}

auto MergeSortRun::operator=(MergeSortRun &&other) noexcept -> MergeSortRun & {
  pages_.swap(other.pages_);
  std::swap(bpm_, other.bpm_);
  return *this;
}

void MergeSortRun::Append(const Tuple &tuple) {
  if (!pages_.empty()) {
    auto guard = bpm_->WritePage(pages_.back());
    if (guard.AsMut<IntermediateResultPage>()->Append(tuple)) {
      return;
    }
  }
  pages_.push_back(bpm_->NewPage());
  auto guard = bpm_->WritePage(pages_.back());
  auto *page = guard.AsMut<IntermediateResultPage>();
  page->Init();
  page->Append(tuple);
}

auto MergeSortRun::Iterator::operator++() -> Iterator & {
  auto guard = run_->bpm_->ReadPage(run_->pages_[page_]);
  slot_++;
  if (slot_ == guard.As<IntermediateResultPage>()->Size()) {
    page_++;
    slot_ = 0;
  }
  return *this;
}

auto MergeSortRun::Iterator::operator*() -> Tuple {
  auto guard = run_->bpm_->ReadPage(run_->pages_[page_]);
  return guard.As<IntermediateResultPage>()->GetTuple(slot_);
}

template <size_t K>
ExternalMergeSortExecutor<K>::ExternalMergeSortExecutor(ExecutorContext *exec_ctx, const SortPlanNode *plan,
                                                        std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), , plan_(plan), cmp_(plan->GetOrderBy()), child_executor_(std::move(child_executor)) {
}

/** Initialize the external merge sort */
template <size_t K>
void ExternalMergeSortExecutor<K>::Init() {
  run_ = MergeSortRun{};
  child_executor_->Init();
  
  auto *bpm = exec_ctx_->GetBufferPoolManager();
  std::vector<MergeSortRun> runs;
  std::vector<SortEntry> entries;
  IntermediateResultPage space;
  
  space.Init();

  auto save_run = [&]() {
    std::sort(entries.begin(), entries.end(), cmp_);
    runs.emplace_back(std::vector<page_id_t>{}, bpm);
    for (const auto &entry : entries) {
      runs.back().Append(entry.second);
    }
    entries.clear();
    space.Init();
  };

  std::vector<Tuple> tuples;
  std::vector<RID> rids;
  
  while (child_executor_->Next(&tuples, &rids, BUSTUB_BATCH_SIZE)) {
    for (auto &tuple : tuples) {
      if (!space.Append(tuple)) {
        save_run();
        space.Append(tuple);
      }
      auto key = GenerateSortKey(tuple, plan_->GetOrderBy(), GetOutputSchema());
      entries.emplace_back(std::move(key), std::move(tuple));
    }
  }
  if (!entries.empty()) {
    save_run();
  }

  while (runs.size() > 1) {
    std::vector<MergeSortRun> next;
    for (size_t i = 0; i + 1 < runs.size(); i += 2) {
      next.push_back(Merge(runs[i], runs[i + 1]));
      runs[i] = MergeSortRun{};
      runs[i + 1] = MergeSortRun{};
    }
    if (runs.size() % 2 != 0) {
      next.push_back(std::move(runs.back()));
    }
    runs = std::move(next);
  }
  if (!runs.empty()) {
    run_ = std::move(runs.front());
  }
  it_ = run_.Begin();
}

template <size_t K>
auto ExternalMergeSortExecutor<K>::Merge(MergeSortRun &left, MergeSortRun &right) -> MergeSortRun {
  MergeSortRun merged({}, exec_ctx_->GetBufferPoolManager());
  auto a = left.Begin();
  auto b = right.Begin();
  while (a != left.End() && b != right.End()) {
    auto lt = *a;
    auto rt = *b;
    SortEntry l{GenerateSortKey(lt, plan_->GetOrderBy(), GetOutputSchema()), std::move(lt)};
    SortEntry r{GenerateSortKey(rt, plan_->GetOrderBy(), GetOutputSchema()), std::move(rt)};
    if (cmp_(r, l)) {
      merged.Append(r.second);
      ++b;
    } else {
      merged.Append(l.second);
      ++a;
    }
  }
  for (; a != left.End(); ++a) {
    merged.Append(*a);
  }
  for (; b != right.End(); ++b) {
    merged.Append(*b);
  }
  return merged;
}


/**
 * Yield the next tuple batch from the external merge sort.
 * @param[out] tuple_batch The next tuple batch produced by the external merge sort.
 * @param[out] rid_batch The next tuple RID batch produced by the external merge sort.
 * @param batch_size The number of tuples to be included in the batch (default: BUSTUB_BATCH_SIZE)
 * @return `true` if a tuple was produced, `false` if there are no more tuples
 */
template <size_t K>
auto ExternalMergeSortExecutor<K>::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch,
                                        size_t batch_size) -> bool {
  
  tuple_batch->clear();
  rid_batch->clear();
  
  while (it_ != run_.End() && tuple_batch->size() < batch_size) {
    tuple_batch->push_back(*it_);
    rid_batch->emplace_back();
    ++it_;
  }

  if (it_ == run_.End()) {
    run_ = MergeSortRun{};
    it_ = run_.End();
  }

  return !tuple_batch->empty();
}

template class ExternalMergeSortExecutor<2>;

}  // namespace bustub
