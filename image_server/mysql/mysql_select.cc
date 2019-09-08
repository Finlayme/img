#include <cstdio>
#include <cstdlib>
#include <mysql/mysql.h>

int main() {
  // 使用 mysql API 来操作数据库了
  // 1. 先创建一个 mysql 的句柄
  MYSQL* mysql = mysql_init(NULL);
  // 2. 拿着句柄和数据库建立链接
  if (mysql_real_connect(mysql, "127.0.0.1", "root", "", "image_system2", 3306, NULL, 0) == NULL) {
    // 数据库链接失败
    printf("连接失败! %s\n", mysql_error(mysql));
    return 1;
  }
  // 3. 设置编码格式
  mysql_set_character_set(mysql, "utf8");
  // 4. 拼接 SQL 语句
  char sql[4096] = {0};
  sprintf(sql, "select * from image_table");
  // 5. 执行 sql 语句, 负责了客户端给服务器发送数据的过程
  int ret = mysql_query(mysql, sql);
  if (ret != 0) {
    printf("执行 sql 失败! %s\n", mysql_error(mysql));
    return 1;
  }

  // 6. 获取结果集合
  MYSQL_RES* result = mysql_store_result(mysql);
  int rows = mysql_num_rows(result);
  int cols = mysql_num_fields(result);
  for (int i = 0; i < rows; ++i) {
    MYSQL_ROW row = mysql_fetch_row(result);
    for (int j = 0; j < cols; ++j) {
      printf("%s\t", row[j]);
    }
    printf("\n");
  }
  // 7. 释放结果集合
  mysql_free_result(result);

  // 8. 关闭句柄
  mysql_close(mysql);
  return 0;
}

