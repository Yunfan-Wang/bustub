#include "common/bustub_instance.h"
#include "gtest/gtest.h"

namespace bustub {

TEST(HashJoinLocalTest, skew) {
  BusTubInstance db(8);
  StringVectorWriter writer;
  ASSERT_TRUE(db.ExecuteSql("create table l (a int)", writer));
  ASSERT_TRUE(db.ExecuteSql("create table r (b int, payload varchar(128))", writer));
  ASSERT_TRUE(db.ExecuteSql("insert into l values (1), (2), (null)", writer));

  std::string sql = "insert into r values (1, 'abcdefghijklmnopqrstuvwxyz')";
  for (int i = 1; i < 400; i++) {
    sql += ", (1, 'abcdefghijklmnopqrstuvwxyz')";
  }
  ASSERT_TRUE(db.ExecuteSql(sql, writer));
  ASSERT_TRUE(db.ExecuteSql("select count(*), count(b) from l left join r on a = b", writer));
 
  const std::vector<std::vector<std::string>> expected{{"402", "400"}};
  EXPECT_EQ(expected, writer.values_);
}

}
