/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "DiligentCore/Common/interface/ThreadPool.h"

void TestAsyncTask()
{
    IAsyncTask_Run((IAsyncTask*)NULL, 0);
    IAsyncTask_Cancel((IAsyncTask*)NULL);
    IAsyncTask_SetStatus((IAsyncTask*)NULL, ASYNC_TASK_STATUS_CANCELLED);
    ASYNC_TASK_STATUS Status = IAsyncTask_GetStatus((IAsyncTask*)NULL);
    (void)Status;
    IAsyncTask_SetPriority((IAsyncTask*)NULL, 1.f);
    float Priority = IAsyncTask_GetPriority((IAsyncTask*)NULL);
    (void)Priority;
    bool IsFinished = IAsyncTask_IsFinished((IAsyncTask*)NULL);
    (void)IsFinished;
    IAsyncTask_WaitForCompletion((IAsyncTask*)NULL);
    IAsyncTask_WaitUntilRunning((IAsyncTask*)NULL);
}

void TestThreadPool()
{
    IThreadPool_EnqueueTask((IThreadPool*)NULL, (IAsyncTask*)NULL, (IAsyncTask**)NULL, 0);
    IThreadPool_ReprioritizeTask((IThreadPool*)NULL, (IAsyncTask*)NULL);
    IThreadPool_ReprioritizeAllTasks((IThreadPool*)NULL);
    IThreadPool_RemoveTask((IThreadPool*)NULL, (IAsyncTask*)NULL);
    IThreadPool_WaitForAllTasks((IThreadPool*)NULL);
    Uint32 QueueSize = IThreadPool_GetQueueSize((IThreadPool*)NULL);
    (void)QueueSize;
    Uint32 TaskCount = IThreadPool_GetRunningTaskCount((IThreadPool*)NULL);
    (void)TaskCount;
    IThreadPool_StopThreads((IThreadPool*)NULL);
    bool MoreTasks = IThreadPool_ProcessTask((IThreadPool*)NULL, 1, true);
    (void)MoreTasks;
}
