#include "storage/page/intermediate_result_page.h"
#include "gtest/gtest.h"
#include "type/value_factory.h"

namespace bustub {

TEST(IntermediateResultPageLocalTest, full) {
  IntermediateResultPage page;
  page.Init();
  Schema schema({Column("s", TypeId::VARCHAR, 256)});
  Tuple first({ValueFactory::GetVarcharValue("first")}, &schema);
  Tuple rest({ValueFactory::GetVarcharValue(std::string(200, 'x'))}, &schema);
  ASSERT_TRUE(page.Append(first));
  while (page.Append(rest)) {
  }
  ASSERT_GT(page.Size(), 1);
  const auto count = page.Size();
  EXPECT_FALSE(page.Append(rest));
  EXPECT_EQ(count, page.Size());
  EXPECT_EQ("first", page.GetTuple(0).GetValue(&schema, 0).ToString());
  EXPECT_EQ(std::string(200, 'x'), page.GetTuple(count - 1).GetValue(&schema, 0).ToString());

  page.Init();
  EXPECT_EQ(0, page.Size());
  EXPECT_TRUE(page.Append(rest));
}

}
