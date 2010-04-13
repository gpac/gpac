/*****************************************************************
|
|   Neptune - Queue
|
| Copyright (c) 2002-2008, Axiomatic Systems, LLC.
| All rights reserved.
|
| Redistribution and use in source and binary forms, with or without
| modification, are permitted provided that the following conditions are met:
|     * Redistributions of source code must retain the above copyright
|       notice, this list of conditions and the following disclaimer.
|     * Redistributions in binary form must reproduce the above copyright
|       notice, this list of conditions and the following disclaimer in the
|       documentation and/or other materials provided with the distribution.
|     * Neither the name of Axiomatic Systems nor the
|       names of its contributors may be used to endorse or promote products
|       derived from this software without specific prior written permission.
|
| THIS SOFTWARE IS PROVIDED BY AXIOMATIC SYSTEMS ''AS IS'' AND ANY
| EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
| WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
| DISCLAIMED. IN NO EVENT SHALL AXIOMATIC SYSTEMS BE LIABLE FOR ANY
| DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
| (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
| LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
| ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
| (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
| SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
|
 ****************************************************************/

#ifndef _NPT_QUEUE_H_
#define _NPT_QUEUE_H_

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "NptTypes.h"
#include "NptConstants.h"

/*----------------------------------------------------------------------
|   NPT_QueueItem
+---------------------------------------------------------------------*/
class NPT_QueueItem;

/*----------------------------------------------------------------------
|   NPT_GenericQueue
+---------------------------------------------------------------------*/
class NPT_GenericQueue
{
 public:
    // class methods
    static NPT_GenericQueue* CreateInstance(NPT_Cardinal max_items = 0);

    // methods
    virtual           ~NPT_GenericQueue() {}
    virtual NPT_Result Push(NPT_QueueItem* item, 
                           NPT_Timeout     timeout = NPT_TIMEOUT_INFINITE) = 0; 
    virtual NPT_Result Pop(NPT_QueueItem*& item, 
                           NPT_Timeout     timeout = NPT_TIMEOUT_INFINITE) = 0;
    virtual NPT_Result Peek(NPT_QueueItem*& item, 
                           NPT_Timeout     timeout = NPT_TIMEOUT_INFINITE) = 0;
 protected:
    // methods
    NPT_GenericQueue() {}
};

/*----------------------------------------------------------------------
|   NPT_Queue
+---------------------------------------------------------------------*/
template <class T>
class NPT_Queue
{
 public:
    // methods
    NPT_Queue(NPT_Cardinal max_items = 0) :
        m_Delegate(NPT_GenericQueue::CreateInstance(max_items)) {}
    virtual ~NPT_Queue<T>() { delete m_Delegate; }
    virtual NPT_Result Push(T* item, NPT_Timeout timeout = NPT_TIMEOUT_INFINITE) {
        return m_Delegate->Push(reinterpret_cast<NPT_QueueItem*>(item), timeout);
    }
    virtual NPT_Result Pop(T*& item, NPT_Timeout timeout = NPT_TIMEOUT_INFINITE) {
        return m_Delegate->Pop(reinterpret_cast<NPT_QueueItem*&>(item), 
                               timeout);
    }
    virtual NPT_Result Peek(T*& item, NPT_Timeout timeout = NPT_TIMEOUT_INFINITE) {
        return m_Delegate->Peek(reinterpret_cast<NPT_QueueItem*&>(item), 
                                timeout);
    }

 protected:
    // members
    NPT_GenericQueue* m_Delegate;
};

#endif // _NPT_QUEUE_H_
