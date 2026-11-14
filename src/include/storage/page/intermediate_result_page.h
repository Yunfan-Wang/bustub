#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "storage/table/tuple.h"

namespace bustub {

/**
 * Page to hold the intermediate data for external merge sort and hash join.
 * Supports variable-length tuples.
 */
class IntermediateResultPage {
 public:
  /**
   * TODO(P3): Define and implement the methods for reading data from and writing data to the sort
   * page. Feel free to add other helper methods.
   */
  void Init() {
    count_ = 0;
    end_ = sizeof(data_);
  }

  auto Size() const -> uint32_t { return count_; }

  auto Append(const Tuple &tuple) -> bool {
    const auto length = tuple.GetLength();
    const auto slot_offset = count_ * sizeof(Slot);
    const auto slot_end = slot_offset + sizeof(Slot);
    if (slot_end + length > end_) {
      return false;
    }
    end_ -= length;
    const Slot slot{end_, length};
    std::memcpy(data_ + slot_offset, &slot, sizeof(slot));
    if (length != 0) {
      std::memcpy(data_ + end_, tuple.GetData(), length);
    }
    count_++;
    return true;
  }

  auto GetTuple(uint32_t index) const -> Tuple {
    Slot slot;
    std::memcpy(&slot, data_ + index * sizeof(Slot), sizeof(slot));
    if (slot.length == 0) {
      return Tuple{};
    }
    return Tuple(RID{}, data_ + slot.offset, slot.length);
  }

 private:
  /**
   * TODO(P3): Define the private members. You may want to have some necessary metadata for
   * the sort page before the start of the actual data.
   */
  struct Slot {
    uint32_t offset;
    uint32_t length;
  };

  uint32_t count_;
  uint32_t end_;
  char data_[BUSTUB_PAGE_SIZE - 2 * sizeof(uint32_t)];
};

}  // namespace bustub
