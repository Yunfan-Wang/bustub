//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree_leaf_page.h
//
// Identification: src/include/storage/page/b_plus_tree_leaf_page.h
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "storage/page/b_plus_tree_page.h"

namespace bustub {

#define B_PLUS_TREE_LEAF_PAGE_TYPE BPlusTreeLeafPage<KeyType, ValueType, KeyComparator, NumTombs>
#define LEAF_PAGE_HEADER_SIZE 16
#define LEAF_PAGE_DEFAULT_TOMB_CNT 0
#define LEAF_PAGE_TOMB_CNT ((NumTombs < 0) ? LEAF_PAGE_DEFAULT_TOMB_CNT : NumTombs)
#define LEAF_PAGE_SLOT_CNT                                                                               \
  ((BUSTUB_PAGE_SIZE - LEAF_PAGE_HEADER_SIZE - sizeof(size_t) - (LEAF_PAGE_TOMB_CNT * sizeof(size_t))) / \
   (sizeof(KeyType) + sizeof(ValueType)))  // NOLINT

/**
 * Store indexed key and record id(record id = page id combined with slot id,
 * see include/common/rid.h for detailed implementation) together within leaf
 * page. Only support unique key.
 *
 * Leaf pages also contain a fixed buffer of "tombstone" indexes for entries
 * that have been deleted.
 *
 * Leaf page format (keys are stored in order, tomb order is up to you):
 *  --------------------
 * | HEADER | TOMB_SIZE | (where TOMB_SIZE is num_tombstones_)
 *  --------------------
 *  -----------------------------------
 * | TOMB(0) | TOMB(1) | ... | TOMB(k) |
 *  -----------------------------------
 *  ---------------------------------
 * | KEY(1) | KEY(2) | ... | KEY(n) |
 *  ---------------------------------
 *  ---------------------------------
 * | RID(1) | RID(2) | ... | RID(n) |
 *  ---------------------------------
 *
 *  Header format (size in byte, 16 bytes in total):
 *  -----------------------------------------------
 * | PageType (4) | CurrentSize (4) | MaxSize (4) |
 *  -----------------------------------------------
 *  -----------------
 * | NextPageId (4) |
 *  -----------------
 */
FULL_INDEX_TEMPLATE_ARGUMENTS_DEFN
class BPlusTreeLeafPage : public BPlusTreePage {
 public:
  // Delete all constructor / destructor to ensure memory safety
  BPlusTreeLeafPage() = delete;
  BPlusTreeLeafPage(const BPlusTreeLeafPage &other) = delete;

  void Init(int max_size = LEAF_PAGE_SLOT_CNT);

  auto GetTombstones() const -> std::vector<KeyType>;

  // Helper methods
  auto GetNextPageId() const -> page_id_t;
  void SetNextPageId(page_id_t next_page_id);
  auto KeyAt(int index) const -> KeyType;
  auto KeyRefAt(int index) const -> const KeyType & { return key_array_[index]; }
  auto ValueAt(int index) const -> const ValueType & { return rid_array_[index]; }
  auto IsTombstone(int index) const -> bool {
    for (size_t i = 0; i < num_tombstones_; i++) {
      if (tombstones_[i] == static_cast<size_t>(index)) {
        return true;
      }
    }
    return false;
  }
  auto LowerBound(const KeyType &key, const KeyComparator &comparator) const -> int {
    int low = 0;
    int high = GetSize();
    while (low < high) {
      int mid = low + (high - low) / 2;
      if (comparator(key_array_[mid], key) < 0) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    return low;
  }
  void InsertAt(int index, const KeyType &key, const ValueType &value) {
    for (int i = GetSize(); i > index; i--) {
      key_array_[i] = key_array_[i - 1];
      rid_array_[i] = rid_array_[i - 1];
    }
    for (size_t i = 0; i < num_tombstones_; i++) {
      if (tombstones_[i] >= static_cast<size_t>(index)) {
        tombstones_[i]++;
      }
    }
    key_array_[index] = key;
    rid_array_[index] = value;
    ChangeSizeBy(1);
  }
  void Revive(int index, const ValueType &value) {
    size_t count = 0;
    for (size_t i = 0; i < num_tombstones_; i++) {
      if (tombstones_[i] != static_cast<size_t>(index)) {
        tombstones_[count++] = tombstones_[i];
      }
    }
    num_tombstones_ = count;
    rid_array_[index] = value;
  }
  void EraseAt(int index) {
    size_t count = 0;
    for (size_t i = 0; i < num_tombstones_; i++) {
      auto position = tombstones_[i];
      if (position != static_cast<size_t>(index)) {
        tombstones_[count++] = position - (position > static_cast<size_t>(index) ? 1 : 0);
      }
    }
    num_tombstones_ = count;
    for (int i = index; i + 1 < GetSize(); i++) {
      key_array_[i] = key_array_[i + 1];
      rid_array_[i] = rid_array_[i + 1];
    }
    ChangeSizeBy(-1);
  }
  void DeleteAt(int index) {
    if constexpr (LEAF_PAGE_TOMB_CNT == 0) {
      EraseAt(index);
    } else {
      if (num_tombstones_ == LEAF_PAGE_TOMB_CNT) {
        int oldest = static_cast<int>(tombstones_[0]);
        EraseAt(oldest);
        if (oldest < index) {
          index--;
        }
      }
      tombstones_[num_tombstones_++] = index;
    }
  }
  void MoveTo(BPlusTreeLeafPage *recipient, int start, int count, const KeyComparator &comparator) {
    std::vector<KeyType> moved_tombs;
    for (size_t i = 0; i < num_tombstones_; i++) {
      if (tombstones_[i] >= static_cast<size_t>(start) &&
          tombstones_[i] < (static_cast<size_t>(start) + static_cast<size_t>(count))) {
        moved_tombs.push_back(key_array_[tombstones_[i]]);
      }
    }
    for (int i = start; i < start + count; i++) {
      recipient->InsertAt(recipient->LowerBound(key_array_[i], comparator), key_array_[i], rid_array_[i]);
    }
    for (int i = start + count - 1; i >= start; i--) {
      EraseAt(i);
    }
    for (const auto &key : moved_tombs) {
      recipient->DeleteAt(recipient->LowerBound(key, comparator));
    }
  }

  /**
   * @brief for test only return a string representing all keys in
   * this leaf page formatted as "(tombkey1, tombkey2, ...|key1,key2,key3,...)"
   *
   * @return std::string
   */
  auto ToString() const -> std::string {
    std::string kstr = "(";
    bool first = true;

    auto tombs = GetTombstones();
    for (size_t i = 0; i < tombs.size(); i++) {
      kstr.append(std::to_string(tombs[i].ToString()));
      if ((i + 1) < tombs.size()) {
        kstr.append(",");
      }
    }

    kstr.append("|");

    for (int i = 0; i < GetSize(); i++) {
      KeyType key = KeyAt(i);
      if (first) {
        first = false;
      } else {
        kstr.append(",");
      }

      kstr.append(std::to_string(key.ToString()));
    }
    kstr.append(")");

    return kstr;
  }

 private:
  page_id_t next_page_id_;
  size_t num_tombstones_;
  // Fixed-size tombstone buffer (indexes into key_array_ / rid_array_).
  size_t tombstones_[LEAF_PAGE_TOMB_CNT];
  // Array members for page data.
  KeyType key_array_[LEAF_PAGE_SLOT_CNT];
  ValueType rid_array_[LEAF_PAGE_SLOT_CNT];
  // (Spring 2025) Feel free to add more fields and helper functions below if needed
};

}  // namespace bustub
