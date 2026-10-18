#pragma once

#include <windows.h>
#include <oleidl.h>
#include <shellapi.h>
#include <functional>

namespace K2 {

class DropTarget : public IDropTarget {
public:
    using DropCallback = std::function<void(HDROP)>;
    using DragStateCallback = std::function<void(bool)>;

    DropTarget(DropCallback onDrop, DragStateCallback onDragState = nullptr)
        : m_refCount(1), m_onDrop(std::move(onDrop)), m_onDragState(std::move(onDragState)) {}

    virtual ~DropTarget() = default;

    // --- IUnknown ---
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) {
            delete this;
            return 0;
        }
        return count;
    }

    // --- IDropTarget ---
    STDMETHODIMP DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        if (!pdwEffect) return E_INVALIDARG;
        
        if (HasHDROP(pDataObj)) {
            *pdwEffect = DROPEFFECT_COPY;
            if (m_onDragState) m_onDragState(true);
        } else {
            *pdwEffect = DROPEFFECT_NONE;
        }
        return S_OK;
    }

    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        if (!pdwEffect) return E_INVALIDARG;
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }

    STDMETHODIMP DragLeave() override {
        if (m_onDragState) m_onDragState(false);
        return S_OK;
    }

    STDMETHODIMP Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        if (m_onDragState) m_onDragState(false);
        if (!pDataObj || !pdwEffect) return E_INVALIDARG;

        *pdwEffect = DROPEFFECT_COPY;

        FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medium = {};
        if (SUCCEEDED(pDataObj->GetData(&fmt, &medium))) {
            HDROP hDrop = static_cast<HDROP>(GlobalLock(medium.hGlobal));
            if (hDrop) {
                if (m_onDrop) {
                    m_onDrop(hDrop);
                }
                GlobalUnlock(medium.hGlobal);
            }
            ReleaseStgMedium(&medium);
        }
        return S_OK;
    }

private:
    static bool HasHDROP(IDataObject* pDataObj) {
        if (!pDataObj) return false;
        FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        return (pDataObj->QueryGetData(&fmt) == S_OK);
    }

    ULONG m_refCount;
    DropCallback m_onDrop;
    DragStateCallback m_onDragState;
};

} // namespace K2
