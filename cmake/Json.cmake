include(FetchContent)
option(XDB_FETCH_DEPENDENCIES "下载缺失的依赖（固定版本及 SHA256）" ON)

# 本地源码覆盖不执行下载；关闭下载时仍允许使用调用方指定的现有源码。
find_package(nlohmann_json 3.12.0 QUIET CONFIG)
if(NOT TARGET nlohmann_json::nlohmann_json)
    if(NOT XDB_FETCH_DEPENDENCIES AND NOT FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON)
        message(FATAL_ERROR "需要 nlohmann_json >= 3.12.0；请安装或启用 XDB_FETCH_DEPENDENCIES")
    endif()
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(nlohmann_json
        URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.12.0
        URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187)
    FetchContent_MakeAvailable(nlohmann_json)
endif()
