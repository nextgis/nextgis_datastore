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
ThreadData::ThreadData(bool own) :
    m_own(own),
    m_tries(0)
{

}

//------------------------------------------------------------------------------
// ThreadPool
//------------------------------------------------------------------------------
ThreadPool::ThreadPool() :
    m_function(nullptr),
    m_maxThreadCount(1),
    m_threadCount(0),
    m_tries(3),
    m_stopOnFirstFail(false),
    m_failed(false)
{
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
    m_dataMutex.acquire(15.5);
    m_threadData.push_back(data);
    m_dataMutex.release();

    newWorker();
}

void ThreadPool::clearThreadData()
{
    MutexHolder holder(m_dataMutex, 25.5);
    if(m_threadData.empty()) {
        return;
    }

    for(ThreadData *data : m_threadData) {
        if(data && data->isOwn()) {
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

        m_threadMutex.acquire(7.0);
        complete = m_threadCount <= 0;
        double completePercent = double(dataCount()) / currentDataCount;
        if(!progress.onProgress(COD_IN_PROCESS, 1.0 - completePercent,
                                _("Working..."))) {
            clearThreadData();
        }
        m_threadMutex.release();

        if(complete) {
            return;
        }

        CPLSleep(0.55);
    }
}

bool ThreadPool::process()
{
    m_dataMutex.acquire(19.5);
    if(m_threadData.empty()) {
        m_dataMutex.release();
        return false;
    }
    ThreadData* data = m_threadData.front();
    m_threadData.pop_front();
    m_dataMutex.release();
    if(nullptr == data) {
        return false; // Should never happened.
    }

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
        MutexHolder holder(m_dataMutex, 19.5);
        m_threadData.push_back(data);
    }

    return true;
}

void ThreadPool::finished()
{
    m_threadMutex.acquire(19.5);
    m_threadCount--;
    m_threadMutex.release();

    MutexHolder holder(m_dataMutex, 19.5);
    if(m_threadData.empty()) {
        return;
    }

    newWorker();
}

void ThreadPool::newWorker()
{
    MutexHolder holder(m_threadMutex, 19.5);
    if(m_threadCount == m_maxThreadCount) {
        return;
    }
    CPLCreateThread(threadFunction, this);
    m_threadCount++;
}

void ThreadPool::threadFunction(void *threadData)
{
    ThreadPool *pool = static_cast<ThreadPool*>(threadData);
    if(nullptr != pool) {
        while(pool->process()) {
            //CPLSleep(0.125);
        }
        pool->finished();
    }
}


}
