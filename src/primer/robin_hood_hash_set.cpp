//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// robin_hood_hash_set.cpp
//
// Identification: src/primer/robin_hood_hash_set.cpp
//
// Copyright (c) 2015-2026, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/robin_hood_hash_set.h"

#include <cstdint>
#include <string>

namespace bustub {

template <typename KeyType, typename Hash, typename KeyEqual>
RobinHoodHashSet<KeyType, Hash, KeyEqual>::RobinHoodHashSet(size_t capacity, Hash hasher, KeyEqual key_equal)
    : buckets_(capacity), size_(0), hasher_(std::move(hasher)), key_equal_(std::move(key_equal)) {
  if (capacity == 0) {
    throw std::invalid_argument("RobinHoodHashSet capacity must be greater than 0");
  }
}

template <typename KeyType, typename Hash, typename KeyEqual>
RobinHoodHashSet<KeyType, Hash, KeyEqual>::RobinHoodHashSet(RobinHoodHashSet &&other) noexcept {
  /** @TODO(student) Move the table's storage, metadata, functors, and synchronization state safely. */
  // static_cast<void>(other);
  std::unique_lock<std::shared_mutex> guard(other.latch_);

  buckets_ = std::move(other.buckets_);
  size_.store(other.size_.load());
  hasher_ = std::move(other.hasher_);
  key_equal_ = std::move(other.key_equal_);

  other.buckets_.clear();
  other.size_.store(0);
}

template <typename KeyType, typename Hash, typename KeyEqual>
auto RobinHoodHashSet<KeyType, Hash, KeyEqual>::operator=(RobinHoodHashSet &&other) noexcept -> RobinHoodHashSet & {
  /** @TODO(student) Implement move assignment, including self-assignment protection. */
  // static_cast<void>(other);
  if (this == &other) {
    return *this;
  }

  std::unique_lock<std::shared_mutex> this_guard(latch_, std::defer_lock);
  std::unique_lock<std::shared_mutex> other_guard(other.latch_, std::defer_lock);
  std::lock(this_guard, other_guard);

  buckets_ = std::move(other.buckets_);
  size_.store(other.size_.load());
  hasher_ = std::move(other.hasher_);
  key_equal_ = std::move(other.key_equal_);

  other.buckets_.clear();
  other.size_.store(0);

  return *this;
}

template <typename KeyType, typename Hash, typename KeyEqual>
auto RobinHoodHashSet<KeyType, Hash, KeyEqual>::Insert(const KeyType &key) -> bool {
  /** @TODO(student) Insert with linear probing, Robin Hood displacement, and safe concurrent access. */
  std::shared_lock<std::shared_mutex> table_guard(latch_);

  const size_t capacity = buckets_.size();

  if (capacity == 0) {
    return false;
  }

  std::lock_guard<std::mutex> key_guard(key_latches_[KeyLatchIndex(key)]);

  // pass1
  size_t index = HomeBucket(key);

  for (size_t scanned = 0; scanned < capacity; scanned++) {
    Bucket &bucket = buckets_[index];
    std::lock_guard<std::mutex> bucket_guard(bucket.latch_);

    if (bucket.state_ == BucketState::EMPTY) {
      break;
    }

    if (bucket.state_ == BucketState::OCCUPIED && key_equal_(*bucket.key_, key)) {
      return true;
    }

    index = (index + 1) % capacity;
  }

  // bucket search run out
  if (size_.load() == capacity) {
    return false;
  }

  // robinhood insertion
  KeyType candidate = key;
  index = HomeBucket(candidate);

  std::unique_lock<std::mutex> current_guard(buckets_[index].latch_);

  for (size_t scanned = 0; scanned < capacity; scanned++) {
    Bucket &bucket = buckets_[index];

    if (bucket.state_ == BucketState::EMPTY) {
      bucket.key_ = std::move(candidate);
      bucket.state_ = BucketState::OCCUPIED;

      size_.fetch_add(1);
      return true;
    }

    if (bucket.state_ == BucketState::TOMBSTONE) {
      bucket.key_ = std::move(candidate);
      bucket.state_ = BucketState::OCCUPIED;

      size_.fetch_add(1);
      return true;
    }

    const size_t candidate_distance = ProbeDistance(candidate, index);
    const size_t resident_distance = ProbeDistance(*bucket.key_, index);

    const size_t next_index = (index + 1) % capacity;

    if (scanned + 1 == capacity) {
      return false;
    }

    std::unique_lock<std::mutex> next_guard(buckets_[next_index].latch_);

    if (candidate_distance > resident_distance) {
      std::swap(candidate, *bucket.key_);
    }

    current_guard.unlock();
    current_guard = std::move(next_guard);

    index = next_index;
  }

  return false;
}

template <typename KeyType, typename Hash, typename KeyEqual>
auto RobinHoodHashSet<KeyType, Hash, KeyEqual>::Remove(const KeyType &key) -> bool {
  /** @TODO(student) Mark an occupied matching slot as a tombstone without breaking concurrent operations. */
  std::shared_lock<std::shared_mutex> table_guard(latch_);
  std::lock_guard<std::mutex> key_guard(key_latches_[KeyLatchIndex(key)]);

  const size_t capacity = buckets_.size();

  if (capacity == 0) {
    return false;
  }
  size_t index = HomeBucket(key);

  for (size_t scanned = 0; scanned < capacity; scanned++) {
    Bucket &bucket = buckets_[index];
    std::lock_guard<std::mutex> bucket_guard(bucket.latch_);
    if (bucket.state_ == BucketState::EMPTY) {
      return false;
    }

    if (bucket.state_ == BucketState::OCCUPIED && key_equal_(*bucket.key_, key)) {
      bucket.key_.reset();
      bucket.state_ = BucketState::TOMBSTONE;

      size_.fetch_sub(1);
      return true;
    }

    /*
     * OCCUPIED with wrong key: continue
     * TOMBSTONE: also continue
     */
    index = (index + 1) % capacity;
  }
  return false;
}

template <typename KeyType, typename Hash, typename KeyEqual>
void RobinHoodHashSet<KeyType, Hash, KeyEqual>::Clear() {
  /** @TODO(student) Remove occupied entries and tombstones while preserving capacity. */
  std::unique_lock<std::shared_mutex> guard(latch_);

  for (auto &bucket : buckets_) {
    bucket.key_.reset();
    bucket.state_ = BucketState::EMPTY;
  }

  size_.store(0);
}

template <typename KeyType, typename Hash, typename KeyEqual>
auto RobinHoodHashSet<KeyType, Hash, KeyEqual>::Contains(const KeyType &key) const -> bool {
  /** @TODO(student) Probe through tombstones and stop only where correctness permits. */
  std::shared_lock<std::shared_mutex> table_guard(latch_);

  const size_t capacity = buckets_.size();

  if (capacity == 0) {
    return false;
  }

  size_t index = HomeBucket(key);

  for (size_t scanned = 0; scanned < capacity; scanned++) {
    const Bucket &bucket = buckets_[index];
    std::lock_guard<std::mutex> bucket_guard(bucket.latch_);

    if (bucket.state_ == BucketState::EMPTY) {
      return false;
    }

    if (bucket.state_ == BucketState::OCCUPIED && key_equal_(*bucket.key_, key)) {
      return true;
    }

    index = (index + 1) % capacity;
  }
  return false;
}

template <typename KeyType, typename Hash, typename KeyEqual>
auto RobinHoodHashSet<KeyType, Hash, KeyEqual>::GetBucket(const KeyType &key) const -> size_t {
  /** @TODO(student) Return the physical bucket holding key, or BucketCount() when absent. */
  std::shared_lock<std::shared_mutex> table_guard(latch_);

  const size_t capacity = buckets_.size();

  if (capacity == 0) {
    return 0;
  }

  size_t index = HomeBucket(key);

  for (size_t scanned = 0; scanned < capacity; scanned++) {
    const Bucket &bucket = buckets_[index];
    std::lock_guard<std::mutex> bucket_guard(bucket.latch_);

    if (bucket.state_ == BucketState::EMPTY) {
      return capacity;
    }

    if (bucket.state_ == BucketState::OCCUPIED && key_equal_(*bucket.key_, key)) {
      return index;
    }

    index = (index + 1) % capacity;
  }

  return capacity;
}

template <typename KeyType, typename Hash, typename KeyEqual>
auto RobinHoodHashSet<KeyType, Hash, KeyEqual>::Size() const -> size_t {
  /** @TODO(student) Return the number of occupied entries safely. */
  return size_.load();
}

template <typename KeyType, typename Hash, typename KeyEqual>
auto RobinHoodHashSet<KeyType, Hash, KeyEqual>::Capacity() const -> size_t {
  /** @TODO(student) Return the fixed capacity safely. */
  std::shared_lock<std::shared_mutex> guard(latch_);
  return buckets_.size();
}

template <typename KeyType, typename Hash, typename KeyEqual>
auto RobinHoodHashSet<KeyType, Hash, KeyEqual>::LoadFactor() const -> double {
  /** @TODO(student) Return Size() / Capacity(), including moved-from handling. */
  std::shared_lock<std::shared_mutex> guard(latch_);

  if (buckets_.empty()) {
    return 0.0;
  }

  return static_cast<double>(size_.load()) / static_cast<double>(buckets_.size());
}

template <typename KeyType, typename Hash, typename KeyEqual>
auto RobinHoodHashSet<KeyType, Hash, KeyEqual>::MaxProbeDistance() const -> size_t {
  /** @TODO(student) Return the greatest probe distance among live entries. */
  std::shared_lock<std::shared_mutex> table_guard(latch_);

  if (buckets_.empty()) {
    return 0;
  }

  size_t max_distance = 0;

  for (size_t index = 0; index < buckets_.size(); index++) {
    const Bucket &bucket = buckets_[index];
    std::lock_guard<std::mutex> bucket_guard(bucket.latch_);

    if (bucket.state_ != BucketState::OCCUPIED) {
      continue;
    }

    const size_t distance = ProbeDistance(*bucket.key_, index);

    if (distance > max_distance) {
      max_distance = distance;
    }
  }

  return max_distance;
}

template class RobinHoodHashSet<int>;
template class RobinHoodHashSet<int64_t>;
template class RobinHoodHashSet<std::string>;

}  // namespace bustub
