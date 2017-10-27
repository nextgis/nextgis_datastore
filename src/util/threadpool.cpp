/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "threadpool.h"

#include "cpl_conv.h"

namespace ngs {


//------------------------------------------------------------------------------
// ThreadData
//------------------------------------------------------------------------------
ThreadData::ThreadData(bool own) : m_own(own),
    m_tries(0)
{

}

//------------------------------------------------------------------------------
// ThreadPool
//------------------------------------------------------------------------------
ThreadPool::ThreadPool() :
    m_dataMutex(CPLCreateMutex()),
    m_threadMutex(CPLCreateMutex()),
    m_function(nullptr),
    m_maxThreadCount(1),
    m_threadCount(0),
    m_tries(3),
    m_stopOnFirstFail(false),
    m_failed(false)
{
    CPLReleaseMutex(m_dataMutex);
    CPLReleaseMutex(m_threadMutex);
}

ThreadPool::~ThreadPool()
{
    clearThreadData();
}

void ThreadPool::init(unsigned char numThreads, poolThreadFunction function,
                      unsigned char tries, bool stopOnFirstFail)
{
    m_maxThreadCount = numThreads;
    m_function = function;
    m_tries = tries;
    m_stopOnFirstFail = stopOnFirstFail;
}

void ThreadPool::addThreadData(ThreadData *data)
{
    CPLAcquireMutex(m_dataMutex, 5.5);
    m_threadData.push_back(data);
    CPLReleaseMutex(m_dataMutex);

    newWorker();
}

void ThreadPool::clearThreadData()
{
    CPLMutexHolder holder(m_dataMutex, 5.5);
    for(ThreadData* data : m_threadData) {
        if(data->isOwn()) {
            delete data;
        }
    }
    m_threadData.clear();
}

void ThreadPool::waitComplete(const Progress &progress)
{
    bool complete = false;
    size_t currentDataCount = dataCount();
    while(true) {

        CPLAcquireMutex(m_threadMutex, 7.0);
        complete = m_threadCount <= 0;
        double completePercent = double(dataCount()) / currentDataCount;
        if(!progress.onProgress(COD_IN_PROCESS, 1.0 - completePercent,
                                _("Working..."))) {
            clearThreadData();
        }
        CPLReleaseMutex(m_threadMutex);

        if(complete) {
            return;
        }

        CPLSleep(0.5);
    }
}

bool ThreadPool::process()
{
    CPLAcquireMutex(m_dataMutex, 9.5);
    if(m_threadData.empty()) {
        CPLReleaseMutex(m_dataMutex);
        return false;
    }
    ThreadData* data = m_threadData.front();
    m_threadData.pop_front();
    CPLReleaseMutex(m_dataMutex);

    if(m_function(data)) {
        if(data->isOwn()) {
            delete data;
        }
    }
    else if(data->tries() > m_tries) {
        if(data->isOwn()) {
            delete data;
        }
        if(m_stopOnFirstFail) {
            clearThreadData();
            m_failed = true;
            return false;
        }
    }
    else {
        data->increaseTries();
        CPLAcquireMutex(m_dataMutex, 9.5);
        m_threadData.push_back(data);
        CPLReleaseMutex(m_dataMutex);
    }

    return true;
}

void ThreadPool::finished()
{
    CPLAcquireMutex(m_threadMutex, 9.5);
    m_threadCount--;
    CPLReleaseMutex(m_threadMutex);

    CPLAcquireMutex(m_dataMutex, 9.5);
    if(m_threadData.empty()) {
        CPLReleaseMutex(m_dataMutex);
        return;
    }
    CPLReleaseMutex(m_dataMutex);

    newWorker();
}

void ThreadPool::newWorker()
{
    CPLMutexHolder holder(m_threadMutex, 9.5);
    if(m_threadCount == m_maxThreadCount) {
        return;
    }
    CPLCreateThread(threadFunction, this);
    m_threadCount++;
}

void ThreadPool::threadFunction(void *threadData)
{
    ThreadPool *pool = static_cast<ThreadPool*>(threadData);
    while(pool->process()) {
        //CPLSleep(0.125);
    }
    pool->finished();
}


}
