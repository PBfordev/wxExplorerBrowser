////////////////////////////////////////////////////////////////////////////////////
//
//  Name:        wxExplorerBrowser.cpp
//  Purpose:     Implements wxExplorerBrowser, a class for hosting
//               IExplorerBrowser in wxWidgets applications
//  Author:      PB
//  Copyright:   (c) 2018 PB <pbfordev@gmail.com>
//  Licence:     wxWindows licence
//
////////////////////////////////////////////////////////////////////////////////////

#include "wxExplorerBrowser.h"

// @TODO
// no items in Control Panel
// select items

#if  !defined(__WXMSW__) || !wxUSE_OLE || !wxUSE_DYNLIB_CLASS
    #error wxExplorerBrowser requires wxWidgets to be built for MS Windows with support for OLE and DLLs
#endif

#if !wxCHECK_VERSION(3, 1, 0)
    #error wxExplorerBrowser requires wxWidgets version 3.1 or higher
#endif

#include <wx/filename.h>
#include <wx/dcclient.h>
#include <wx/dynlib.h>

#include <wx/msw/private.h>
#include <wx/msw/private/comptr.h>
#include <wx/msw/wrapshl.h>
#include <wx/msw/winundef.h>

#define STRICT_TYPED_ITEMIDS
#include <shlwapi.h>
#include <shobjidl.h>
#include <shldisp.h>

#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0600)
    #error wxExplorerBrowser requires Windows SDK targetting Windows Vista or newer
#endif


// The code in this file took many things from the article (and comments on it)
// at https://www.codeproject.com/Articles/17809/Host-Windows-Explorer-in-your-applications-using-t
// and from the ExplorerBrowserHost sample by chrisg originally published on ShellRevealed.com

// For some reason ICommDlgBrowser::OnStateChange is called twice with CDBOSC_SELCHANGE
// for the same item.
// If WX_EXPLORER_BROWSER_PREVENT_DOUBLED_CHANGESEL_EVENTS is defined,
// the workaround code will be compiled-in which will try preventing that.
#define WX_EXPLORER_BROWSER_PREVENT_DOUBLED_CHANGESEL_EVENTS 1

namespace {

/***************************************************************************

    class wxExplorerBrowserImplHelper
    ---------------------------------
    implements several COM interfaces,
    does the actual item filtering and contains
    utility functions such as converting
    pidl/IShellItem to ExplorerBrowserItem

*****************************************************************************/

class wxExplorerBrowserImplHelper :
    public IServiceProvider,
    public ICommDlgBrowser3,
    public IExplorerBrowserEvents,
    public IFolderFilter,
    public IExplorerPaneVisibility
{
public:
    wxExplorerBrowserImplHelper(wxWindow* host, IExplorerBrowser* explorerBrowser)
        : m_host{host}, m_explorerBrowser{explorerBrowser} {}

    // IUnknown methods implemented from scratch.
    // For some reason the code kept crashing when using COM interface
    // implementation helper macros from <wx/msw/ole/comimpl.h>
    // and IUnknown_SetSite()
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;

    // IServiceProvider method
    STDMETHODIMP QueryService(REFGUID guidService, REFIID riid, void** ppv) override;

    // ICommDlgBrowser methods
    STDMETHODIMP OnDefaultCommand(IShellView* ppshv) override;
    STDMETHODIMP OnStateChange(IShellView* ppshv, ULONG uChange) override;
    STDMETHODIMP IncludeObject(IShellView* ppshv, PCUITEMID_CHILD pidl) override;

    // ICommDlgBrowser2 methods
    STDMETHODIMP Notify(IShellView* ppshv, DWORD dwNotifyType) override;
    STDMETHODIMP GetDefaultMenuText(IShellView* ppshv, LPWSTR pszText, int cchMax) override;
    STDMETHODIMP GetViewFlags(DWORD* pdwFlags) override;

    // ICommDlgBrowser3 methods
    STDMETHODIMP GetCurrentFilter(LPWSTR pszFileSpec, int cchFileSpec) override;
    STDMETHODIMP OnColumnClicked(IShellView*ppshv, int iColumn) override;
    STDMETHODIMP OnPreViewCreated(IShellView* ppshv) override;

    // IExplorerBrowserEvents methods
    STDMETHODIMP OnNavigationPending(PCIDLIST_ABSOLUTE pidlFolder) override;
    STDMETHODIMP OnViewCreated(IShellView* psv) override;
    STDMETHODIMP OnNavigationComplete(PCIDLIST_ABSOLUTE pidlFolder) override;
    STDMETHODIMP OnNavigationFailed(PCIDLIST_ABSOLUTE pidlFolder) override;

    // IFolderFilter methods
    STDMETHODIMP GetEnumFlags(IShellFolder*psf, PCIDLIST_ABSOLUTE pidlFolder, HWND* phwnd, DWORD* pgrfFlags) override;
    STDMETHODIMP ShouldShow(IShellFolder* psf, PCIDLIST_ABSOLUTE pidlFolder, PCUITEMID_CHILD pidlItem) override;

    // IExplorerPaneVisibility method
    STDMETHODIMP GetPaneState(REFEXPLORERPANE ep, EXPLORERPANESTATE *peps) override;

    // Names of all helper methods, i.e., those not implementing a COM interface,
    // start with an underscore

    bool _SetFilter(const wxArrayString& fileMasks, wxUint32 itemTypes);
    bool _RemoveFilter();

    bool _SetPaneSettings(const wxExplorerBrowser::PaneSettings& settings);

    static wxExplorerBrowserItem::Type _SFGAO2wxExplorerBrowserItemType(SFGAOF attr);
    static bool _IShellItem2wxExplorerBrowserItem(IShellItem* item, wxExplorerBrowserItem& ebi);
    static bool _PPIDL2wxExplorerBrowserItem(PCIDLIST_ABSOLUTE pidl, wxExplorerBrowserItem& ebi);

private:
    LONG              m_refCount {1};
    wxWindow*         m_host {nullptr};
    IExplorerBrowser* m_explorerBrowser {nullptr};

    wxArrayString     m_filterMasks; // list of filter masks, such as *.JPG
    wxUint32          m_filterTypes {0}; // flags for item types, such as File, Directory

    wxExplorerBrowser::PaneSettings m_paneSettings;

    bool _SendNotifyEvent(wxEventType command, PCIDLIST_ABSOLUTE list) const;
    bool _SendNotifyEvent(wxEventType command, const wxExplorerBrowserItem& ebi) const;

    bool _GetSelectedItem(wxExplorerBrowserItem& ebi);

#ifdef WX_EXPLORER_BROWSER_PREVENT_DOUBLED_CHANGESEL_EVENTS
    class ChangeSelEventData
    {
    public:
        ChangeSelEventData() {}

        void SetItem(const wxExplorerBrowserItem& item)
        {
            m_item = item;
        }

        bool IsDoubled(const ChangeSelEventData& previous) const
        {
            // If the difference between the two events is <=
            // the doubledTime and the item contains the same data,
            // then the event is considered a doubled event.
            static const ULONGLONG doubledTime = 50; // in milliseconds

            return
                m_timeCreated - previous.m_timeCreated <= doubledTime
                && m_item.GetSFGAO() == previous.m_item.GetSFGAO()
                && m_item.GetPath() == previous.m_item.GetPath()
                && m_item.GetDisplayName() == previous.m_item.GetDisplayName();
        }
    private:
        ULONGLONG m_timeCreated {::GetTickCount64()};
        wxExplorerBrowserItem m_item;
    };

    ChangeSelEventData m_prevChangeSelEventData;
#endif // #ifdef WX_EXPLORER_BROWSER_PREVENT_DOUBLED_CHANGESEL_EVENTS

    wxDECLARE_NO_COPY_CLASS(wxExplorerBrowserImplHelper);
};

// IUnknown_SetSite is not available on MinGW
// so it we must be load dynamically
HRESULT Call_IUnknown_SetSite(IUnknown* punk, IUnknown* punkSite)
{
    typedef HRESULT (WINAPI *IUnknown_SetSite_t)(IUnknown*, IUnknown*);
    static IUnknown_SetSite_t s_pfnIUnknown_SetSite = nullptr;

    if ( !s_pfnIUnknown_SetSite )
    {
        wxDynamicLibrary dll(wxS("shlwapi.dll"));

        if ( dll.IsLoaded() )
            s_pfnIUnknown_SetSite = (IUnknown_SetSite_t)dll.GetSymbol(wxS("IUnknown_SetSite"));
    }

    if ( s_pfnIUnknown_SetSite )
        return s_pfnIUnknown_SetSite(punk, punkSite);
    else
        return E_FAIL;
}

ULONG wxExplorerBrowserImplHelper::AddRef()
{
    return ::InterlockedIncrement(&m_refCount);
}

ULONG wxExplorerBrowserImplHelper::Release()
{
    LONG refCount = ::InterlockedDecrement(&m_refCount);

    if ( refCount == 0 )
        delete this;

    return refCount;
}

HRESULT wxExplorerBrowserImplHelper::QueryInterface(REFIID riid, void** ppv)
{
    if ( riid == IID_IUnknown )
        *ppv = static_cast<IUnknown*>(static_cast<IServiceProvider*>(this));
    else
    if ( riid == IID_IServiceProvider )
        *ppv = static_cast<IServiceProvider*>(this);
    else
    if ( riid == IID_ICommDlgBrowser
         || riid == IID_ICommDlgBrowser2
         || riid == IID_ICommDlgBrowser3
        )
        *ppv = static_cast<ICommDlgBrowser3*>(this);
    else
    if ( riid == IID_IExplorerBrowserEvents )
        *ppv = static_cast<IExplorerBrowserEvents*>(this);
    else
    if ( riid == IID_IFolderFilter )
        *ppv = static_cast<IFolderFilter*>(this);
    else
    if ( riid == IID_IExplorerPaneVisibility )
        *ppv = static_cast<IExplorerPaneVisibility*>(this);
    else
    {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

HRESULT wxExplorerBrowserImplHelper::QueryService(REFGUID guidService, REFIID riid, void** ppv)
{
    if ( guidService == SID_SExplorerBrowserFrame
         || guidService == SID_ExplorerPaneVisibility
        )
        return QueryInterface(riid, ppv);

    *ppv = nullptr;
    return E_NOINTERFACE;
}

HRESULT wxExplorerBrowserImplHelper::OnDefaultCommand(IShellView* WXUNUSED(ppshv))
{
    wxExplorerBrowserItem ebi;

    if ( _GetSelectedItem(ebi) )
    {
        if ( !_SendNotifyEvent(wxEVT_EXPLORER_BROWSER_DEFAULT_COMMAND, ebi) )
            return S_OK; // event was vetoed, no default action will happen
    }

    return S_FALSE;
}

 HRESULT wxExplorerBrowserImplHelper::OnStateChange(IShellView* WXUNUSED(ppshv), ULONG uChange)
{
     if ( uChange == CDBOSC_SELCHANGE )
    {
        wxExplorerBrowserItem ebi;

        _GetSelectedItem(ebi);

#ifdef WX_EXPLORER_BROWSER_PREVENT_DOUBLED_CHANGESEL_EVENTS
        ChangeSelEventData csed;

        csed.SetItem(ebi);
        if ( csed.IsDoubled(m_prevChangeSelEventData) )
            return S_OK; // do not send the same event

        m_prevChangeSelEventData = csed;
#endif // #ifdef WX_EXPLORER_BROWSER_PREVENT_DOUBLED_CHANGESEL_EVENTS

        _SendNotifyEvent(wxEVT_EXPLORER_BROWSER_SELECTION_CHANGED, ebi);
    }

    return S_OK;
}


 // ShouldShow() is used to actually filter the items
HRESULT wxExplorerBrowserImplHelper::IncludeObject(IShellView* WXUNUSED(ppshv), PCUITEMID_CHILD WXUNUSED(pidl))
{
      return S_OK;
}

HRESULT wxExplorerBrowserImplHelper::Notify(IShellView* WXUNUSED(ppshv), DWORD dwNotifyType)
{
    if ( dwNotifyType == CDB2N_CONTEXTMENU_START )
    {
        wxExplorerBrowserItem ebi;

        if ( _GetSelectedItem(ebi) )
        {
            if ( !_SendNotifyEvent(wxEVT_EXPLORER_BROWSER_CONTEXTMENU_START, ebi) )
                return S_OK; // event was vetoed, context menu will not be shown
        }
    }

    return S_FALSE;
}

HRESULT wxExplorerBrowserImplHelper::GetDefaultMenuText(IShellView* WXUNUSED(ppshv),
                                                        LPWSTR WXUNUSED(pszText),
                                                        int WXUNUSED(cchMax))
{
    return S_FALSE;
}

HRESULT wxExplorerBrowserImplHelper::GetViewFlags(DWORD* pdwFlags)
{
    *pdwFlags = CDB2GVF_NOSELECTVERB;

    // if this flag is not set, neither IncludeObject nor ShouldShow are called
    if ( m_filterMasks.empty() )
        *pdwFlags |= CDB2GVF_NOINCLUDEITEM;

    return S_OK;
}

/* Never gets called?! */
HRESULT wxExplorerBrowserImplHelper::GetCurrentFilter(LPWSTR WXUNUSED(pszFileSpec), int WXUNUSED(cchFileSpec))
{
    return E_NOTIMPL;
}

/* Never gets called?! */
HRESULT wxExplorerBrowserImplHelper::OnColumnClicked(IShellView* WXUNUSED(ppshv), int WXUNUSED(iColumn))
{
    return S_OK;
}

HRESULT wxExplorerBrowserImplHelper::OnPreViewCreated(IShellView* WXUNUSED(ppshv))
{
    return S_OK;
}

HRESULT wxExplorerBrowserImplHelper::OnNavigationPending(PCIDLIST_ABSOLUTE pidlFolder)
{
    if ( _SendNotifyEvent(wxEVT_EXPLORER_BROWSER_NAVIGATING, pidlFolder) )
        return S_OK;
    else
        return E_FAIL;
}

HRESULT wxExplorerBrowserImplHelper::OnViewCreated(IShellView* psv)
{
    HRESULT hr;
    wxCOMPtr<IFolderView> fv;

    hr = psv->QueryInterface(wxIID_PPV_ARGS(IFolderView, &fv));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IShellView::QueryInterface(IFolderView)"), hr);
        return E_FAIL;
    }

    wxCOMPtr<IPersistFolder2> pf2;

    hr = fv->GetFolder(wxIID_PPV_ARGS(IPersistFolder2, &pf2));
    if  ( FAILED(hr) )
    {
        wxLogApiError(wxS("IFolderView::GetFolder()"), hr);
        return E_FAIL;
    }

    PIDLIST_ABSOLUTE pidl = nullptr;

    hr = pf2->GetCurFolder(&pidl);
    if  ( FAILED(hr) )
    {
        wxLogApiError(wxS("IPersistFolder2::GetCurFolder()"), hr);
        return E_FAIL;
    }

    _SendNotifyEvent(wxEVT_EXPLORER_BROWSER_VIEW_CREATED, pidl);
    ::CoTaskMemFree(pidl);

    return S_OK;
}

HRESULT wxExplorerBrowserImplHelper::OnNavigationComplete(PCIDLIST_ABSOLUTE pidlFolder)
{
    _SendNotifyEvent(wxEVT_EXPLORER_BROWSER_NAVIGATION_COMPLETE, pidlFolder);
    return S_OK;
}

HRESULT wxExplorerBrowserImplHelper::OnNavigationFailed(PCIDLIST_ABSOLUTE pidlFolder)
{
    _SendNotifyEvent(wxEVT_EXPLORER_BROWSER_NAVIGATION_FAILED, pidlFolder);
    return S_OK;
}

HRESULT wxExplorerBrowserImplHelper::GetEnumFlags(IShellFolder* WXUNUSED(psf),
                                                  PCIDLIST_ABSOLUTE WXUNUSED(pidlFolder),
                                                  HWND* WXUNUSED(phwnd),
                                                  DWORD* WXUNUSED(pgrfFlags))
{
    return S_OK;
}


HRESULT wxExplorerBrowserImplHelper::ShouldShow(IShellFolder* psf,
                                                PCIDLIST_ABSOLUTE WXUNUSED(pidlFolder),
                                                PCUITEMID_CHILD pidlItem)
{
    if ( m_filterMasks.empty() )
        return S_OK;

    HRESULT hr;
    wxCOMPtr<IShellItem> si;

    hr = ::SHCreateItemWithParent(nullptr, psf, pidlItem, wxIID_PPV_ARGS(IShellItem, &si));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("::SHCreateItemWithParent()"), hr);
        return E_FAIL;
    }

    wxExplorerBrowserItem ebi;

    if ( !_IShellItem2wxExplorerBrowserItem(si, ebi)
          || ebi.GetType() == wxExplorerBrowserItem::Unknown )
        return E_FAIL;

    if (  !(ebi.GetType() & m_filterTypes) )
        return S_OK; // do not filter this item type

    wxString name;

    if ( ebi.GetType() == wxExplorerBrowserItem::Other )
    {
        name = ebi.GetDisplayName();
    }
    else
    {
        // Since the path for directories does not end with a slash,
        // wxFileName::GetFullName here will also return the parent-relative
        // directory name because it thinks it is a file
        name = wxFileName(ebi.GetPath()).GetFullName();
    }

    // wxMatchWild() is surprisingly case sensitive even on MSW
    name.MakeUpper();

    for ( const auto& mask : m_filterMasks )
    {
        if ( wxMatchWild(mask, name, false) )
            return S_OK;
    }

    return S_FALSE;
}

STDMETHODIMP wxExplorerBrowserImplHelper::GetPaneState(REFEXPLORERPANE ep, EXPLORERPANESTATE *peps)
{
    if ( ep == EP_NavPane )
        *peps = m_paneSettings.GetFlags(wxExplorerBrowser::EP_NavPane);
    else
    if ( ep == EP_Commands )
        *peps = m_paneSettings.GetFlags(wxExplorerBrowser::EP_Commands);
    else
    if ( ep == EP_Commands_Organize )
        *peps = m_paneSettings.GetFlags(wxExplorerBrowser::EP_Commands_Organize);
    else
    if ( ep == EP_Commands_View )
        *peps = m_paneSettings.GetFlags(wxExplorerBrowser::EP_Commands_View);
    else
    if ( ep == EP_DetailsPane )
        *peps = m_paneSettings.GetFlags(wxExplorerBrowser::EP_DetailsPane);
    else
    if ( ep == EP_PreviewPane )
        *peps = m_paneSettings.GetFlags(wxExplorerBrowser::EP_PreviewPane);
    else
    if ( ep == EP_QueryPane )
        *peps = m_paneSettings.GetFlags(wxExplorerBrowser::EP_QueryPane);
    else
    if ( ep == EP_AdvQueryPane )
        *peps = m_paneSettings.GetFlags(wxExplorerBrowser::EP_AdvQueryPane);
    else
    if ( ep == EP_StatusBar )
        *peps = m_paneSettings.GetFlags(wxExplorerBrowser::EP_StatusBar);
    else
    if ( ep == EP_Ribbon )
        *peps = m_paneSettings.GetFlags(wxExplorerBrowser::EP_Ribbon);
    else
        return E_INVALIDARG;

    return S_OK;
}

// Filtering does not work for query-backed views such as libraries or search results.
// Neither ICommDlgBrowser::IncludeObject() nor IFolderFilter::ShouldShow() are called for those, see e.g.
// https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/252a9c82-617c-4126-8347-56dcedb4342f
// ICommDlgBrowser3::GetFilter() is never called for any folder at all...
bool wxExplorerBrowserImplHelper::_SetFilter(const wxArrayString& fileMasks, wxUint32 itemTypes)
{
    m_filterMasks.reserve(fileMasks.size());

    // we do not want case-sensitive filtering
    for ( const auto& mask : fileMasks )
        m_filterMasks.push_back(mask.Upper());

    m_filterTypes = itemTypes;

    return true;
}

bool wxExplorerBrowserImplHelper::_RemoveFilter()
{
    m_filterMasks.clear();
    return true;
}

bool wxExplorerBrowserImplHelper::_SetPaneSettings(const wxExplorerBrowser::PaneSettings& settings)
{
    m_paneSettings = settings;
    return true;
}

wxExplorerBrowserItem::Type wxExplorerBrowserImplHelper::_SFGAO2wxExplorerBrowserItemType(SFGAOF attr)
{
    static const SFGAOF FileSystemFile = SFGAO_FILESYSTEM | SFGAO_STREAM;
    static const SFGAOF FileSystemDirectory = SFGAO_FILESYSTEM | SFGAO_FOLDER;
    static const SFGAOF VirtualZipDirectory = SFGAO_FILESYSTEM | SFGAO_FOLDER | SFGAO_STREAM;

    // remove the the attributes we are not interested in
    attr &= (SFGAO_FILESYSTEM | SFGAO_FOLDER | SFGAO_STREAM);

    switch ( attr )
    {
        case 0:
            return wxExplorerBrowserItem::Unknown;
        case FileSystemFile:
        case VirtualZipDirectory:
            return wxExplorerBrowserItem::File;
        case FileSystemDirectory:
            return wxExplorerBrowserItem::Directory;
        default:
            return wxExplorerBrowserItem::Other;
    }
}

bool wxExplorerBrowserImplHelper::_IShellItem2wxExplorerBrowserItem(IShellItem* item, wxExplorerBrowserItem& ebi)
{
    wxCHECK(item, false);

    static const SFGAOF mask = SFGAO_FILESYSTEM | SFGAO_FOLDER | SFGAO_STREAM | SFGAO_LINK;

    HRESULT hr;
    SFGAOF attr;

    hr = item->GetAttributes(mask, &attr);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("(IShellItem::GetAttributesOf()"), hr);
        return false;
    }

    PWSTR name = nullptr;
    wxString path, displayName;

    // will fail for non-filesystem items
    if ( SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name)) )
    {
        path = name;
        ::CoTaskMemFree(name);
    }

    hr = item->GetDisplayName(SIGDN_NORMALDISPLAY, &name);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IShellItem::GetDisplayName(SIGDN_NORMALDISPLAY)"), hr);
        return false;
    }
    else
    {
        displayName = name;
        ::CoTaskMemFree(name);
    }

    ebi.SetType(_SFGAO2wxExplorerBrowserItemType(attr));
    ebi.SetPath(path);
    ebi.SetDisplayName(displayName);
    ebi.SetSFGAO(attr);

    return true;
}

bool wxExplorerBrowserImplHelper::_PPIDL2wxExplorerBrowserItem(PCIDLIST_ABSOLUTE pidl, wxExplorerBrowserItem& ebi)
{
    wxCHECK(pidl, false);

    HRESULT hr;
    wxCOMPtr<IShellItem> si;

    hr = ::SHCreateItemFromIDList(pidl, wxIID_PPV_ARGS(IShellItem, &si));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("SHCreateItemFromIDList"), hr);
        return false;
    }

    return _IShellItem2wxExplorerBrowserItem(si, ebi);
}


bool wxExplorerBrowserImplHelper::_SendNotifyEvent(wxEventType command, PCIDLIST_ABSOLUTE list) const
{
    wxExplorerBrowserItem ebi;

    if ( _PPIDL2wxExplorerBrowserItem(list, ebi) )
        return _SendNotifyEvent(command, ebi);

    return false;
}

bool wxExplorerBrowserImplHelper::_SendNotifyEvent(wxEventType command,
                                                   const wxExplorerBrowserItem& ebi) const
{
    wxExplorerBrowserEvent evt(command, m_host->GetId());

    evt.SetEventObject(m_host);
    evt.SetItem(ebi);

    if ( m_host->ProcessWindowEvent(evt) )
        return evt.IsAllowed();

    return true;
}

bool wxExplorerBrowserImplHelper::_GetSelectedItem(wxExplorerBrowserItem& ebi)
{
    HRESULT hr;
    wxCOMPtr<IFolderView2> fv2;

    hr = m_explorerBrowser->GetCurrentView(wxIID_PPV_ARGS(IFolderView2, &fv2));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::GetCurrentView()"), hr);
        return false;
    }

    int selected;

    if ( fv2->GetSelectedItem(-1, &selected) != S_OK )
    {
        // no selected items
        return false;
    }

    wxCOMPtr<IShellItem> si;

    hr = fv2->GetItem(selected, wxIID_PPV_ARGS(IShellItem, &si));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IFolderView2::GetItem()"), hr);
        return false;
    }

    return _IShellItem2wxExplorerBrowserItem(si, ebi);
}

} // unnamed namespace

/***************************************************************************

    class wxExplorerBrowserImpl
    ---------------------------------
    actual implemention of wxExplorerBrowser methods

*****************************************************************************/

class wxExplorerBrowser::wxExplorerBrowserImpl
{
public:
    wxExplorerBrowserImpl(wxWindow* host) : m_host{host} {}
    ~wxExplorerBrowserImpl();

    bool Create(const CreateStruct& createStruct, const wxString& path);

    bool SetFolderSettings(const FolderSettings& folderSettings);
    bool GetOptions(wxUint32& options);
    bool SetOptions(wxUint32 options);
    bool SetEmptyText(const wxString& text);
    bool SetPropertyBag(const wxString& bag);
    bool BrowseTo(const wxString& item, bool keepWordWheelText);
    bool BrowseTo(BrowseTarget target, bool keepWordWheelText);

    bool Refresh();

    bool SearchFolder(const wxString& str);
    bool RemoveAll();

    bool SelectItems(const wxExplorerBrowserItem::List& items, bool notTakeFocus);
    bool DeselectAllItems(bool notTakeFocus);
    bool GetSelectedItems(wxExplorerBrowserItem::List& items, wxUint32 itemTypes);

    bool GetAllItems(wxExplorerBrowserItem::List& items, wxUint32 itemTypes);

    bool GetFolder(wxExplorerBrowserItem& item);

    bool SetFilter(const wxArrayString& fileMasks, wxUint32 itemTypes);
    bool RemoveFilter();

    bool SetPaneSettings(const PaneSettings& settings);

    void SetSize(const wxSize& size);
    bool TranslateMessage(WXMSG* msg);

    IExplorerBrowser* GetIExplorerBrowser() { return m_explorerBrowser.get(); }
private:
    wxWindow* m_host {nullptr};;
    wxCOMPtr<IExplorerBrowser> m_explorerBrowser;
    wxCOMPtr<wxExplorerBrowserImplHelper> m_explorerBrowserHelper;
    DWORD m_adviseCookie {0};

    bool GetCurrentView(wxCOMPtr<IShellView>& sv);
    bool GetCurrentView(wxCOMPtr<IFolderView2>& sv);

    static bool ShellItemArrayToExplorerBrowserItemList(wxCOMPtr<IShellItemArray> shellItems,
                                                        wxExplorerBrowserItem::List& items, wxUint32 itemTypes);
};

wxExplorerBrowser::wxExplorerBrowserImpl::~wxExplorerBrowserImpl()
{
    if ( m_explorerBrowser )
    {
        HRESULT hr;

        if ( m_explorerBrowserHelper )
        {
            hr = Call_IUnknown_SetSite(m_explorerBrowser, nullptr);
            if ( FAILED(hr) )
                wxLogApiError(wxS("Call_IUnknown_SetSite()"), hr);

            hr = m_explorerBrowser->Unadvise(m_adviseCookie);
            if ( FAILED(hr) )
                wxLogApiError(wxS("IExplorerBrowser::Unadvise()"), hr);

            wxCOMPtr<IFolderFilterSite> ffs;

            hr = m_explorerBrowser->QueryInterface(wxIID_PPV_ARGS(IFolderFilterSite, &ffs));
            if ( SUCCEEDED(hr) )
            {
                hr = ffs->SetFilter(nullptr);
                if ( FAILED(hr) )
                    wxLogApiError(wxS("IFolderFilterSite::SetFilter(nullptr)"), hr);
            }
            else
                wxLogApiError(wxS("IExplorerBrowser::QueryInterface(IFolderFilterSite)"), hr);
        }

        hr = m_explorerBrowser->Destroy();
        if ( FAILED(hr) )
            wxLogApiError(wxS("IExplorerBrowser::Destroy()"), hr);
    }
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::Create(const CreateStruct& createStruct,
                                                      const wxString& path)
{
    wxCHECK(m_host, false); // the host window must be already created
    wxCHECK(!m_explorerBrowser, false); // prevent attempted multiple calls to Create()

    HRESULT hr;

    hr = ::CoCreateInstance(CLSID_ExplorerBrowser, nullptr, CLSCTX_INPROC,
                wxIID_PPV_ARGS(IExplorerBrowser, &m_explorerBrowser));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("CoCreateInstance(CLSID_ExplorerBrowser)"), hr);
        return false;
    }

    m_explorerBrowserHelper = new wxExplorerBrowserImplHelper(m_host, m_explorerBrowser.get());

    // new wxExplorerBrowserImplHelper() creates the object with refcount = 1.
    // wxCOMPtr lacks an attach-like method, assigning its value with operator=
    // increases the refcount of the contained object to 2, which is wrong here,
    // so we have to work around it and decrease the count back by calling Release()
    // otherwise the owned object would never get destroyed.
    m_explorerBrowserHelper->Release();

    m_explorerBrowserHelper->_SetPaneSettings(createStruct.paneSettings);

    hr = m_explorerBrowser->SetOptions(static_cast<::EXPLORER_BROWSER_OPTIONS>(createStruct.options));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::SetOptions()"), hr);
        return false;
    }

    hr = Call_IUnknown_SetSite(m_explorerBrowser,
        static_cast<IUnknown*>(static_cast<IServiceProvider*>(m_explorerBrowserHelper)));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("Call_IUnknown_SetSite()"), hr);
        return false;
    }

    wxCOMPtr<IFolderFilterSite> ffs;

    hr = m_explorerBrowser->QueryInterface(wxIID_PPV_ARGS(IFolderFilterSite, &ffs));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::QueryInterface(IFolderFilterSite)"), hr);
        return false;
    }

    hr = ffs->SetFilter(static_cast<IUnknown*>(static_cast<IServiceProvider*>(m_explorerBrowserHelper)));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IFolderFilterSite::SetFilter()"), hr);
        return false;
    }

    RECT r = {0};
    FOLDERSETTINGS fs = {0};

    fs.ViewMode = createStruct.folderSettings.m_viewMode;
    fs.fFlags = createStruct.folderSettings.m_flags;

    hr = m_explorerBrowser->Initialize(m_host->GetHWND(), &r, &fs);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::Initialize()"), hr);
        return false;
    }

    hr = m_explorerBrowser->Advise(m_explorerBrowserHelper, &m_adviseCookie);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::Advise()"), hr);
        return false;
    }

    return BrowseTo(path, false);
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::SetFolderSettings(const FolderSettings& folderSettings)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;
    FOLDERSETTINGS fs = {0};

    fs.ViewMode = folderSettings.m_viewMode;
    fs.fFlags = folderSettings.m_flags;

    hr = m_explorerBrowser->SetFolderSettings(&fs);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::SetFolderSettings()"), hr);
        return false;
    }

    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::SetPropertyBag(const wxString& bag)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;

    hr = m_explorerBrowser->SetPropertyBag(bag.wc_str());
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::SetPropertyBag()"), hr);
        return false;
    }

    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::GetOptions(wxUint32& options)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;
    ::EXPLORER_BROWSER_OPTIONS ebo = static_cast<::EXPLORER_BROWSER_OPTIONS>(options);

    hr = m_explorerBrowser->GetOptions(&ebo);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::GetOptions()"), hr);
        return false;
    }

    options = ebo;
    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::SetOptions(wxUint32 options)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;

    hr = m_explorerBrowser->SetOptions(static_cast<::EXPLORER_BROWSER_OPTIONS>(options));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::SetOptions()"), hr);
        return false;
    }

    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::SetEmptyText(const wxString& text)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;

    hr = m_explorerBrowser->SetEmptyText(text.wc_str());
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::()"), hr);
        return false;
    }

    return true;
}


bool wxExplorerBrowser::wxExplorerBrowserImpl::BrowseTo(const wxString& item, bool keepWordWheelText)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;
    PIDLIST_ABSOLUTE pidl = nullptr;

    hr = ::SHParseDisplayName(PCWSTR(item.wc_str()), nullptr, &pidl, 0, nullptr);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("SHParseDisplayName()"), hr);
        return false;
    }

    UINT flags = 0;

    if ( keepWordWheelText )
        flags |= SBSP_KEEPWORDWHEELTEXT;


    hr = m_explorerBrowser->BrowseToIDList(pidl, flags);
    ::CoTaskMemFree(pidl);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::BrowseToIDList()"), hr);
        return false;
    }

    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::BrowseTo(BrowseTarget target, bool keepWordWheelText)
{
    wxCHECK(m_explorerBrowser, false);

    UINT flags;

    switch ( target )
    {
        case Parent:
            flags = SBSP_PARENT;
            break;
        case HistoryBack:
            flags = SBSP_NAVIGATEBACK;
            break;
        case HistoryForward:
            flags = SBSP_NAVIGATEFORWARD;
            break;
        default:
            wxFAIL_MSG(wxS("Invalid item value"));
            return false;
    }

    if ( keepWordWheelText )
        flags |= SBSP_KEEPWORDWHEELTEXT;

    HRESULT hr;

    hr = m_explorerBrowser->BrowseToIDList(nullptr, flags);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::BrowseToIDList()"), hr);
        return false;
    }

    return true;
}


bool wxExplorerBrowser::wxExplorerBrowserImpl::Refresh()
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;
    wxCOMPtr<IShellView> sv;

    if ( !GetCurrentView(sv) )
        return false;

    hr = sv->Refresh();
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IShellView::Refresh()"), hr);
        return false;
    }
    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::SearchFolder(const wxString& str)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;
    wxCOMPtr<IShellView> sv;

    if ( !GetCurrentView(sv) )
        return false;

    wxCOMPtr<IDispatch> d;
    wxCOMPtr<IShellFolderViewDual3> sfvd3;

    hr = sv->GetItemObject(SVGIO_BACKGROUND, wxIID_PPV_ARGS(IDispatch, &d));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IShellView::GetItemObject(SVGIO_BACKGROUND)"), hr);
        return false;
    }

    hr = d->QueryInterface(wxIID_PPV_ARGS(IShellFolderViewDual3, &sfvd3));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IDispatch::QueryInterface(IShellFolderViewDual3)"), hr);
        return false;
    }

    hr = sfvd3->FilterView(BSTR(str.wc_str()));
    if  ( FAILED(hr) )
    {
        wxLogApiError(wxS("IShellFolderViewDual3::FilterView()"), hr);
        return false;
    }

    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::RemoveAll()
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;

    hr = m_explorerBrowser->RemoveAll();
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::RemoveAll()"), hr);
        return false;
    }

    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::SelectItems(const wxExplorerBrowserItem::List& items, bool notTakeFocus)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;
    wxCOMPtr<IShellView> sv;
    wxCOMPtr<IFolderView2> fv2;

    if ( !GetCurrentView(sv) )
        return false;

    if ( !GetCurrentView(fv2) )
        return false;

    wxCOMPtr<IShellFolder> sf;

    hr = fv2->GetFolder(wxIID_PPV_ARGS(IShellFolder, &sf));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IFolderView2::GetFolder()"), hr);
        return false;
    }


    PIDLIST_RELATIVE pidl = nullptr;
    wxString name;

    for ( const auto& item : items )
    {
        name = item.GetPath();
        if ( name.empty() )
            name = item.GetDisplayName();

        hr = sf->ParseDisplayName(nullptr, nullptr, LPWSTR(name.wc_str()), nullptr, &pidl, nullptr);
        if ( FAILED(hr) )
        {
            wxLogApiError(wxS("IShellFolder::ParseDisplayName()"), hr);
            return false;
        }

        UINT flags = SVSI_SELECT;

        if ( notTakeFocus )
            flags |= SVSI_NOTAKEFOCUS;

        hr = sv->SelectItem(pidl, flags);
        ::CoTaskMemFree(pidl);
        if ( FAILED(hr) )
        {
            wxLogApiError(wxS("IShellView::SelectItem()"), hr);
            return false;
        }
    }

    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::DeselectAllItems(bool notTakeFocus)
{
    wxCHECK(m_explorerBrowser, false);

    wxCOMPtr<IShellView> sv;

    if ( !GetCurrentView(sv) )
        return false;

    HRESULT hr;
    UINT flags = SVSI_DESELECTOTHERS;

    if ( notTakeFocus )
        flags |= SVSI_NOTAKEFOCUS;

    hr = sv->SelectItem(nullptr, flags);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IShellView::SelectItem()"), hr);
        return false;
    }

    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::GetSelectedItems(wxExplorerBrowserItem::List& items,
                                                                wxUint32 itemTypes)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;
    wxCOMPtr<IFolderView2> fv2;

    if ( !GetCurrentView(fv2) )
        return false;

    wxCOMPtr<IShellItemArray> sia;

    hr = fv2->GetSelection(FALSE, &sia);
    if ( FAILED(hr) ) // no items selected
    {
        return true;
    }

    return ShellItemArrayToExplorerBrowserItemList(sia, items, itemTypes);
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::GetAllItems(wxExplorerBrowserItem::List& items,
                                                           wxUint32 itemTypes)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;
    wxCOMPtr<IFolderView2> fv2;

    if ( !GetCurrentView(fv2) )
        return false;

    wxCOMPtr<IShellItemArray> sia;

    hr = fv2->Items(SVGIO_ALLVIEW | SVGIO_FLAG_VIEWORDER, wxIID_PPV_ARGS(IShellItemArray, &sia));
    if ( FAILED(hr) ) // no items
    {
        wxLogApiError(wxS("IFolderView2::Items()"), hr);
        return false;
    }

    return ShellItemArrayToExplorerBrowserItemList(sia, items, itemTypes);
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::GetFolder(wxExplorerBrowserItem& item)
{
    wxCHECK(m_explorerBrowser, false);

    HRESULT hr;
    wxCOMPtr<IFolderView2> fv2;

    if ( !GetCurrentView(fv2) )
        return false;

    wxCOMPtr<IPersistFolder2> pf2;

    hr = fv2->GetFolder(wxIID_PPV_ARGS(IPersistFolder2, &pf2));
    if  ( FAILED(hr) )
    {
        wxLogApiError(wxS("IFolderView2::GetFolder()"), hr);
        return false;
    }

    PIDLIST_ABSOLUTE pidl = nullptr;

    hr = pf2->GetCurFolder(&pidl);
    if  ( FAILED(hr) )
    {
        wxLogApiError(wxS("IPersistFolder2::GetCurFolder()"), hr);
        return false;
    }

    const bool result = wxExplorerBrowserImplHelper::_PPIDL2wxExplorerBrowserItem(pidl, item);
    ::CoTaskMemFree(pidl);

    return result;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::SetFilter(const wxArrayString& fileMasks, wxUint32 itemTypes)
{
    wxCHECK(m_explorerBrowserHelper, false);

    return m_explorerBrowserHelper->_SetFilter(fileMasks, itemTypes);
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::RemoveFilter()
{
    wxCHECK(m_explorerBrowserHelper, false);

    return m_explorerBrowserHelper->_RemoveFilter();
}


bool wxExplorerBrowser::wxExplorerBrowserImpl::SetPaneSettings(const PaneSettings& settings)
{
    wxCHECK(m_explorerBrowserHelper, false);

    return m_explorerBrowserHelper->_SetPaneSettings(settings);
}

void wxExplorerBrowser::wxExplorerBrowserImpl::SetSize(const wxSize& size)
{
    if ( !m_explorerBrowser )
        return;

    RECT rect = {0};

    rect.right = size.GetWidth();
    rect.bottom = size.GetHeight();
    m_explorerBrowser->SetRect(nullptr, rect);
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::TranslateMessage(WXMSG* msg)
{
    if ( m_explorerBrowser && WM_KEYFIRST <= msg->message && msg->message <= WM_KEYLAST )
    {
        wxCOMPtr<IInputObject> io;

        if ( SUCCEEDED(m_explorerBrowser->QueryInterface(wxIID_PPV_ARGS(IInputObject, &io))) )
        {
            if ( io->HasFocusIO() == S_OK )
            {
                if ( io->TranslateAcceleratorIO(msg) == S_OK )
                    return true;
            }
        }
    }

    return false;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::GetCurrentView(wxCOMPtr<IShellView>& sv)
{
    HRESULT hr = m_explorerBrowser->GetCurrentView(wxIID_PPV_ARGS(IShellView, &sv));

    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::GetCurrentView(IShellView)"), hr);
        return false;
    }

    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::GetCurrentView(wxCOMPtr<IFolderView2>& sv)
{
    HRESULT hr = m_explorerBrowser->GetCurrentView(wxIID_PPV_ARGS(IFolderView2, &sv));

    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IExplorerBrowser::GetCurrentView(IFolderView2)"), hr);
        return false;
    }

    return true;
}

bool wxExplorerBrowser::wxExplorerBrowserImpl::ShellItemArrayToExplorerBrowserItemList(wxCOMPtr<IShellItemArray> shellItems,
                                                wxExplorerBrowserItem::List& items, wxUint32 itemTypes)
{
    HRESULT hr;
    DWORD count = 0;

    hr = shellItems->GetCount(&count);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IShellItemArray::GetCount()"), hr);
        return false;
    }

    wxExplorerBrowserItem::List tmpItems;

    tmpItems.reserve(static_cast<size_t>(count));

    for ( DWORD i = 0; i < count; ++i )
    {
        wxCOMPtr<IShellItem> si;
        wxExplorerBrowserItem ebi;

        hr = shellItems->GetItemAt(i, &si);
        if ( FAILED(hr) )
        {
            wxLogApiError(wxS("IShellItemArray::GetItemAt()"), hr);
            return false;
        }

        if ( !wxExplorerBrowserImplHelper::_IShellItem2wxExplorerBrowserItem(si, ebi) )
            return false;

        if ( (itemTypes & ebi.GetType()) )
            tmpItems.push_back(ebi);
    }

    items = std::move(tmpItems);
    return true;
}

/***************************************************************************

    class wxExplorerBrowser
    ---------------------------------
    methods just call the actual implemention in wxExplorerBrowserImpl

*****************************************************************************/

const char wxExplorerBrowserNameStr[] = "wxExplorerBrowser";

wxIMPLEMENT_DYNAMIC_CLASS(wxExplorerBrowser, wxPanel);

wxExplorerBrowser::wxExplorerBrowser(wxWindow* parent,
                                     const CreateStruct& createStruct,
                                     const wxString& path,
                                     wxWindowID id,
                                     const wxPoint& pos, const wxSize& size,
                                     const wxString& name)
{
    Create(parent, createStruct, path, id, pos, size, name);
}

wxExplorerBrowser::~wxExplorerBrowser()
{
    if ( m_impl )
        delete m_impl;
}


bool wxExplorerBrowser::Create(wxWindow* parent,
                               const CreateStruct& createStruct,
                               const wxString& path,
                               wxWindowID id,
                               const wxPoint& pos, const wxSize& size,
                               const wxString& name)
{
    if ( !wxPanel::Create(parent, id, pos, size,
            wxBORDER_NONE | wxWANTS_CHARS | wxCLIP_CHILDREN | wxCLIP_SIBLINGS,
            name ) )
    {
        return false;
    }

    // when the ExplorerBrowser was hosted directly, there was an issue
    // with its wxButton sibling, where upon a wxButton
    // having focus and user clicking on an ExplorerBrowser item
    // resulted in the application being stuck in an infinite
    // loop in the window procedure.
    // Thus we create a wxWindow which will actually host the browser
    // as the only child of wxExplorerBrowser
    m_host = new wxWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxBORDER_NONE | wxWANTS_CHARS | wxCLIP_CHILDREN | wxCLIP_SIBLINGS,
        wxS("wxExplorerBrowserWindow"));

    m_impl = new wxExplorerBrowserImpl(m_host);

    if ( m_impl->Create(createStruct, path) )
    {
        Bind(wxEVT_SIZE, &wxExplorerBrowser::OnSize, this);

        // the following four lines are just
        // to suppress needless window background erasing
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &wxExplorerBrowser::OnPaint, this);
        m_host->SetBackgroundStyle(wxBG_STYLE_PAINT);
        m_host->Bind(wxEVT_PAINT, &wxExplorerBrowser::OnPaint, this);

        return true;
    }

    return false;
}

bool wxExplorerBrowser::SetFolderSettings(const FolderSettings& folderSettings)
{
    wxCHECK(m_impl, false);

    return m_impl->SetFolderSettings(folderSettings);
}

bool wxExplorerBrowser::GetOptions(wxUint32& options)
{
    wxCHECK(m_impl, false);

    return m_impl->GetOptions(options);
}
bool wxExplorerBrowser::SetOptions(wxUint32 options)
{
    wxCHECK(m_impl, false);

    return m_impl->SetOptions(options);
}

bool wxExplorerBrowser::SetEmptyText(const wxString& text)
{
    wxCHECK(m_impl, false);

    return m_impl->SetEmptyText(text);
}

bool wxExplorerBrowser::SetPropertyBag(const wxString& bag)
{
    wxCHECK(m_impl, false);

    return m_impl->SetPropertyBag(bag);
}

bool wxExplorerBrowser::BrowseTo(const wxString& item, bool keepWordWheelText)
{
    wxCHECK(m_impl, false);

    return m_impl->BrowseTo(item, keepWordWheelText);
}

bool wxExplorerBrowser::BrowseTo(BrowseTarget target, bool keepWordWheelText)
{
    wxCHECK(m_impl, false);

    return m_impl->BrowseTo(target, keepWordWheelText);
}

bool wxExplorerBrowser::Refresh()
{
    wxCHECK(m_impl, false);

    return m_impl->Refresh();
}

bool wxExplorerBrowser::SearchFolder(const wxString& str)
{
    wxCHECK(m_impl, false);

    return m_impl->SearchFolder(str);
}

bool wxExplorerBrowser::RemoveAll()
{
    wxCHECK(m_impl, false);

    return m_impl->RemoveAll();
}

bool wxExplorerBrowser::SelectItems(const wxExplorerBrowserItem::List& items, bool notTakeFocus)
{
    wxCHECK(m_impl, false);

    return m_impl->SelectItems(items, notTakeFocus);
}

bool wxExplorerBrowser::DeselectAllItems(bool notTakeFocus)
{
    wxCHECK(m_impl, false);

    return m_impl->DeselectAllItems(notTakeFocus);
}
bool wxExplorerBrowser::GetSelectedItems(wxExplorerBrowserItem::List& items, wxUint32 itemTypes)
{
    wxCHECK(m_impl, false);
    wxCHECK_MSG(itemTypes, false, wxS("At least one item type must be specified"));

    return m_impl->GetSelectedItems(items, itemTypes);
}

bool wxExplorerBrowser::GetAllItems(wxExplorerBrowserItem::List& items, wxUint32 itemTypes)
{
    wxCHECK(m_impl, false);
    wxCHECK_MSG(itemTypes, false, wxS("At least one item type must be specified"));

    return m_impl->GetAllItems(items, itemTypes);
}

bool wxExplorerBrowser::GetFolder(wxExplorerBrowserItem& item)
{
    wxCHECK(m_impl, false);

    return m_impl->GetFolder(item);
}

bool wxExplorerBrowser::SetFilter(const wxArrayString& fileMasks, wxUint32 itemTypes)
{
    wxCHECK(m_impl, false);
    wxCHECK_MSG(itemTypes, false, wxS("At least one item type must be specified"));

    if ( m_impl->SetFilter(fileMasks, itemTypes) )
    {
        Refresh();
        return true;
    }

    return false;
}

bool wxExplorerBrowser::RemoveFilter()
{
    wxCHECK(m_impl, false);

    if ( m_impl->RemoveFilter() )
    {
        Refresh();
        return true;
    }

    return false;
}

bool wxExplorerBrowser::SetPaneSettings(const PaneSettings& settings)
{
    wxCHECK(m_impl, false);

    return m_impl->SetPaneSettings(settings);
}

void* wxExplorerBrowser::GetIExplorerBrowser()
{
    wxCHECK(m_impl, nullptr);

    return m_impl->GetIExplorerBrowser();
}

bool wxExplorerBrowser::MSWTranslateMessage(WXMSG* msg)
{
    if ( m_impl && m_impl->TranslateMessage(msg) )
        return true;

    return wxPanel::MSWTranslateMessage(msg);
}

void wxExplorerBrowser::OnSize(wxSizeEvent& evt)
{
    const wxSize size = GetClientSize();

    m_host->SetSize(size);
    m_impl->SetSize(size);
    evt.Skip();
}

void wxExplorerBrowser::OnPaint(wxPaintEvent& evt)
{
    wxWindow* w = static_cast<wxWindow*>(evt.GetEventObject());

    if ( w )
    {
        wxPaintDC pdc(w);
        // do nothing here, the window is covered by the ExplorerBrowser,
        // this paint handler exists solely to prevent useless background
        // erasing
    }
}

/***************************************************************************

    class wxExplorerBrowserEvent
    ---------------------------------
    just some definitions through wxWidgets macros,
    the methods are implemented inline in the header

*****************************************************************************/

wxIMPLEMENT_DYNAMIC_CLASS(wxExplorerBrowserEvent, wxNotifyEvent);

wxDEFINE_EVENT(wxEVT_EXPLORER_BROWSER_DEFAULT_COMMAND, wxExplorerBrowserEvent);
wxDEFINE_EVENT(wxEVT_EXPLORER_BROWSER_SELECTION_CHANGED, wxExplorerBrowserEvent);
wxDEFINE_EVENT(wxEVT_EXPLORER_BROWSER_CONTEXTMENU_START, wxExplorerBrowserEvent);
wxDEFINE_EVENT(wxEVT_EXPLORER_BROWSER_NAVIGATING, wxExplorerBrowserEvent);
wxDEFINE_EVENT(wxEVT_EXPLORER_BROWSER_NAVIGATION_COMPLETE, wxExplorerBrowserEvent);
wxDEFINE_EVENT(wxEVT_EXPLORER_BROWSER_NAVIGATION_FAILED, wxExplorerBrowserEvent);
wxDEFINE_EVENT(wxEVT_EXPLORER_BROWSER_VIEW_CREATED, wxExplorerBrowserEvent);
