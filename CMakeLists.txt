cmake_minimum_required(VERSION 2.9)
project(MYMUDUO)

# myduo最终编译为so动态库，动态库路径在根目录lib下面
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
# 设置调试信息，启动c++11
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11") #为什么这里的cmake都不行，就是没有自动拼凑代码

#定义参与编译源代码
aux_source_directory(. SRC.LIST)#这写的都是啥，为什么没有出来

#编译生成动态库mymuduo
add_library(MYMUDUO SHARED ${SRC.LIST})