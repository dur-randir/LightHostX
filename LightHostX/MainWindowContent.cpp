//
//  MainWindowContent.cpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#include "MainWindowContent.hpp"
#include "AppProperties.hpp"
#include "DeviceSelectorWindow.hpp"

MainWindowContent::MainWindowContent(AudioDeviceManagerSP      dm,
                                     KnownPluginList&          kpl,
                                     AudioPluginFormatManager& fmt,
                                     AudioProcessorGraphSP     g)
    : deviceManager(dm), posted(0)
{
    graphCanvas = std::make_unique<NodeGraphCanvas>(dm, kpl, fmt, g);

    graphCanvas->onManagePlugins    = [this] {
        if (onManagePlugins) onManagePlugins();
    };
    graphCanvas->onGraphChanged     = [this] {
        if (posted) return;
        posted++;

        MessageManager::callAsync([this] {
            if (auto xml = this->getState()) {
                AppProperties::get()->setValue("nodeGraphState", xml.get());
                AppProperties::saveIfNeeded();
            }
            posted = 0;
        });
    };
    graphCanvas->onEditNode         = [this] (int /*nodeId*/, NodeType type) {
        showDialog(type);
    };

    addAndMakeVisible(*graphCanvas);
}

void MainWindowContent::showDialog(NodeType type) {
    auto* wnd = new DeviceSelectorWindow(
        type == NodeType::Input ? "Audio Input" : "Audio Output",
        deviceManager,
        type == NodeType::Input ? 256 : 0,
        type == NodeType::Output ? 256 : 0,
        [this]() {
            graphCanvas->updateInOutNames();
        });
    wnd->setVisible(true);
    wnd->setAlwaysOnTop(true);
}

void MainWindowContent::paint(Graphics& g) {
    g.fillAll(NP::bg);
}

void MainWindowContent::resized() {
    auto bounds = getLocalBounds();
    graphCanvas->setBounds(bounds);
}

std::unique_ptr<XmlElement> MainWindowContent::getState() const {
    return graphCanvas->getState();
}

void MainWindowContent::loadState(const XmlElement& xml) {
    graphCanvas->loadState(xml);
}
