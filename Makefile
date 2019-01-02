CXX=g++
ARNOLD_PATH=/home/cactus/Arnold-5.2.2.0-linux
CXXFLAGS=\
	-Wall\
	-std=c++17\
	-O3\
	-shared\
	-fPIC\
	-Wno-narrowing\
	-I${ARNOLD_PATH}/include\
	-ggdb3

LDFLAGS=-L${ARNOLD_PATH}/bin -lai 

.PHONY=all clean

all: dump pointcloud

pointcloud: Makefile src/pointcloud.cpp ${HEADERS}
	${CXX} ${CXXFLAGS} src/pointcloud.cpp -o bin/pointcloud.so ${LDFLAGS}

dump: Makefile src/dump.cpp ${HEADERS}
	${CXX} ${CXXFLAGS} src/dump.cpp -o bin/dump.so ${LDFLAGS}

clean:
	rm -f dump pointcloud
