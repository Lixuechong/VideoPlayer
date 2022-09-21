//
// Created by lixuechong on 2022/9/21.
//

#include "JNICallbackHelper.h"

JNICallbackHelper::JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject job) {
    this->vm = vm;
    this->env = env; // env也不可以跨线程调用
//    this->job = job; // job，不能跨函数，想要使用必须全局引用。

    this->job = env->NewGlobalRef(job); // 提升为全局引用。

    jclass clazz = env->GetObjectClass(job);
    jmd_prepared = env->GetMethodID(clazz, "_jni_prepared", "()V");
    jmd_error = env->GetMethodID(clazz, "_jni_error", "(Ljava/lang/String;)V");
}


JNICallbackHelper::~JNICallbackHelper() {
    this->vm = 0;
    env->DeleteGlobalRef(job);
    job = 0;
    env = 0;
}

void JNICallbackHelper::onPrepared(int thread_mode) {
    if (thread_mode == THREAD_MAIN) {
        this->env->CallVoidMethod(this->job, this->jmd_prepared);// 利用反射
    }
    if (thread_mode == THREAD_CHILD) {
        // 使用子线程的JniEnv，全新的env，调用java的方法。
        JNIEnv *env_child;
        this->vm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(this->job, this->jmd_prepared);// 利用反射
        this->vm->DetachCurrentThread();
    }
}

void JNICallbackHelper::onError(char *msg, int thread_mode) {
    if (thread_mode == THREAD_MAIN) {
        this->env->CallVoidMethod(this->job, this->jmd_error);// 利用反射
    }
    if (thread_mode == THREAD_CHILD) {
        // 使用子线程的JniEnv，全新的env，调用java的方法。
        JNIEnv *env_child;
        this->vm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(this->job, this->jmd_error, msg);// 利用反射
        this->vm->DetachCurrentThread();
    }
}
