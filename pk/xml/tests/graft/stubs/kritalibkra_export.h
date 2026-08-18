// R-25 Task 1 判据②试接占位：kritalibkra_export.h 是 CMake
// generate_export_header() 在真实构建时才产出的生成头（源树里不存在），
// 试接编译不跑那套 CMake 生成步骤，这里用一个空宏占位顶上——跟
// kritaglobal_export.h/kritaimage_export.h（R-07 Task 3 已有的同款占位，见
// graft_run_a.sh 用到的 stubs 目录）同一个理由、同一种处理方式，不是新发明。
#pragma once
#define KRITALIBKRA_EXPORT
#define KRITALIBKRA_TEST_EXPORT
