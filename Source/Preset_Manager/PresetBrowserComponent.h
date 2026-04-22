#pragma once
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetManager.h"

/**
 * PresetBrowserComponent
 * ======================
 * A self-contained, floating preset browser: dark chrome, navigation arrows,
 * Save As / Refresh / Folder / Reset toolbar, scrollable Factory / User list.
 *
 * Customise sizing by passing a PresetBrowserConfig to PresetBrowserNavBar.
 *
 * Minimal integration:
 * --------------------
 *   // PluginEditor.h
 *   PresetBrowserNavBar navBar_;
 *
 *   // PluginEditor constructor
 *   , navBar_ (*p.presetManager)          // default config
 *   // — or —
 *   , navBar_ (*p.presetManager, { .navBarHeight = 40, .popupHeight = 500 })
 *   addAndMakeVisible (navBar_);
 *
 *   // PluginEditor::resized()
 *   navBar_.setBounds (getLocalBounds().removeFromTop (config.navBarHeight));
 */

// =============================================================================
//  Forward declarations
// =============================================================================
class PresetListBox;
class PresetBrowserPopup;

// =============================================================================
//  PresetBrowserConfig  — all sizing in one place
// =============================================================================
struct PresetBrowserConfig
{
    // Nav bar
    int   navBarHeight   = 36;    // height of the strip inside your editor

    // Popup panel
    int   popupWidth     = 320;   // minimum width (expands to match nav bar if wider)
    int   popupHeight    = 420;   // total height of the floating panel

    // Toolbar inside the popup
    int   toolbarHeight  = 40;    // height of the Save As / Refresh / ... row

    // List rows
    int   rowHeight      = 28;    // height of each preset row
    int   headerHeight   = 24;    // height of section header rows

    // Typography
    float fontSizeRow    = 13.5f; // preset name text
    float fontSizeHeader = 11.0f; // section header text
    float fontSizeNav    = 13.0f; // preset name in the nav bar
};

// =============================================================================
//  PresetBrowserColours  (edit in PresetBrowserComponent.cpp to retheme)
// =============================================================================
struct PresetBrowserColours
{
    static const juce::Colour background;
    static const juce::Colour navBar;
    static const juce::Colour navArrow;
    static const juce::Colour navText;
    static const juce::Colour toolbarButton;
    static const juce::Colour toolbarIcon;
    static const juce::Colour sectionHeader;
    static const juce::Colour rowNormal;
    static const juce::Colour rowHover;
    static const juce::Colour rowSelected;
    static const juce::Colour rowTextNormal;
    static const juce::Colour rowTextSelected;
    static const juce::Colour separator;
};

// =============================================================================
//  PresetBrowserNavBar  — the thin strip shown inside your editor
// =============================================================================
class PresetBrowserNavBar : public juce::Component,
                            public juce::ChangeListener
{
public:
    explicit PresetBrowserNavBar (PresetManager& pm,
                                  PresetBrowserConfig config = {});
    ~PresetBrowserNavBar() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    const PresetBrowserConfig& getConfig() const { return config_; }

    void closePopup();   // called by PresetBrowserPopup on outside click

private:
    PresetManager&       presetManager_;
    PresetBrowserConfig  config_;

    juce::TextButton prevButton_ { "<" };
    juce::TextButton nextButton_ { ">" };
    juce::TextButton nameButton_;

    std::unique_ptr<PresetBrowserPopup> popup_;

    void openPopup();
    void updateLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserNavBar)
};

// =============================================================================
//  PresetBrowserPopup  — the floating panel
// =============================================================================
class PresetBrowserPopup : public juce::Component,
                           public juce::ChangeListener
{
public:
    PresetBrowserPopup (PresetManager& pm,
                        juce::Component& anchor,
                        const PresetBrowserConfig& config);
    ~PresetBrowserPopup() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void presetChosen (const juce::String& name);
    void mouseDown    (const juce::MouseEvent& e) override;

private:
    PresetManager&            presetManager_;
    juce::Component&          anchor_;
    const PresetBrowserConfig config_;

    juce::TextButton saveAsBtn_  { "Save As" };
    juce::TextButton refreshBtn_ { "Refresh" };
    juce::TextButton revealBtn_  { "Folder"  };
    juce::TextButton resetBtn_   { "Reset"   };
    juce::TextButton deleteBtn_  { "Del" };

    std::unique_ptr<PresetListBox> listBox_;

    void refreshList();
    void saveAs();
    void confirmDelete();
    void updateDeleteButton();
    void styleToolbarButton (juce::TextButton& btn);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserPopup)
};

// =============================================================================
//  PresetListBox  — scrollable grouped list
// =============================================================================
class PresetListBox : public juce::Component,
                      public juce::ListBoxModel
{
public:
    struct Row
    {
        juce::String name;
        bool isHeader  = false;
        bool isFactory = false;
    };

    PresetListBox (PresetManager& pm,
                   PresetBrowserPopup& popup,
                   const PresetBrowserConfig& config);

    void refresh();
    void selectCurrentPreset();

    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&,
                           int w, int h, bool selected) override;
    void listBoxItemClicked       (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void resized() override;

private:
    PresetManager&            presetManager_;
    PresetBrowserPopup&       popup_;
    const PresetBrowserConfig config_;
    juce::ListBox             listBox_;
    juce::Array<Row>          rows_;

    void buildRows();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetListBox)
};
