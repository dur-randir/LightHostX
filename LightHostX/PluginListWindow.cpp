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

    setContentOwned(new PluginListComponent(pluginFormatManager,
            list,
            deadMansPedalFile,
            AppProperties::get()), true);

    setUsingNativeTitleBar(true);
    setResizable(true, false);
    setResizeLimits(300, 400, 800, 1500);
    setTopLeftPosition(60, 60);

    restoreWindowStateFromString(AppProperties::get()->getValue("listWindowPos"));
    setVisible(true);
}

PluginListWindow::~PluginListWindow() {
    AppProperties::get()->setValue("listWindowPos", getWindowStateAsString());
    clearContentComponent();
}

void PluginListWindow::closeButtonPressed() {
    //owner.removePluginsLackingInputOutput();

    //owner.pluginListWindow = nullptr;
    this->setVisible(false);
}

