/*****************************************************************
|
|   Neptune - Automatic Cleaner
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

#ifndef _NPT_AUTOMATIC_CLEANER_H_
#define _NPT_AUTOMATIC_CLEANER_H_

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "NptList.h"

/*----------------------------------------------------------------------
|   NPT_AutomaticCleaner
+---------------------------------------------------------------------*/
class NPT_AutomaticCleaner
{
public:
    class Singleton {
    public:
        virtual ~Singleton() {}
    };
    
    // singleton management
    class Cleaner {
        static Cleaner AutomaticCleaner;
        ~Cleaner() {
            if (Instance) {
                delete Instance;
                Instance = NULL;
            }
        }
    };
    static NPT_AutomaticCleaner* GetInstance();
    static void Shutdown() {
        if (Instance) {
            delete Instance;
            Instance = NULL;
        }
	}
    
    // destructor
    ~NPT_AutomaticCleaner();
    
    // methods
    NPT_Result Register(Singleton* singleton);
    NPT_Result RegisterTlsContext(Singleton* singleton);
    NPT_Result RegisterHttpConnectionManager(Singleton* singleton);
    
private:
    // class members
    static NPT_AutomaticCleaner* Instance;
    
    // constructor
    NPT_AutomaticCleaner();
    
    // members
    NPT_List<Singleton*> m_Singletons;
    Singleton* m_TlsContext;
    Singleton* m_HttpConnectionManager;
};

#endif // _NPT_AUTOMATIC_CLEANER_H_
