# `test1` 作为顶层工程的一个子项目，只负责定义本目录自己的目标。
# 语言标准、输出目录等工程级配置由上一级 CMakeLists.txt 统一管理。

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/test1)


# ============================================
# 1. 第三方依赖（NtyCo 协程库）
# ============================================

# 检查NtyCo目录下是否有CMakeLists.txt
# 如果有，将其作为子项目加入构建（这样NtyCo会先被构建）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/NtyCo/CMakeLists.txt")
    add_subdirectory(NtyCo)
endif()
# 注意：NtyCo使用Makefile构建，没有CMakeLists.txt，所以这个if不会执行
# 但我们保留此逻辑以备将来迁移


# ============================================
# 2. 查找系统库和依赖
# ============================================

# 查找线程库（自动处理pthread/win32线程的差异）
# 生成 Threads::Threads 导入目标，推荐用这个而不是直接写-pthread
find_package(Threads REQUIRED)

# 包含CheckIncludeFile模块（用于检测头文件是否存在）
include(CheckIncludeFile)

# 检测系统是否支持io_uring（Linux 5.1+新异步IO接口）
# 检查结果存入变量 HAVE_IO_URING_H（存在为1，不存在为空）
check_include_file(linux/io_uring.h HAVE_IO_URING_H)


# ============================================
# 3. 编译选项设置
# ============================================

# 定义通用编译选项变量（供多个目标复用）
# -Wall：开启所有常见警告
# -Wextra：开启额外警告（如符号比较、缺失字段初始化等）
# -O2：开启优化级别2（平衡编译速度和运行性能）
set(COMMON_COMPILE_FLAGS -Wall -Wextra -O2)

# ============================================
# 4. 源文件定义
# ============================================

# 定义kvstore主程序的所有源文件列表
# 包含：主程序、网络模型、存储引擎实现
set(KVSTORE_SOURCES
    kvstore.c        # 主程序入口，命令解析
    reactor.c        # Reactor事件驱动模型实现
    ntyco.c          # 协程封装层
    proactor.c       # Proactor异步模型实现（使用io_uring）
    kvs_array.c      # 基于数组的存储引擎
    kvs_rbtree.c     # 基于红黑树的存储引擎
    kvs_hash.c       # 基于哈希表的存储引擎
)

# 定义测试程序的源文件（独立的简单测试工具）
set(TESTCASE_SOURCES
    testcase.c       # 测试用例程序
)


# ============================================
# 5. 创建可执行文件目标
# ============================================

# 创建kvstore主程序目标（从KVSTORE_SOURCES源文件构建）
add_executable(kvstore ${KVSTORE_SOURCES})

# 创建testcase测试程序目标
add_executable(testcase ${TESTCASE_SOURCES})

# 把本子项目需要的头文件目录绑定到具体目标上，避免污染其他子项目
target_include_directories(kvstore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/NtyCo/core
)

# 将告警和优化选项绑定到当前目标，而不是整个工程
target_compile_options(kvstore PRIVATE ${COMMON_COMPILE_FLAGS})
target_compile_options(testcase PRIVATE ${COMMON_COMPILE_FLAGS})


# ============================================
# 6. 链接库配置（kvstore主程序）
# ============================================

# 为kvstore链接所需的库
target_link_libraries(kvstore 
    # 直接指定NtyCo静态库的完整路径（避免复制问题）
    # 使用${CMAKE_CURRENT_SOURCE_DIR}确保是源码目录的绝对路径
    ${CMAKE_CURRENT_SOURCE_DIR}/NtyCo/libntyco.a
    
    # 线程库（CMake自动处理平台差异，比直接写-pthread更规范）
    Threads::Threads
    
    # 动态链接库（dlopen/dlsym等，CMAKE_DL_LIBS自动判断是否需要-ldl）
    ${CMAKE_DL_LIBS}
)

# 如果系统支持io_uring（头文件存在），则额外链接liburing库
if(HAVE_IO_URING_H)
    target_link_libraries(kvstore uring)
endif()


# ============================================
# 7. 预处理器宏定义
# ============================================

# 为kvstore定义编译时宏（控制哪些存储引擎被启用）
target_compile_definitions(kvstore PRIVATE
    ENABLE_ARRAY=1   # 启用数组存储引擎
    ENABLE_RBTREE=1  # 启用红黑树存储引擎
    ENABLE_HASH=1    # 启用哈希表存储引擎
    ENABLE_KVSTORE=1 # 启用KVStore主功能
)


# ============================================
# 8. 测试程序配置
# ============================================

# 为testcase链接线程库和动态链接库（不需要NtyCo协程库）
target_link_libraries(testcase 
    Threads::Threads 
    ${CMAKE_DL_LIBS}
)


# ============================================
# 9. 安装规则
# ============================================

# 定义安装规则：
# 执行 `cmake --install build` 或 `make install` 后，
# 会把这两个可执行文件复制到安装前缀下的 bin 目录
install(TARGETS kvstore testcase
    RUNTIME DESTINATION bin
)
