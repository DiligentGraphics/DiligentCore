/*
 *  Copyright 2023 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

#include <string>
#include <android/native_activity.h>
#include <android/asset_manager.h>

namespace Diligent
{

class JNIMiniHelper
{
public:
    void Init(ANativeActivity* activity, std::string activity_class_name)
    {
        VERIFY(activity != nullptr && !activity_class_name.empty(), "Activity and class name can't be null");
        activity_            = activity;
        activity_class_name_ = std::move(activity_class_name);
    }

    static JNIMiniHelper& GetInstance()
    {
        static JNIMiniHelper helper;
        return helper;
    }

    static std::string GetExternalFilesDir(ANativeActivity* activity, std::string activity_class_name)
    {
        JNIMiniHelper Helper;
        Helper.Init(activity, activity_class_name);
        return Helper.GetExternalFilesDir();
    }

    std::string GetExternalFilesDir()
    {
        if (activity_ == nullptr)
        {
            LOG_ERROR_MESSAGE("JNIMiniHelper has not been initialized. Call init() to initialize the helper");
            return "";
        }

        std::string ExternalFilesPath;
        {
            std::lock_guard<std::mutex> guard{mutex_};

            JNIEnv* env          = nullptr;
            bool    DetachThread = AttachCurrentThread(env);
            if (jstring jstr_path = GetExternalFilesDirJString(env))
            {
                const char* path  = env->GetStringUTFChars(jstr_path, nullptr);
                ExternalFilesPath = std::string(path);
                env->ReleaseStringUTFChars(jstr_path, path);
                env->DeleteLocalRef(jstr_path);
            }
            if (DetachThread)
                DetachCurrentThread();
        }

        return ExternalFilesPath;
    }

    /*
     * Attach current thread
     * In Android, the thread doesn't have to be 'Detach' current thread
     * as application process is only killed and VM does not shut down
     */
    bool AttachCurrentThread(JNIEnv*& env)
    {
        env = nullptr;
        if (activity_->vm->GetEnv((void**)&env, JNI_VERSION_1_4) == JNI_OK)
            return false; // Already attached
        activity_->vm->AttachCurrentThread(&env, nullptr);
        pthread_key_create((int32_t*)activity_, DetachCurrentThreadDtor);
        return true;
    }

    /*
     * Unregister this thread from the VM
     */
    static void DetachCurrentThreadDtor(void* p)
    {
        LOG_INFO_MESSAGE("detached current thread");
        auto* activity = reinterpret_cast<ANativeActivity*>(p);
        activity->vm->DetachCurrentThread();
    }

private:
    JNIMiniHelper()
    {
    }

    ~JNIMiniHelper()
    {
    }

    // clang-format off
    JNIMiniHelper           (const JNIMiniHelper&) = delete;
    JNIMiniHelper& operator=(const JNIMiniHelper&) = delete;
    JNIMiniHelper           (JNIMiniHelper&&)      = delete;
    JNIMiniHelper& operator=(JNIMiniHelper&&)      = delete;
    // clang-format on

    jstring GetExternalFilesDirJString(JNIEnv* env)
    {
        if (activity_ == nullptr)
        {
            LOG_ERROR_MESSAGE("JNIHelper has not been initialized. Call init() to initialize the helper");
            return NULL;
        }

        jstring obj_Path = nullptr;
        // Invoking getExternalFilesDir() java API
        jclass    cls_Env  = env->FindClass(activity_class_name_.c_str());
        jmethodID mid      = env->GetMethodID(cls_Env, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
        jobject   obj_File = env->CallObjectMethod(activity_->clazz, mid, NULL);
        if (obj_File)
        {
            jclass    cls_File    = env->FindClass("java/io/File");
            jmethodID mid_getPath = env->GetMethodID(cls_File, "getPath", "()Ljava/lang/String;");
            obj_Path              = (jstring)env->CallObjectMethod(obj_File, mid_getPath);
            env->DeleteLocalRef(cls_File);
            env->DeleteLocalRef(obj_File);
        }
        env->DeleteLocalRef(cls_Env);
        return obj_Path;
    }

    void DetachCurrentThread()
    {
        activity_->vm->DetachCurrentThread();
    }

    ANativeActivity* activity_ = nullptr;
    std::string      activity_class_name_;

    // mutex for synchronization
    // This class uses singleton pattern and can be invoked from multiple threads,
    // each methods locks the mutex for a thread safety
    mutable std::mutex mutex_;
};

} // namespace Diligent
