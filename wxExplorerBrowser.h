////////////////////////////////////////////////////////////////////////////////////
//
//  Name:        wxExplorerBrowser.h
//  Purpose:     Implements wxExplorerBrowser, a class for hosting
//               IExplorerBrowser in wxWidgets applications
//  Author:      PB
//  Copyright:   (c) 2018 PB <pbfordev@gmail.com>
//  Licence:     wxWindows licence
//
////////////////////////////////////////////////////////////////////////////////////

#ifndef WX_EXPLORER_BROWSER_H_DEFINED
#define WX_EXPLORER_BROWSER_H_DEFINED

#include <vector>

#include <wx/panel.h>

/** @file 
    
    Contains wxExplorerBrowser, a wxWidgets control hosting IExplorerBrowser.
*/

/**    
    Represents a very simplified shell item. 
*/
class wxExplorerBrowserItem
{
public:
    typedef std::vector<wxExplorerBrowserItem> List;

    /**    
        Zip files are reported as File although they 
        can also browsed as folders, all items inside 
        zips are reported as Other.    
    */    
    enum Type
    {
        Unknown     = 0,    /*!< item type is unknown, unimportant, or item is invalid  */
        File        = 0x01, /*!< filesystem file        */
        Directory   = 0x02, /*!< filesystem directory   */
        Other       = 0x08  /*!< not File or Directory  */
    };

    wxExplorerBrowserItem(Type type = Unknown)
        : m_type(type), m_SFGAO(0)  {}

    Type GetType() const { return m_type; }

    /*! Returns the full filesystem path if the item is File
        or Directory and empty string otherwise. */
    wxString GetPath() const { return m_path; }

    /*! Returns parent-relative display name as shown in the explorer view */
    wxString GetDisplayName() const { return m_displayName; }

    /** 
        Returns a combination of SFGAO_FILESYSTEM, SFGAO_FOLDER, SFGAO_LINK, and SFGAO_STREAM for the item.
        
        @see IsFile(), IsDirectory(), IsFileSystem(), IsFolder, IsVirtualZipDirectory(), IsShortcut()
    */
    wxUint32 GetSFGAO() const { return m_SFGAO; }

    /*! Returns true if the item is a filesystem file. */
    bool IsFile() const { return GetType() == File; }

    /*! Returns true if the item is a filesystem folder. */
    bool IsDirectory() const { return GetType() == Directory; }

    /*! Returns true if the item is a filesystem file or folder. */
    bool IsFileSystem() const { return (IsFile() || IsDirectory()); }

    /*! Returns true if the item is Directory or a virtual folder. */
    bool IsFolder() const { return (GetSFGAO() & 0x20000000L); } // 0x20000000L = SFGAO_FOLDER

    /** 
        Returns true if the item is a virtual zip directory, i.e.,
        a filesystem file that can also be browsed as a virtual folder. 
    */
    bool IsVirtualZipDirectory() const { return IsFile() && IsFolder(); }

    /*! Returns true if the item is a shortcut. */
    bool IsShortcut() const { return (GetSFGAO() & 0x00010000); } // 0x00010000 = SFGAO_LINK
   
    /*! Sets item's type. */
    void SetType(Type type) { m_type = type; }
    
    /*! Sets item's path. */
    void SetPath(const wxString& path) { m_path = path; }
    
    /*! Sets item's display name. */
    void SetDisplayName(const wxString& name) { m_displayName = name; }
    
    /*! Sets item's SFGAO attributes. */
    void SetSFGAO(wxUint32 attr) { m_SFGAO = attr; }
private:
    Type m_type;
    wxString m_path;
    wxString m_displayName;
    wxUint32 m_SFGAO;
};

/**
    Default name for wxExplorerBrowser.
*/
extern const char wxExplorerBrowserNameStr[];

/**
    A wxWidgets control that hosts [IExplorerBrowser](https://msdn.microsoft.com/en-us/library/windows/desktop/bb761909).

    Requires Windows Vista or newer.

    Known limitations:
    Filtering does not work at all for folders that are part of Windows libraries.

    @see wxExplorerBrowserItem, wxExplorerBrowserEvent
*/
class wxExplorerBrowser : public wxPanel
{
public:
    /** 
        Same as [EXPLORER_BROWSER_OPTIONS](https://msdn.microsoft.com/en-us/library/windows/desktop/bb762501)
    */
    enum Options
    {
        EBO_NONE                = 0x00000000,
        EBO_NAVIGATEONCE        = 0x00000001,
        EBO_SHOWFRAMES          = 0x00000002,
        EBO_ALWAYSNAVIGATE      = 0x00000004,
        EBO_NOTRAVELLOG         = 0x00000008,
        EBO_NOWRAPPERWINDOW     = 0x00000010,
        EBO_HTMLSHAREPOINTVIEW  = 0x00000020,
        EBO_NOBORDER            = 0x00000040,
        EBO_NOPERSISTVIEWSTATE  = 0x00000080
    };
    
    /** 
        Same as [FOLDERVIEWMODE](https://msdn.microsoft.com/en-us/library/windows/desktop/bb762510)
    */
    enum ViewMode
    {
        FVM_AUTO        = -1,
        FVM_ICON        = 1,
        FVM_SMALLICON   = 2,
        FVM_LIST        = 3,
        FVM_DETAILS     = 4,
        FVM_THUMBNAIL   = 5,
        FVM_TILE        = 6,
        FVM_THUMBSTRIP  = 7,
        FVM_CONTENT     = 8,
    };
    
    /** 
        Same as [FOLDERFLAGS](https://msdn.microsoft.com/en-us/library/windows/desktop/bb762508)
    */
    enum FolderFlags
    {
        FWF_NONE                 = 0x00000000,
        FWF_AUTOARRANGE          = 0x00000001,
        FWF_ABBREVIATEDNAMES     = 0x00000002,
        FWF_SNAPTOGRID           = 0x00000004,
        FWF_OWNERDATA            = 0x00000008,
        FWF_BESTFITWINDOW        = 0x00000010,
        FWF_DESKTOP              = 0x00000020,
        FWF_SINGLESEL            = 0x00000040,
        FWF_NOSUBFOLDERS         = 0x00000080,
        FWF_TRANSPARENT          = 0x00000100,
        FWF_NOCLIENTEDGE         = 0x00000200,
        FWF_NOSCROLL             = 0x00000400,
        FWF_ALIGNLEFT            = 0x00000800,
        FWF_NOICONS              = 0x00001000,
        FWF_SHOWSELALWAYS        = 0x00002000,
        FWF_NOVISIBLE            = 0x00004000,
        FWF_SINGLECLICKACTIVATE  = 0x00008000,
        FWF_NOWEBVIEW            = 0x00010000,
        FWF_HIDEFILENAMES        = 0x00020000,
        FWF_CHECKSELECT          = 0x00040000,
        FWF_NOENUMREFRESH        = 0x00080000,
        FWF_NOGROUPING           = 0x00100000,
        FWF_FULLROWSELECT        = 0x00200000,
        FWF_NOFILTERS            = 0x00400000,
        FWF_NOCOLUMNHEADER       = 0x00800000,
        FWF_NOHEADERINALLVIEWS   = 0x01000000,
        FWF_EXTENDEDTILES        = 0x02000000,
        FWF_TRICHECKSELECT       = 0x04000000,
        FWF_AUTOCHECKSELECT      = 0x08000000,
        FWF_NOBROWSERVIEWSTATE   = 0x10000000,
        FWF_SUBSETGROUPS         = 0x20000000,
        FWF_USESEARCHFOLDER      = 0x40000000,
        FWF_ALLOWRTLREADING      = 0x80000000
    };

    /** 
        Same as [FOLDERSETTINGS](https://msdn.microsoft.com/en-us/library/windows/desktop/bb773308)
    */
    struct FolderSettings
    {
        wxUint32 m_viewMode;
        wxUint32 m_flags;

        FolderSettings() 
            : m_viewMode(static_cast<wxUint32>(FVM_AUTO)), m_flags(FWF_NONE) {}        
    };

    /** 
        Individual explorer panes, see [here](https://msdn.microsoft.com/en-us/library/windows/desktop/bb761856)
        for descriptions of individual panes.

        @see PaneState, PaneSettings
    */
    enum PaneID
    {
        EP_NavPane = 0,
        EP_Commands,
        EP_Commands_Organize,
        EP_Commands_View,
        EP_DetailsPane,
        EP_PreviewPane,
        EP_QueryPane,
        EP_AdvQueryPane,
        EP_StatusBar,
        EP_Ribbon     
    };

    /** 
        Same as [EXPLORERPANESTATE](https://msdn.microsoft.com/en-us/library/windows/desktop/bb762580).

        @see PaneID, PaneSettings
    */
     enum PaneState
     { 
        EPS_DONTCARE      = 0x0000,
        EPS_DEFAULT_ON    = 0x0001,
        EPS_DEFAULT_OFF   = 0x0002,
        EPS_STATEMASK     = 0xFFFF,
        EPS_INITIALSTATE  = 0x00010000,
        EPS_FORCE         = 0x00020000
    };
     /** 
        @class PaneSettings
        Manages visiblity of individual explorer panes.
        
        @see PaneID, PaneState, wxExplorerBrowser::CreateStruct
     */
     class PaneSettings
     {
     public:
         PaneSettings() { memset(&m_data, EPS_DONTCARE, sizeof(m_data)); }

         /** See PaneState for possible values of flags. */
         void SetFlags(PaneID pane, wxUint32 flags) { m_data[pane] = flags; }
         
         /** See PaneState for possible values of flags. */
         wxUint32 GetFlags(PaneID pane) const { return m_data[pane]; }
     private:
         wxUint32 m_data[EP_Ribbon+1];
     };
     
     /**
        Determines the parameters of newly created hosted ExplorerBrowser.
     */
     struct CreateStruct
     {        
         wxUint32 options;   /*!< see the Options enum  */
         FolderSettings folderSettings;
         PaneSettings paneSettings;

         CreateStruct() : options(EBO_NOBORDER | EBO_SHOWFRAMES) {}
     };

     enum BrowseTarget
     {
        Parent,         /*!< Go the parent of the current folder. */  
        HistoryBack,    /*!< Go back in the browsing history. */  
        HistoryForward  /*!< Go forward in the browsing history. */     
     };
     
     /** 
        The default constructor, the control is not created until Create() is called.
     */
     wxExplorerBrowser() {}
     ~wxExplorerBrowser();

    wxExplorerBrowser(wxWindow* parent, const CreateStruct& createStruct,                      
                      const wxString& path = wxEmptyString,
                      wxWindowID id = wxID_ANY,
                      const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                      const wxString& name = wxExplorerBrowserNameStr);

    bool Create(wxWindow* parent, const CreateStruct& createStruct,                
                const wxString& path = wxEmptyString,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                const wxString& name = wxExplorerBrowserNameStr);

    bool SetFolderSettings(const FolderSettings& folderSettings);

    /**
        See Options for possible values of @a options.
    */
    bool GetOptions(wxUint32& options);
    
    /**
        See Options for possible values of @a options.
    */
    bool SetOptions(wxUint32 options);
    
    /**
        String @a text will be shown when the view does not contain any items.
    */
    bool SetEmptyText(const wxString& text);    
    
    /**
        The view settings will be stored in the Registry under the name @a bag.
    */
    bool SetPropertyBag(const wxString& bag);   
    
    /**
        Will change the curent folder to the @a item.
        Item can be an absolute filesystem path or another string
        ::SHParseDisplayName can understand, such as 
        "::{20D04FE0-3AEA-1069-A2D8-08002B30309D}" for My Computer.

        if @keepWordWheelText then any search text entered in the Search box in Windows Explorer
         will be be preserved during the navigation, i.e., the items at the new location will be
         filtered in the same way they were filtered at the previous location,
    */
    bool BrowseTo(const wxString& item, bool keepWordWheelText = false);
    
    /**
        Will change the curent folder to the @a target. 
        For explanation of @a keepWordWheelText see the other variant
        of this method.
    */
    bool BrowseTo(BrowseTarget target, bool keepWordWheelText = false);
    
    /**  Refreshes the current folder view. */
    bool Refresh();

    /** Returns the folder the contents of which is currently displayed. */
    bool GetFolder(wxExplorerBrowserItem& item);

    /** Displays the result of search for the curent folder and
        its subfolders. @a str can be anything Search in Explorer understands.  
        Call with an empty string to cancel search.
    */
    bool SearchFolder(const wxString& str);
    
    /** Removes all items from the results folder */
    bool RemoveAll();    

    /**
        Items' type is ignored here, items' path or display name must be relative to the current folder.
        Items that were already selected keep their selection.
        If @a notTakeFocus is true, the folder view will not be focused.
    */
    bool SelectItems(const wxExplorerBrowserItem::List& items, bool notTakeFocus = true);
    
    /**
        Deselects all the selected items in the current folder.
        If @a notTakeFocus is true, the folder view will not be focused.
    */
    bool DeselectAllItems(bool notTakeFocus = true);

    /** 
        Returns selected items that match @a itemTypes.
    */
    bool GetSelectedItems(wxExplorerBrowserItem::List& items,
                          wxUint32 itemTypes = wxExplorerBrowserItem::File);

    /** 
        Returns all items in the current folder that match @a itemTypes.        
    */
    bool GetAllItems(wxExplorerBrowserItem::List& items,
                     wxUint32 itemTypes = wxExplorerBrowserItem::File);
                
    /**
        An item of @a fileMasks should contain a single wild-card mask such as "*.jpg" or "budget201*.*".
        The filter will be applied only on items with their type matching @a itemTypes.

        @bug Unfortunately filtering does not work for query-backed views such as libraries or search results.        
    */
    bool SetFilter(const wxArrayString& fileMasks, wxUint32 itemTypes = wxExplorerBrowserItem::File);
    
    /** 
        Clears the filter. @see SetFilter()
    */
    bool RemoveFilter();

    /** 
        Sets pane settings. 
        The change in settings will be reflected only when the view changes,
        e.g. a different folder was navigated to.
    */
    bool SetPaneSettings(const PaneSettings& settings);

    /** 
        Returns the actual IExplorerBrowser pointer
        or nullptr if the interface was not created.
    */
    void* GetIExplorerBrowser();
    
    /** @private 
        Overriding is necessary for ExplorerBrowser shortcuts like <Ctrl+A>
        or <F2> keep working while being hosted.
    */
    bool MSWTranslateMessage(WXMSG* msg) override;         
private:    
    // use the pimpl idiom so MSW headers are not included from a public header
    class wxExplorerBrowserImpl;    
    wxExplorerBrowserImpl* m_impl { nullptr };
  
    // the only child of the panel which is
    // the actual parent of ExploreBrowser control
    wxWindow* m_host {nullptr};

    void OnSize(wxSizeEvent& evt);
    void OnPaint(wxPaintEvent& evt);

    wxDECLARE_DYNAMIC_CLASS(wxExplorerBrowser);
};

/**
    All the wxExplorerBrowserEvents are those sent only for the items of the folder view,
    see [IExplorerBrowserEvents](https://msdn.microsoft.com/en-us/library/windows/desktop/bb761883).
    I.e., the events for the navigation panel hosting favorites and the folder tree 
    ([INameSpaceTreeControlEvents](https://msdn.microsoft.com/en-us/library/windows/desktop/bb761567)
    are not sent. 

    For some of the events which may concern selected items, the event's
    item contains only the first selected item of possible many.
    Use wxIExplorerBrowser::GetSelectedItems() to obtain all selected items.

    @b wxEVT_EXPLORER_BROWSER_DEFAULT_COMMAND    
    Sent before the default action is taken on the selected item(s), can be vetoed.
    
    @b wxEVT_EXPLORER_BROWSER_SELECTION_CHANGED    
    Sent after the selection was changed. If the current selection is empty, 
    event's item type will be Unknown and the item will not contain any other information.    
    
    @b wxEVT_EXPLORER_BROWSER_CONTEXTMENU_START    
    Sent before the shell context menu for the selected item(s) is shown, can be vetoed.
    
    @b wxEVT_EXPLORER_BROWSER_NAVIGATING    
    Sent before the folder is changed, can be vetoed. 
    Event's item contains the folder to which the view is navigating.
    
    @b wxEVT_EXPLORER_BROWSER_NAVIGATION_COMPLETE
    Sent after the folder was changed. 
    
    @b wxEVT_EXPLORER_BROWSER_NAVIGATION_FAILED
    Sent when the folder could not be changed, e.g., navigating was vetoed or the folder is not available.
    
    @b wxEVT_EXPLORER_BROWSER_VIEW_CREATED
    Sent when the new view for a folder was created.    

    @see wxExplorerBrowserItem, wxExplorerBrowser, wxNotifyEvent::Veto()

*/
class wxExplorerBrowserEvent: public wxNotifyEvent
{
public:
    wxExplorerBrowserEvent(wxEventType command = wxEVT_NULL, int id = 0)
        : wxNotifyEvent(command, id) {}

    const wxExplorerBrowserItem& GetItem() const { return m_item; }
    void SetItem(const wxExplorerBrowserItem& item) { m_item = item; }

    wxEvent* Clone() const override { return new wxExplorerBrowserEvent(*this); }
private:
    wxExplorerBrowserItem m_item;

    wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxExplorerBrowserEvent);
};

wxDECLARE_EVENT(wxEVT_EXPLORER_BROWSER_DEFAULT_COMMAND, wxExplorerBrowserEvent);
wxDECLARE_EVENT(wxEVT_EXPLORER_BROWSER_SELECTION_CHANGED, wxExplorerBrowserEvent);
wxDECLARE_EVENT(wxEVT_EXPLORER_BROWSER_CONTEXTMENU_START, wxExplorerBrowserEvent);
wxDECLARE_EVENT(wxEVT_EXPLORER_BROWSER_NAVIGATING, wxExplorerBrowserEvent);
wxDECLARE_EVENT(wxEVT_EXPLORER_BROWSER_NAVIGATION_COMPLETE, wxExplorerBrowserEvent);
wxDECLARE_EVENT(wxEVT_EXPLORER_BROWSER_NAVIGATION_FAILED, wxExplorerBrowserEvent);
wxDECLARE_EVENT(wxEVT_EXPLORER_BROWSER_VIEW_CREATED, wxExplorerBrowserEvent);

#define wxExplorerBrowserEventHandler(func) (&func)

typedef void (wxEvtHandler::*wxExplorerBrowserEventFunction)(wxExplorerBrowserEvent&);

#define EXPLORER_DEFAULT_COMMAND(id, func) \
    wx__DECLARE_EVT1(wxEVT_EXPLORER_BROWSER_DEFAULT_COMMAND, id, wxExplorerBrowserEventHandler(func))
#define EXPLORER_BROWSER_SELECTION_CHANGED(id, func) \
    wx__DECLARE_EVT1(wxEVT_EXPLORER_BROWSER_SELECTION_CHANGED, id, wxExplorerBrowserEventHandler(func))
#define EVT_EXPLORER_BROWSER_CONTEXTMENU_START(id, func) \
    wx__DECLARE_EVT1(wxEVT_EXPLORER_BROWSER_CONTEXTMENU_START, id, wxExplorerBrowserEventHandler(func))
#define EXPLORER_BROWSER_NAVIGATING(id, func) \
    wx__DECLARE_EVT1(wxEVT_EXPLORER_BROWSER_NAVIGATING, id, wxExplorerBrowserEventHandler(func))
#define EXPLORER_BROWSER_NAVIGATION_COMPLETE(id, func) \
    wx__DECLARE_EVT1(wxEVT_EXPLORER_BROWSER_NAVIGATION_COMPLETE, id, wxExplorerBrowserEventHandler(func))
#define EXPLORER_BROWSER_NAVIGATION_FAILED(id, func) \
    wx__DECLARE_EVT1(wxEVT_EXPLORER_BROWSER_NAVIGATION_FAILED, id, wxExplorerBrowserEventHandler(func))
#define EXPLORER_BROWSER_VIEW_CREATED(id, func) \
    wx__DECLARE_EVT1(wxEVT_EXPLORER_BROWSER_VIEW_CREATED, id, wxExplorerBrowserEventHandler(func))

#endif //ifndef WX_EXPLORER_BROWSER_H_DEFINED