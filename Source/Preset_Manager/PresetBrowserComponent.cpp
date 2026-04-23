#include "PresetBrowserComponent.h"

// =============================================================================
//  Colours
// =============================================================================
const juce::Colour PresetBrowserColours::background      { 0xFF1E1E1E };
const juce::Colour PresetBrowserColours::navBar          { 0xFF2A2A2A };
const juce::Colour PresetBrowserColours::navArrow        { 0xFFAAAAAA };
const juce::Colour PresetBrowserColours::navText         { 0xFFDDDDDD };
const juce::Colour PresetBrowserColours::toolbarButton   { 0xFF333333 };
const juce::Colour PresetBrowserColours::toolbarIcon     { 0xFFBBBBBB };
const juce::Colour PresetBrowserColours::sectionHeader   { 0xFF888888 };
const juce::Colour PresetBrowserColours::rowNormal       { 0xFF252525 };
const juce::Colour PresetBrowserColours::rowHover        { 0xFF2E2E2E };
const juce::Colour PresetBrowserColours::rowSelected     { 0xFF3A5F8A };
const juce::Colour PresetBrowserColours::rowTextNormal   { 0xFFCCCCCC };
const juce::Colour PresetBrowserColours::rowTextSelected { 0xFFFFFFFF };
const juce::Colour PresetBrowserColours::separator       { 0xFF444444 };

// =============================================================================
//  NavBar
// =============================================================================
PresetBrowserNavBar::PresetBrowserNavBar (PresetManager& pm,
                                          PresetBrowserConfig config)
    : presetManager_ (pm)
    , config_ (config)
{
    // Must be explicitly set — JUCE components default to opaque,
    // which would paint a solid background over your plugin surface.
    setOpaque (false);

    presetManager_.addChangeListener (this);

    addAndMakeVisible (prevButton_);
    prevButton_.setButtonText ("<");
    prevButton_.onClick = [this] { presetManager_.loadPreviousPreset(); };

    addAndMakeVisible (nextButton_);
    nextButton_.setButtonText (">");
    nextButton_.onClick = [this] { presetManager_.loadNextPreset(); };

    addAndMakeVisible (nameButton_);
    nameButton_.onClick = [this] { openPopup(); };

    for (auto* b : { &prevButton_, &nextButton_, &nameButton_ })
    {
        b->setColour (juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        b->setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff2a2520));  // plugin accent — visible on light bg
        b->setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff2a2520));
        b->setColour (juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
    }

    updateLabel();
}

PresetBrowserNavBar::~PresetBrowserNavBar()
{
    presetManager_.removeChangeListener (this);
}

void PresetBrowserNavBar::paint (juce::Graphics& g)
{
    // Rounded outline
    g.setColour (juce::Colour (0xff888888));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);

    // Draw preset name directly — bypasses TextButton font/truncation issues
    auto nameArea = getLocalBounds()
                        .withTrimmedLeft  (getHeight())
                        .withTrimmedRight (getHeight());
    g.setFont (juce::Font (13.0f));
    g.setColour (juce::Colour (0xff2a2520));
    g.drawText (presetManager_.getCurrentPresetName(), nameArea, juce::Justification::centred, true);
}

void PresetBrowserNavBar::resized()
{
    auto r       = getLocalBounds();
    int  btnSize = r.getHeight();   // arrow buttons are square, matching bar height
    prevButton_.setBounds (r.removeFromLeft  (btnSize));
    nextButton_.setBounds (r.removeFromRight (btnSize));
    nameButton_.setBounds (r);
}

void PresetBrowserNavBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateLabel();
}

void PresetBrowserNavBar::updateLabel()
{
    nameButton_.setButtonText ({});  // text drawn directly in paint()
    repaint();
}

void PresetBrowserNavBar::openPopup()
{
    if (popup_ != nullptr) { closePopup(); return; }

    popup_ = std::make_unique<PresetBrowserPopup> (presetManager_, *this, config_);

    auto* top  = getTopLevelComponent();
    int popupW = juce::jmax (getWidth(), config_.popupWidth);
    int popupH = config_.popupHeight;

    auto navTopLeft = top->getLocalPoint (this, juce::Point<int> (0, 0));
    int spaceBelow  = top->getHeight() - (navTopLeft.y + getHeight());
    int spaceAbove  = navTopLeft.y;

    int yPos;
    if (spaceBelow >= popupH)
        yPos = navTopLeft.y + getHeight();
    else if (spaceAbove >= popupH)
        yPos = navTopLeft.y - popupH;
    else
        yPos = (spaceBelow >= spaceAbove)
                   ? navTopLeft.y + getHeight()
                   : navTopLeft.y - popupH;

    yPos = juce::jlimit (0, juce::jmax (0, top->getHeight() - popupH), yPos);

    popup_->setBounds (navTopLeft.x, yPos, popupW, popupH);
    top->addAndMakeVisible (*popup_);
    popup_->toFront (true);
}

void PresetBrowserNavBar::closePopup()
{
    popup_.reset();
}

// =============================================================================
//  PresetBrowserPopup
// =============================================================================
PresetBrowserPopup::PresetBrowserPopup (PresetManager& pm,
                                        juce::Component& anchor,
                                        const PresetBrowserConfig& config)
    : presetManager_ (pm)
    , anchor_ (anchor)
    , config_ (config)
{
    presetManager_.addChangeListener (this);
    setOpaque (true);

    for (auto* b : { &saveAsBtn_, &refreshBtn_, &revealBtn_, &resetBtn_, &deleteBtn_ })
        styleToolbarButton (*b);

    saveAsBtn_ .onClick = [this] { saveAs(); };
    refreshBtn_.onClick = [this] { refreshList(); };
    revealBtn_ .onClick = [this] { presetManager_.revealUserPresetsFolder(); };
    resetBtn_  .onClick = [this] { presetManager_.resetToDefault(); };
    deleteBtn_ .onClick = [this] { confirmDelete(); };

    updateDeleteButton();

    listBox_ = std::make_unique<PresetListBox> (presetManager_, *this, config_);
    addAndMakeVisible (*listBox_);
    refreshList();

    juce::Desktop::getInstance().addGlobalMouseListener (this);
}

PresetBrowserPopup::~PresetBrowserPopup()
{
    presetManager_.removeChangeListener (this);
    juce::Desktop::getInstance().removeGlobalMouseListener (this);
}

void PresetBrowserPopup::mouseDown (const juce::MouseEvent& e)
{
    if (! getScreenBounds().contains (e.getScreenPosition()))
        if (auto* nav = dynamic_cast<PresetBrowserNavBar*> (&anchor_))
            nav->closePopup();
}

void PresetBrowserPopup::paint (juce::Graphics& g)
{
    g.fillAll (PresetBrowserColours::background);

    g.setColour (PresetBrowserColours::navBar);
    g.fillRect (getLocalBounds().removeFromTop (config_.toolbarHeight));

    g.setColour (PresetBrowserColours::separator);
    g.drawRect (getLocalBounds());
}

void PresetBrowserPopup::resized()
{
    auto r     = getLocalBounds();
    auto tools = r.removeFromTop (config_.toolbarHeight).reduced (4, 4);

    // Four equal text buttons + delete button on the right (wider so "Del" fits)
    int deleteW = 48;
    int btnW    = (tools.getWidth() - deleteW - 8) / 4;

    saveAsBtn_ .setBounds (tools.removeFromLeft (btnW).reduced (2, 0));
    refreshBtn_.setBounds (tools.removeFromLeft (btnW).reduced (2, 0));
    revealBtn_ .setBounds (tools.removeFromLeft (btnW).reduced (2, 0));
    resetBtn_  .setBounds (tools.removeFromLeft (btnW).reduced (2, 0));
    tools.removeFromLeft (8);
    deleteBtn_ .setBounds (tools.removeFromLeft (deleteW).reduced (2, 0));

    listBox_->setBounds (r);
}

void PresetBrowserPopup::changeListenerCallback (juce::ChangeBroadcaster*)
{
    listBox_->refresh();
    updateDeleteButton();
}

void PresetBrowserPopup::updateDeleteButton()
{
    auto name      = presetManager_.getCurrentPresetName();
    bool canDelete = presetManager_.isUserPreset (name);
    deleteBtn_.setEnabled (canDelete);
    deleteBtn_.setAlpha (canDelete ? 1.0f : 0.35f);
}

void PresetBrowserPopup::confirmDelete()
{
    auto name = presetManager_.getCurrentPresetName();
    if (! presetManager_.isUserPreset (name)) return;

    juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::WarningIcon,
        "Delete Preset",
        "Delete \"" + name + "\"? This cannot be undone.",
        "Delete", "Cancel", nullptr,
        juce::ModalCallbackFunction::create ([this, name] (int result)
        {
            if (result == 1)
            {
                presetManager_.deletePreset (name);
                refreshList();
            }
        }));
}

void PresetBrowserPopup::presetChosen (const juce::String& name)
{
    presetManager_.loadPreset (name);
}

void PresetBrowserPopup::refreshList()
{
    listBox_->refresh();
    listBox_->selectCurrentPreset();
}

void PresetBrowserPopup::saveAs()
{
    auto* dlg = new juce::AlertWindow ("Save Preset",
                                       "Enter a name for the new preset:",
                                       juce::MessageBoxIconType::NoIcon);
    dlg->addTextEditor ("name", presetManager_.getCurrentPresetName());
    dlg->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    dlg->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dlg->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dlg] (int result)
        {
            if (result == 1)
            {
                auto name = dlg->getTextEditorContents ("name").trim();
                if (name.isNotEmpty())
                {
                    presetManager_.saveAsPreset (name);
                    refreshList();
                }
            }
        }), true);
}

void PresetBrowserPopup::styleToolbarButton (juce::TextButton& btn)
{
    addAndMakeVisible (btn);
    btn.setColour (juce::TextButton::buttonColourId,  PresetBrowserColours::toolbarButton);
    btn.setColour (juce::TextButton::textColourOffId, PresetBrowserColours::toolbarIcon);
    btn.setColour (juce::TextButton::textColourOnId,  PresetBrowserColours::toolbarIcon);
    btn.setColour (juce::ComboBox::outlineColourId,   juce::Colours::transparentBlack);
}

// =============================================================================
//  PresetListBox
// =============================================================================
PresetListBox::PresetListBox (PresetManager& pm,
                              PresetBrowserPopup& popup,
                              const PresetBrowserConfig& config)
    : presetManager_ (pm)
    , popup_ (popup)
    , config_ (config)
{
    listBox_.setModel (this);
    listBox_.setRowHeight (config_.rowHeight);
    listBox_.setColour (juce::ListBox::backgroundColourId,
                        PresetBrowserColours::background);
    listBox_.setColour (juce::ListBox::outlineColourId,
                        juce::Colours::transparentBlack);
    addAndMakeVisible (listBox_);
    buildRows();
}

void PresetListBox::refresh()
{
    buildRows();
    listBox_.updateContent();
    selectCurrentPreset();
}

void PresetListBox::buildRows()
{
    rows_.clear();

    auto factory = presetManager_.getFactoryPresetNames();
    auto user    = presetManager_.getUserPresetNames();

    if (factory.size() > 0)
    {
        rows_.add ({ "FACTORY PRESETS", true, true });
        for (auto& n : factory)
            rows_.add ({ n, false, true });
    }

    if (user.size() > 0)
    {
        rows_.add ({ "USER PRESETS", true, false });
        for (auto& n : user)
            rows_.add ({ n, false, false });
    }

    if (rows_.isEmpty())
        rows_.add ({ "No presets found", true, false });
}

int PresetListBox::getNumRows()
{
    return rows_.size();
}

void PresetListBox::paintListBoxItem (int rowIndex,
                                      juce::Graphics& g,
                                      int width, int height,
                                      bool selected)
{
    if (! juce::isPositiveAndBelow (rowIndex, rows_.size())) return;

    const auto& row = rows_[rowIndex];

    if (row.isHeader)
    {
        g.fillAll (PresetBrowserColours::background);
        g.setColour (PresetBrowserColours::separator);
        g.fillRect (0, height - 1, width, 1);
        g.setColour (PresetBrowserColours::sectionHeader);
        g.setFont (juce::Font (config_.fontSizeHeader, juce::Font::bold));
        g.drawText (row.name, 10, 0, width - 10, height,
                    juce::Justification::centredLeft);
        return;
    }

    g.fillAll (selected ? PresetBrowserColours::rowSelected
                        : PresetBrowserColours::rowNormal);

    g.setColour (selected ? PresetBrowserColours::rowTextSelected
                          : PresetBrowserColours::rowTextNormal);
    g.setFont (juce::Font (config_.fontSizeRow));
    g.drawText (row.name, 18, 0, width - 18, height,
                juce::Justification::centredLeft);

    g.setColour (PresetBrowserColours::separator.withAlpha (0.4f));
    g.fillRect (0, height - 1, width, 1);
}

void PresetListBox::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, rows_.size()) && ! rows_[row].isHeader)
        popup_.presetChosen (rows_[row].name);
}

void PresetListBox::listBoxItemDoubleClicked (int row, const juce::MouseEvent& e)
{
    listBoxItemClicked (row, e);
}

void PresetListBox::resized()
{
    listBox_.setBounds (getLocalBounds());
}

void PresetListBox::selectCurrentPreset()
{
    auto current = presetManager_.getCurrentPresetName();
    for (int i = 0; i < rows_.size(); ++i)
    {
        if (! rows_[i].isHeader && rows_[i].name == current)
        {
            listBox_.selectRow (i, false, true);
            listBox_.scrollToEnsureRowIsOnscreen (i);
            return;
        }
    }
    listBox_.deselectAllRows();
}
