#cmake 最小版本号
cmake_minimum_required(VERSION 3.1)
#项目名
project(udpfammilytest)

#添加编译时的环境变量，如gcc编译时需要添加的参数
SET (CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb -pthread")

# set the C++ standard to C++ 11
set(CMAKE_CXX_STANDARD 11)
#添加子目录，子目录中含有第三方库,需明确第三方库所生成的库名用于链接
add_subdirectory(lib/kcp)
add_subdirectory(lib/yaml-cpp)

include_directories(lib/kcp
                    lib/yaml-cpp)

#生成可执行文件
add_executable(udp_tans config.h config.cpp dataprocess.h dataprocess.cpp datatrans.cpp datatrans.h  util.h test.cpp test.h)
#将可执行文件链接到第三方库
target_link_libraries(udp_tans 
                        yaml-cpp 
                        kcp    )
                        #添加第三方库的include目录
target_include_directories(udp_tans
PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/kcp/
    ${PROJECT_SOURCE_DIR}/lib/yaml-cpp/include
)
