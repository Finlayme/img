create database if not exists image_system2;
use image_system2;

drop table if exists image_table;
create table image_table(
  image_id int not null primary key auto_increment,
  image_name varchar(256),
  size int,
  upload_time varchar(50),
  md5 varchar(128),
  type varchar(128),
  path varchar(1024)
);

insert into image_table values(null, 'test.png', 1024, '2019/08/26', 'aaaabbbbbccccc', 'png', 'data/test.png')
