//
//  PluginListWindow.cpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#include "PluginListWindow.hpp"
#include "AppProperties.hpp"

PluginListWindow::PluginListWindow(KnownPluginList& list, AudioPluginFormatManager& pluginFormatManager)
        : DocumentWindow("Available plugins", Colours::white,
            DocumentWindow::minimiseButton | DocumentWindow::closeButton)
{
    const File deadMansPedalFile(AppProperties::get()->getFile().getSiblingFile("RecentlyCrashedPluginsList"));

    list.addChangeListener(this);
    setContentOwned(new PluginListComponent(pluginFormatManager,
            list,
            deadMansPedalFile,
            AppProperties::get()), true);

    setUsingNativeTitleBar(true);
    setResizable(true, false);
    setResizeLimits(300, 400, 800, 1500);
    setTopLeftPosition(60, 60);

    restoreWindowStateFromString(AppProperties::get()->getValue("listWindowPos"));
}

PluginListWindow::~PluginListWindow() {
    AppProperties::get()->setValue("listWindowPos", getWindowStateAsString());
    clearContentComponent();
}

void PluginListWindow::closeButtonPressed() {
    this->setVisible(false);
}

void PluginListWindow::changeListenerCallback(ChangeBroadcaster* changed)
{
    if (auto knownPluginList = dynamic_cast<KnownPluginList*>(changed)) {
        MessageManager::callAsync([knownPluginList]{
            std::unique_ptr<XmlElement> savedPluginList (knownPluginList->createXml());

            if (savedPluginList != nullptr) {
                AppProperties::get()->setValue ("pluginList", savedPluginList.get());
                AppProperties::saveIfNeeded();
            }

        });
    }
}

bool PluginListWindow::keyPressed(const KeyPress& key)
{
    if (key.getKeyCode() == KeyPress::escapeKey) {
        closeButtonPressed();
        return true;
    }
    return false;
}
