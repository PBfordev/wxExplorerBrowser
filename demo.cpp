////////////////////////////////////////////////////////////////////////////////////
//
//  Name:        wxExplorerBrowser demo
//  Purpose:     Demonstrates how to use wxExplorerBrowser
//  Author:      PB
//  Copyright:   (c) 2018 PB <pbfordev@gmail.com>
//  Licence:     wxWindows licence
//
////////////////////////////////////////////////////////////////////////////////////

#include <wx/wx.h>
#include <wx/artprov.h>
#include <wx/utils.h> 
#include <wx/textdlg.h>
#include <wx/choicdlg.h>

#include "wxExplorerBrowser.h"


wxString wxExplorerBrowserItemToString(const wxExplorerBrowserItem& item)
{
    wxString strType;

    switch ( item.GetType() )
    {
        case wxExplorerBrowserItem::File:      strType = wxS("File"); break;
        case wxExplorerBrowserItem::Directory: strType = wxS("Directory"); break;
        case wxExplorerBrowserItem::Other:     strType = wxS("Other"); break;
        default:                               strType = wxS("Unknown");
    }

    return wxString::Format(wxS("Type \"%s\", Display name \"%s\", Path \"%s\""),
        strType, item.GetDisplayName(), item.GetPath());
}


class MyFrame : public wxFrame
{
public:   
    MyFrame()
        : wxFrame(NULL, wxID_ANY, "wxExplorerBrowser sample", wxDefaultPosition, wxSize(1024, 800))
    {
        wxToolBar* toolbar = CreateToolBar(wxTB_DEFAULT_STYLE | wxTB_TEXT | wxTB_NODIVIDER);

        toolbar->AddTool(wxID_BACKWARD, _("Go Back"), wxArtProvider::GetBitmap(wxART_GO_BACK, wxART_TOOLBAR));
        toolbar->AddTool(wxID_FORWARD, _("Go Forward"), wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_TOOLBAR));
        toolbar->AddTool(wxID_UP, _("Go to Parent"), wxArtProvider::GetBitmap(wxART_GO_DIR_UP, wxART_TOOLBAR));
        toolbar->AddSeparator();

        wxComboBox *filterCombo = new wxComboBox(toolbar, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(180,-1), 0, NULL, wxCB_READONLY);
        filterCombo->Append(_("All"));
        filterCombo->Append(_("Microsoft Word Documents Only"));
        filterCombo->SetSelection(0);        
        toolbar->AddControl(filterCombo, _("Show Files")); 
        toolbar->AddSeparator();

        toolbar->AddTool(wxID_FIND, _("Search"), wxArtProvider::GetBitmap(wxART_FIND, wxART_TOOLBAR));
        toolbar->AddSeparator();

        wxComboBox* defaultActionCombo = new wxComboBox(toolbar, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1,-1), 0, NULL, wxCB_READONLY);
        defaultActionCombo->Append(_("Allow All"));
        defaultActionCombo->Append(_("Ask All"));
        defaultActionCombo->Append(_("Ask for Files Only"));
        defaultActionCombo->SetSelection(0);    
        toolbar->AddControl(defaultActionCombo, _("Default Action"));
        toolbar->AddSeparator();

        toolbar->AddTool(wxID_VIEW_LIST, _("Selected Items"), wxArtProvider::GetBitmap(wxART_TICK_MARK, wxART_TOOLBAR));        
        
        toolbar->Realize();        

        Bind(wxEVT_TOOL, [=](wxCommandEvent&) { m_explorerBrowser->BrowseTo(wxExplorerBrowser::HistoryBack); }, wxID_BACKWARD);
        Bind(wxEVT_TOOL, [=](wxCommandEvent&) { m_explorerBrowser->BrowseTo(wxExplorerBrowser::HistoryForward); }, wxID_FORWARD);
        Bind(wxEVT_TOOL, [=](wxCommandEvent&) { m_explorerBrowser->BrowseTo(wxExplorerBrowser::Parent); }, wxID_UP);
        filterCombo->Bind(wxEVT_COMBOBOX, &MyFrame::OnFilterChanged, this);
        Bind(wxEVT_TOOL, &MyFrame::OnSearch, this, wxID_FIND);
        defaultActionCombo->Bind(wxEVT_COMBOBOX, &MyFrame::OnDefaultActionChanged, this);        
        Bind(wxEVT_TOOL, &MyFrame::OnShowSelectedItems, this, wxID_VIEW_LIST);
          
        wxPanel* mainPanel = new wxPanel(this, wxID_ANY);
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

        wxExplorerBrowser::CreateStruct cs;
                        
        m_explorerBrowser = new wxExplorerBrowser(mainPanel, cs);
        mainSizer->Add(m_explorerBrowser, wxSizerFlags().Proportion(5).Expand().Border());

        m_log = new wxTextCtrl(mainPanel, wxID_ANY, wxEmptyString,
                               wxDefaultPosition, wxDefaultSize,
                               wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);        
        mainSizer->Add(m_log, wxSizerFlags().Proportion(2).Expand().Border());

        mainPanel->SetSizer(mainSizer);
                    
        m_explorerBrowser->Bind(wxEVT_EXPLORER_BROWSER_DEFAULT_COMMAND, &MyFrame::OnExplorerBrowserEvent, this);
        m_explorerBrowser->Bind(wxEVT_EXPLORER_BROWSER_SELECTION_CHANGED, &MyFrame::OnExplorerBrowserEvent, this);
        m_explorerBrowser->Bind(wxEVT_EXPLORER_BROWSER_CONTEXTMENU_START, &MyFrame::OnExplorerBrowserEvent, this);
        m_explorerBrowser->Bind(wxEVT_EXPLORER_BROWSER_NAVIGATING, &MyFrame::OnExplorerBrowserEvent, this);
        m_explorerBrowser->Bind(wxEVT_EXPLORER_BROWSER_NAVIGATION_COMPLETE, &MyFrame::OnExplorerBrowserEvent, this);
        m_explorerBrowser->Bind(wxEVT_EXPLORER_BROWSER_NAVIGATION_FAILED, &MyFrame::OnExplorerBrowserEvent, this);
        m_explorerBrowser->Bind(wxEVT_EXPLORER_BROWSER_VIEW_CREATED, &MyFrame::OnExplorerBrowserEvent, this);
    }	
private:    
    enum DefaultAction
    {
        AllowAll = 0,
        AskAll = 1,
        AskFiles = 2
    };

    wxExplorerBrowser* m_explorerBrowser;
    wxTextCtrl* m_log;

    DefaultAction m_defaultAction = AllowAll;

    void OnFilterChanged(wxCommandEvent& evt)
    {
        if ( evt.GetSelection() == 1 ) // MS Word documents
        {            
            wxArrayString as;            

            as.push_back("*.doc*");
            as.push_back("*.dot*");
            as.push_back("*.wbk");
            as.push_back("*.rtf");
            m_explorerBrowser->SetFilter(as);
        }
        else
            m_explorerBrowser->RemoveFilter();                
    }   

    void OnDefaultActionChanged(wxCommandEvent& evt)
    {        
        switch ( evt.GetSelection() )
        {
            case 0: m_defaultAction = AllowAll; break;
            case 1: m_defaultAction = AskAll; break;
            case 2: m_defaultAction = AskFiles; break;
            default: wxFAIL;        
        }
    }

    void OnShowSelectedItems(wxCommandEvent&)
    {
        wxExplorerBrowserItem::List items;

        if ( !m_explorerBrowser->GetSelectedItems(items,
                wxExplorerBrowserItem::File
                    | wxExplorerBrowserItem::Directory
                    | wxExplorerBrowserItem::Other)
            )
        {
            wxLogError(_("Could not get selected items."));
            return;
        }

        if ( items.empty() )
        {
            wxLogMessage(_("There are no selected items."));
            return;
        }

        wxArrayString itemInfos;

        itemInfos.reserve(items.size());
        for ( size_t i = 0; i < items.size(); ++i )
            itemInfos.push_back(wxExplorerBrowserItemToString(items[i]));

        wxGetSingleChoice(wxString::Format(_("%zu selected items:"), items.size()),
            _("Selected items"), itemInfos, 0, this);
    }   

    void OnSearch(wxCommandEvent&)
    {
        static wxString searchStr;

        searchStr = wxGetTextFromUser(_("Enter search string, empty strings cancels the searching)"), _("Search"), searchStr);        
        m_explorerBrowser->SearchFolder(searchStr);       
    }   

    void OnExplorerBrowserEvent(wxExplorerBrowserEvent& evt)
    {
        const wxEventType command = evt.GetEventType();
        const wxExplorerBrowserItem item = evt.GetItem();
        wxString evtString;

        if ( command == wxEVT_EXPLORER_BROWSER_DEFAULT_COMMAND )
        {
            evtString = "wxEVT_EXPLORER_BROWSER_DEFAULT_COMMAND";
            
            if ( m_defaultAction == AskAll 
                 || (m_defaultAction == AskFiles && item.IsFile()) // for simplicity sake, check only the first item
                )       
            {
                if ( wxMessageBox(_("Allow default action for selected item(s)?"),
                        _("Confirm"), wxYES_NO, this) == wxNO )
                {
                    evt.Veto();
                }
            }
        }
        else
        if ( command == wxEVT_EXPLORER_BROWSER_SELECTION_CHANGED )
        {
            evtString = "wxEVT_EXPLORER_BROWSER_SELECTION_CHANGED";
        }
        else
        if ( command == wxEVT_EXPLORER_BROWSER_CONTEXTMENU_START )
        {
            evtString = "wxEVT_EXPLORER_BROWSER_CONTEXTMENU_START";
        }
        else
        if ( command == wxEVT_EXPLORER_BROWSER_NAVIGATING )
        {
            evtString = "wxEVT_EXPLORER_BROWSER_NAVIGATING";            
        }
        else
        if ( command == wxEVT_EXPLORER_BROWSER_NAVIGATION_COMPLETE )
        {
            evtString = "wxEVT_EXPLORER_BROWSER_NAVIGATION_COMPLETE";
        }
        else
        if ( command == wxEVT_EXPLORER_BROWSER_NAVIGATION_FAILED )
        {
            evtString = "wxEVT_EXPLORER_BROWSER_NAVIGATION_FAILED";
        }
        else
        if ( command == wxEVT_EXPLORER_BROWSER_VIEW_CREATED )
        {
            evtString = "wxEVT_EXPLORER_BROWSER_VIEW_CREATED";
        }
        else
        {
            evtString = "Unknown event!";
        }

        m_log->AppendText(wxString::Format("%s: %s\n", evtString, wxExplorerBrowserItemToString(item)));
    }
};


class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        if ( !wxCheckOsVersion(6) )
        {
            wxLogError("wxExplorerBrowser can be used only on Windows Vista or newer.");
            return false;
        }
        
        (new MyFrame())->Show();
        return true;
    }
}; wxIMPLEMENT_APP(MyApp);