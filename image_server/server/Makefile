FLAG=-L /usr/lib64/mysql -lmysqlclient -ljsoncpp -g

all:image_server db_test

image_server:image_server.cc
	g++ $^ -o $@ -std=c++11 -lpthread $(FLAG)

db_test:db_test.cc db.hpp
	g++ db_test.cc -o $@  $(FLAG)

.PHONY:clean
clean:
	rm db_test image_server
