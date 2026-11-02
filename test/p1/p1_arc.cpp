#include <thread>
#include <vector>

#include "buffer/arc_replacer.h"
#include "gtest/gtest.h"

namespace bustub {

TEST(P1Arc, ghost) {
  ArcReplacer arc(7);

  for (int frame = 0; frame < 6; ++frame) {
    arc.RecordAccess(frame, frame + 1);
    arc.SetEvictable(frame, frame != 5);
  }
  ASSERT_EQ(5, arc.Size());

  arc.RecordAccess(0, 1);

  ASSERT_EQ(1, arc.Evict());
  ASSERT_EQ(2, arc.Evict());
  ASSERT_EQ(3, arc.Evict());
  ASSERT_EQ(2, arc.Size());
  // cp1 --
  arc.RecordAccess(1, 7);
  arc.SetEvictable(1, true);

  arc.RecordAccess(2, 2);
  arc.SetEvictable(2, true);
  ASSERT_EQ(4, arc.Size());

  arc.RecordAccess(3, 3);
  arc.SetEvictable(3, true);
  arc.RecordAccess(6, 4);
  arc.SetEvictable(6, true);
  ASSERT_EQ(6, arc.Size());

  ASSERT_EQ(4, arc.Evict());


  
  ASSERT_EQ(0, arc.Evict());

  arc.RecordAccess(4, 1);
  arc.SetEvictable(4, true);
  ASSERT_EQ(5, arc.Size());

  ASSERT_EQ(1, arc.Evict());
}

}
