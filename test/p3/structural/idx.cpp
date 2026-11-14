#include <iostream>

#include "common/bustub_instance.h"

using namespace bustub;
// very simple structure test. 先过中门再谈未来~
int main() {

  BusTubInstance db;
  SimpleStreamWriter out(std::cout, true);

  db.ExecuteSql("create table t(a int, b int, c int);", out);
  db.ExecuteSql("insert into t values (1, 2, 40), (1, 3, 50), (2, 3, 60);", out);
  db.ExecuteSql("create index ab on t(a, b);", out);

  std::cout << "prefix with filter: IndexScan, row 50\n";
  
  db.ExecuteSql("explain (o) select c from t where a = 1 and c > 40;", out);
  db.ExecuteSql("select c from t where a = 1 and c > 40;", out);

  std::cout << "or on different columns: SeqScan, rows 40 and 60\n";
  
  db.ExecuteSql("explain (o) select c from t where b = 2 or a = 2;", out);
  db.ExecuteSql("select c from t where b = 2 or a = 2;", out);
}
