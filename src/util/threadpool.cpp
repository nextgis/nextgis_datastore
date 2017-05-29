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
    m_maxThreadCount(1),
    m_threadCount(0)
{
    CPLReleaseMutex(m_dataMutex);
    CPLReleaseMutex(m_threadMutex);
}

ThreadPool::~ThreadPool()
{
    clearThreadData();
}

void ThreadPool::init(unsigned char numThreads, poolThreadFunction function)
{
    m_maxThreadCount = numThreads;
    m_function = function;
}

void ThreadPool::addThreadData(ThreadData *data)
{
    CPLAcquireMutex(m_dataMutex, 1000.0);
    m_threadData.push_back(data);
    CPLReleaseMutex(m_dataMutex);

    newWorker();
}

void ThreadPool::clearThreadData()
{
    CPLAcquireMutex(m_dataMutex, 1000.0);
    for(ThreadData* data : m_threadData) {
        if(data->isOwn()) {
            delete data;
        }
    }
    m_threadData.clear();
    CPLReleaseMutex(m_dataMutex);
}

void ThreadPool::waitComplete() const
{
    bool complete = false;
    while(true) {

        CPLAcquireMutex(m_threadMutex, 250.0);
        complete = m_threadCount <= 0;
        CPLReleaseMutex(m_threadMutex);

        if(complete) {
            return;
        }

        CPLSleep(0.5);
    }
}

bool ThreadPool::process()
{
    CPLAcquireMutex(m_dataMutex, 1000.0);
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
    else if(data->tries() > 3) {
        if(data->isOwn()) {
            delete data;
        }
    }
    else {
        CPLAcquireMutex(m_dataMutex, 1000.0);
        data->increaseTries();
        m_threadData.push_back(data);
        CPLReleaseMutex(m_dataMutex);
    }

    return true;
}

void ThreadPool::finished()
{
    CPLAcquireMutex(m_threadMutex, 250.0);
    m_threadCount--;
    CPLReleaseMutex(m_threadMutex);
}

void ThreadPool::newWorker()
{
    CPLAcquireMutex(m_threadMutex, 250.0);
    if(m_threadCount == m_maxThreadCount) {
        CPLReleaseMutex(m_threadMutex);
        return;
    }
    CPLCreateThread(threadFunction, this);
    m_threadCount++;
    CPLReleaseMutex(m_threadMutex);
}

void ThreadPool::threadFunction(void *threadData)
{
    ThreadPool *pool = static_cast<ThreadPool*>(threadData);
    while(pool->process()) {

    }
    pool->finished();
}


}
