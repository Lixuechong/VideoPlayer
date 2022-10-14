#ifndef NE_PLAYER_MACRO_H
#define NE_PLAYER_MACRO_H

#define THREAD_MAIN 1 // 主线程
#define THREAD_CHILD 2 // 子线程

#endif //NE_PLAYER_MACRO_H

// 宏函数(用来释放资源) (原理：在预编译阶段会把代码copy到项目使用的地方。)
#define DELETE(object) if (object) {delete object; object = nullptr;}
