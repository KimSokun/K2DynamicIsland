#pragma once

#include <windows.h>
#include <unknwn.h>
#include <functional>
#include <utility>
#include "WebView2.h"

namespace K2 {

class EnvironmentCompletedHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    using CallbackType = std::function<HRESULT(HRESULT, ICoreWebView2Environment*)>;

    explicit EnvironmentCompletedHandler(CallbackType callback)
        : m_callback(std::move(callback)), m_refCount(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* env) override {
        if (m_callback) return m_callback(errorCode, env);
        return S_OK;
    }

private:
    CallbackType m_callback;
    LONG m_refCount;
};

class ControllerCompletedHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
public:
    using CallbackType = std::function<HRESULT(HRESULT, ICoreWebView2Controller*)>;

    explicit ControllerCompletedHandler(CallbackType callback)
        : m_callback(std::move(callback)), m_refCount(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* ctrl) override {
        if (m_callback) return m_callback(errorCode, ctrl);
        return S_OK;
    }

private:
    CallbackType m_callback;
    LONG m_refCount;
};

class WebMessageReceivedHandler : public ICoreWebView2WebMessageReceivedEventHandler {
public:
    using CallbackType = std::function<HRESULT(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*)>;

    explicit WebMessageReceivedHandler(CallbackType callback)
        : m_callback(std::move(callback)), m_refCount(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2WebMessageReceivedEventHandler) {
            *ppv = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        if (m_callback) return m_callback(sender, args);
        return S_OK;
    }

private:
    CallbackType m_callback;
    LONG m_refCount;
};

class NavigationStartingHandler : public ICoreWebView2NavigationStartingEventHandler {
public:
    using CallbackType = std::function<HRESULT(ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*)>;

    explicit NavigationStartingHandler(CallbackType callback)
        : m_callback(std::move(callback)), m_refCount(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2NavigationStartingEventHandler) {
            *ppv = static_cast<ICoreWebView2NavigationStartingEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) override {
        if (m_callback) return m_callback(sender, args);
        return S_OK;
    }

private:
    CallbackType m_callback;
    LONG m_refCount;
};

class NewWindowRequestedHandler : public ICoreWebView2NewWindowRequestedEventHandler {
public:
    using CallbackType = std::function<HRESULT(ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs*)>;

    explicit NewWindowRequestedHandler(CallbackType callback)
        : m_callback(std::move(callback)), m_refCount(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2NewWindowRequestedEventHandler) {
            *ppv = static_cast<ICoreWebView2NewWindowRequestedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) override {
        if (m_callback) return m_callback(sender, args);
        return S_OK;
    }

private:
    CallbackType m_callback;
    LONG m_refCount;
};

} // namespace K2
