#include <fstream>
#include <signal.h>
#include <sys/stat.h>
#include "httplib.h"
#include "db.hpp"

class FileUtil {
public:
  static bool Write(const std::string& file_name,
      const std::string& content) {
    std::ofstream file(file_name.c_str());
    if (!file.is_open()) {
      return false;
    }
    file.write(content.c_str(), content.length());
    file.close();
    return true;
  }

  static bool Read(const std::string& file_name, 
      std::string* content) {
    std::ifstream file(file_name.c_str());
    if (!file.is_open()) {
      return false;
    }
    struct stat st;
    stat(file_name.c_str(), &st);
    content->resize(st.st_size);
    // 一口气把整个文件都读完
    // 需要先知道该文件的大小
    // char* 缓冲区长度
    // int 读取多长
    file.read((char*)content->c_str(), content->size());
    file.close();
    return true;
  }
};

MYSQL* mysql = NULL;

int main() {
  using namespace httplib;

  mysql = image_system::MySQLInit();
  image_system::ImageTable image_table(mysql);
  signal(SIGINT, [](int) {
      image_system::MySQLRelease(mysql);
      exit(0);
    });

  Server server;
  // 客户端请求 /hello 路径的时候, 执行一个特定的函数
  // 指定不同的路径对应到不同的函数上, 这个过程
  // 称为 "设置路由"
  // 服务器中有两个重要的概念:
  // 1. 请求(Request)
  // 2. 响应(Response)
  // [&image_table] 这是 lambda 的重要特性, 捕获变量
  // 本来 lambda 内部是不能直接访问 image_table 的.
  // 捕捉之后就可以访问了. 其中 & 的含义是相当于按引用捕获
  server.Post("/image", [&image_table](const Request& req, Response& resp) {
      Json::FastWriter writer;
      Json::Value resp_json;
      printf("上传图片\n");
      // 1. 对参数进行校验
			auto ret = req.has_file("upload");
      if (!ret) {
        printf("文件上传出错!\n");
        resp.status = 404;
        // 可以使用 json 格式组织一个返回结果
        resp_json["ok"] = false;
        resp_json["reason"] = "上传文件出错, 没有需要的upload 字段";
        resp.set_content(writer.write(resp_json), "application/json");
        return;
      }
      // 2. 根据文件名获取到文件数据 file 对象
			const auto& file = req.get_file_value("upload");
			// file.filename;
			// file.content_type;
      // 3. 把图片属性信息插入到数据库中
      Json::Value image;
      image["image_name"] = file.filename;
      image["size"] = (int)file.length;
      image["upload_time"] = "2018/08/29";  // TODO
      image["md5"] = "aaaaaaa"; // TODO
      image["type"] = file.content_type;
      image["path"] = "./data/" + file.filename;
      ret = image_table.Insert(image);
      if (!ret) {
        printf("image_table Insert failed!\n");
        resp_json["ok"] = false;
        resp_json["reason"] = "数据库插入失败!";
        resp.status = 500;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
      }
      
      // 4. 把图片保存到指定的磁盘目录中
			auto body = req.body.substr(file.offset, file.length);
      FileUtil::Write(image["path"].asString(), body);

      // 5. 构造一个响应数据通知客户端上传成功
      resp_json["ok"] = true;
      resp.status = 200;
      resp.set_content(writer.write(resp_json), "application/json");
      return;
    });

  server.Get("/image", [&image_table](const Request& req,
        Response& resp) {
      (void) req; // 没有任何实际的效果
      printf("获取所有图片信息\n");
      Json::Value resp_json;
      Json::FastWriter writer;
      // 1. 调用数据库接口来获取数据
      bool ret = image_table.SelectAll(&resp_json);
      if (!ret) {
        printf("查询数据库失败!\n");
        resp_json["ok"] = false;
        resp_json["reason"] = "查询数据库失败!";
        resp.status = 500;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
      }
      // 2. 构造响应结果返回给客户端
      resp.status = 200;
      resp.set_content(writer.write(resp_json), "application/json");
    });

  // 1. 正则表达式
  // 2. 原始字符串(raw string)
  server.Get(R"(/image/(\d+))", [&image_table](const Request& req, Response& resp) {
      Json::FastWriter writer;
      Json::Value resp_json;
      // 1. 先获取到图片 id
      int image_id = std::stoi(req.matches[1]);
      printf("获取 id 为 %d 的图片信息!\n", image_id);
      // 2. 再根据图片 id 查询数据库
      bool ret = image_table.SelectOne(image_id, &resp_json);
      if (!ret) {
        printf("数据库查询出错!\n");
        resp_json["ok"] = false;
        resp_json["reason"] = "数据库查询出错";
        resp.status = 404;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
      }
      // 3. 把查询结果返回给客户端
      resp_json["ok"] = true;
      resp.set_content(writer.write(resp_json), "application/json");
      return;
    });

  server.Get(R"(/show/(\d+))", [&image_table](const Request& req,
        Response& resp) {
      Json::FastWriter writer;
      Json::Value resp_json;
      // 1. 根据图片 id 去数据库中查到对应的目录
      int image_id = std::stoi(req.matches[1]);
      printf("获取 id 为 %d 的图片内容!\n", image_id);
      Json::Value image;
      bool ret = image_table.SelectOne(image_id, &image);
      if (!ret) {
        printf("读取数据库失败!\n");
        resp_json["ok"] = false;
        resp_json["reason"] = "数据库查询出错";
        resp.status = 404;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
      }
      // 2. 根据目录找到文件内容, 读取文件内容
      std::string image_body;
      printf("%s\n", image["path"].asCString());
      ret = FileUtil::Read(image["path"].asString(), &image_body);
      if (!ret) {
        printf("读取图片文件失败!\n");
        resp_json["ok"] = false;
        resp_json["reason"] = "读取图片文件失败";
        resp.status = 500;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
      }
      // 3. 把文件内容构造成一个响应
      resp.status = 200;
      // 不同的图片, 设置的 content type 是不一样的. 
      // 如果是 png 应该设为 image/png
      // 如果是 jpg 应该设为 image/jpg
      resp.set_content(image_body, image["type"].asCString());
    });

  server.Delete(R"(/image/(\d+))", [&image_table](const Request& req, Response& resp) {
      Json::FastWriter writer;
      Json::Value resp_json;

      // 1. 根据图片 id 去数据库中查到对应的目录
      int image_id = std::stoi(req.matches[1]);
      printf("删除 id 为 %d 的图片!\n", image_id);
      // 2. 查找到对应文件的路径
      Json::Value image;
      bool ret = image_table.SelectOne(image_id, &image);
      if (!ret) {
        printf("删除图片文件失败!\n");
        resp_json["ok"] = false;
        resp_json["reason"] = "删除图片文件失败";
        resp.status = 404;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
      }
      // 3. 调用数据库操作进行删除
      ret = image_table.Delete(image_id);
      if (!ret) {
        printf("删除图片文件失败!\n");
        resp_json["ok"] = false;
        resp_json["reason"] = "删除图片文件失败";
        resp.status = 404;
        resp.set_content(writer.write(resp_json), "application/json");
        return;
      }
      // 4. 删除磁盘上的文件
      // C++ 标准库中, 没有删除文件的方法
      // C++ 17 标准里是有的. 
      // 此处只能使用操作系统提供的函数了
      unlink(image["path"].asCString());
      
      // 5. 构造响应
      resp_json["ok"] = true;
      resp.status = 200;
      resp.set_content(writer.write(resp_json), "application/json");
    });

  server.set_base_dir("./wwwroot");
  server.listen("0.0.0.0", 9094);

  return 0;
}
