// Only the focused PS edit context is changed; never the global keyboard compartment.
#include <msctf.h>
#include <oleauto.h>
#include <olectl.h>

struct InputContextTsf : ITfThreadMgrEventSink,ITfCompartmentEventSink {
    ITfContext* context=nullptr;
    ITfThreadMgr* thread=nullptr;
    ITfCompartmentMgr* compartments=nullptr;
    ITfCompartment* keyboard=nullptr;
    VARIANT original{};
    TfClientId client=0;
    bool activated=false,changed=false;
    ITfSource *threadSource=nullptr,*keyboardSource=nullptr;
    DWORD threadCookie=TF_INVALID_COOKIE,keyboardCookie=TF_INVALID_COOKIE;
    volatile LONG references=1;
    void (*notify)()=nullptr;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** value) override {
        if(!value)return E_POINTER;
        *value=nullptr;
        if(id==IID_IUnknown || id==IID_ITfThreadMgrEventSink)*value=static_cast<ITfThreadMgrEventSink*>(this);
        else if(id==IID_ITfCompartmentEventSink)*value=static_cast<ITfCompartmentEventSink*>(this);
        else return E_NOINTERFACE;
        AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&references);}
    // The pinned DLL owns the static sink's anchor reference until PS exits.
    ULONG STDMETHODCALLTYPE Release() override{return InterlockedDecrement(&references);}
    HRESULT signal(){if(notify)notify();return S_OK;}
    HRESULT STDMETHODCALLTYPE OnInitDocumentMgr(ITfDocumentMgr*) override{return signal();}
    HRESULT STDMETHODCALLTYPE OnUninitDocumentMgr(ITfDocumentMgr*) override{return signal();}
    HRESULT STDMETHODCALLTYPE OnSetFocus(ITfDocumentMgr*,ITfDocumentMgr*) override{return signal();}
    HRESULT STDMETHODCALLTYPE OnPushContext(ITfContext*) override{return signal();}
    HRESULT STDMETHODCALLTYPE OnPopContext(ITfContext*) override{return signal();}
    HRESULT STDMETHODCALLTYPE OnChange(REFGUID id) override{return id==GUID_COMPARTMENT_KEYBOARD_DISABLED?signal():S_OK;}
    bool watching()const{return threadCookie!=TF_INVALID_COOKIE && keyboardCookie!=TF_INVALID_COOKIE;}
    static bool unwatch(ITfSource*& source,DWORD& cookie,bool invalidated=false) {
        if(!source)return true;
        if(cookie!=TF_INVALID_COOKIE) {
            const auto result=source->UnadviseSink(cookie);
            if(FAILED(result) && result!=CONNECT_E_NOCONNECTION &&
               !(invalidated && result==E_UNEXPECTED))return false;
        }
        source->Release();source=nullptr;cookie=TF_INVALID_COOKIE;return true;
    }
    static ITfContext* current(ITfThreadMgr** manager=nullptr) {
        const auto library=GetModuleHandleW(L"msctf.dll");
        HRESULT(WINAPI*get)(ITfThreadMgr**)=nullptr;
        const auto address=library?GetProcAddress(library,"TF_GetThreadMgr"):nullptr;
        static_assert(sizeof(get)==sizeof(address));memcpy(&get,&address,sizeof(get));
        ITfThreadMgr* thread=nullptr;ITfDocumentMgr* document=nullptr;ITfContext* context=nullptr;
        if(get && SUCCEEDED(get(&thread)) && thread) {
            if(SUCCEEDED(thread->GetFocus(&document)) && document){document->GetTop(&context);document->Release();}
            if(manager)*manager=thread;else thread->Release();
        }
        return context;
    }
    static bool available(){const auto context=current();if(context)context->Release();return context!=nullptr;}
    bool disabled()const {
        if(!keyboard)return true;
        VARIANT value{};const bool ok=SUCCEEDED(keyboard->GetValue(&value)) && value.vt==VT_I4 && value.lVal==1;VariantClear(&value);return ok;
    }
    bool currentDisabled()const {
        auto focused=current();
        if(!context){if(focused)focused->Release();return focused==nullptr;}
        IUnknown *expected=nullptr,*actual=nullptr;
        // Detaching IMM can temporarily remove TSF focus; keep the verified original disabled until focus returns or changes.
        const bool same=!focused || (SUCCEEDED(context->QueryInterface(IID_IUnknown,reinterpret_cast<void**>(&expected))) &&
            SUCCEEDED(focused->QueryInterface(IID_IUnknown,reinterpret_cast<void**>(&actual))) && expected==actual);
        if(expected)expected->Release();
        if(actual)actual->Release();
        if(focused)focused->Release();
        return same && disabled();
    }
    bool start() {
        context=current(&thread);
        if(!context){if(thread){thread->Release();thread=nullptr;}return true;}
        const auto queried=context->QueryInterface(IID_ITfCompartmentMgr,reinterpret_cast<void**>(&compartments));
        if(FAILED(queried) || FAILED(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED,&keyboard)) ||
           FAILED(keyboard->GetValue(&original)) || (original.vt!=VT_EMPTY && original.vt!=VT_I4) || FAILED(thread->Activate(&client)))return false;
        activated=true;VARIANT value{};value.vt=VT_I4;value.lVal=1;changed=true;
        if(FAILED(thread->QueryInterface(IID_ITfSource,reinterpret_cast<void**>(&threadSource))) ||
           FAILED(threadSource->AdviseSink(IID_ITfThreadMgrEventSink,static_cast<ITfThreadMgrEventSink*>(this),&threadCookie)) ||
           FAILED(keyboard->QueryInterface(IID_ITfSource,reinterpret_cast<void**>(&keyboardSource))) ||
           FAILED(keyboardSource->AdviseSink(IID_ITfCompartmentEventSink,static_cast<ITfCompartmentEventSink*>(this),&keyboardCookie)))return false;
        return SUCCEEDED(keyboard->SetValue(client,&value)) && disabled();
    }
    bool keyboardInvalidated() const {
        VARIANT value{};
        const bool invalidated=keyboard && keyboard->GetValue(&value)==E_UNEXPECTED;
        VariantClear(&value);
        return invalidated;
    }
    bool restoreValue() {
        // ClearCompartment invalidates existing interfaces, including the one
        // retained for notifications. Always verify through the saved context's
        // compartment manager, including retries after a failed readback.
        ITfCompartment* live=nullptr;
        if(FAILED(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED,&live)) || !live)return false;
        VARIANT value{};
        const auto read=live->GetValue(&value);
        const bool ours=SUCCEEDED(read) && value.vt==VT_I4 && value.lVal==1;
        const bool alreadyOriginal=ours && original.vt==VT_I4 && original.lVal==1;
        VariantClear(&value);
        // A compartment recreated by Photoshop or its IME owns its own value.
        const bool needsRestore=ours && !alreadyOriginal && !keyboardInvalidated();
        HRESULT result=S_OK;
        if(needsRestore) {
            result=original.vt==VT_EMPTY?compartments->ClearCompartment(client,GUID_COMPARTMENT_KEYBOARD_DISABLED):live->SetValue(client,&original);
        }
        live->Release();
        if(FAILED(read) || FAILED(result))return false;
        if(!needsRestore)return true;
        live=nullptr;
        if(FAILED(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED,&live)) || !live)return false;
        const auto verified=live->GetValue(&value);
        live->Release();
        const bool same=SUCCEEDED(verified) && value.vt==original.vt && (value.vt==VT_EMPTY || value.lVal==original.lVal);
        VariantClear(&value);
        return same;
    }
    bool restore() {
        if(changed && !restoreValue())return false;
        changed=false;
        const bool invalidated=keyboardSource && keyboardInvalidated();
        if(!unwatch(keyboardSource,keyboardCookie,invalidated) || !unwatch(threadSource,threadCookie))return false;
        if(activated){if(FAILED(thread->Deactivate()))return false;activated=false;}
        if(keyboard)keyboard->Release();
        if(compartments)compartments->Release();
        if(thread)thread->Release();
        if(context)context->Release();
        keyboard=nullptr;compartments=nullptr;thread=nullptr;context=nullptr;VariantClear(&original);client=0;return true;
    }
};
