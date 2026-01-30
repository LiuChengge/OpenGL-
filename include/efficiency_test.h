// Centralized toggle for per-frame efficiency testing and helper printing macro.
// Modify DO_EFFECIENCY_TEST to 1 to enable detailed per-frame timing prints.
#ifndef EFFICIENCY_TEST_H
#define EFFICIENCY_TEST_H

// 默认关闭；用户可修改为1启用
#ifndef DO_EFFECIENCY_TEST
#define DO_EFFECIENCY_TEST 0
#endif

// 简单的打印宏，基于宏值展开或为空（避免在代码处反复写 #if/#endif）
#if DO_EFFECIENCY_TEST
#define EFF_PRINT(...) printf(__VA_ARGS__)
#else
#define EFF_PRINT(...) do {} while(0)
#endif

#endif // EFFICIENCY_TEST_H


