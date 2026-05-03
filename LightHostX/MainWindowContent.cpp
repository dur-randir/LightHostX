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
    : deviceManager(dm)
{
    graphCanvas = std::make_unique<NodeGraphCanvas>(dm, kpl, fmt, g);

    graphCanvas->onDoubleClickLeft  = [this] {
        showDialog(true, NodeType::Input);
    };
    graphCanvas->onDoubleClickRight = [this] {
        showDialog(true, NodeType::Output);
    };
    graphCanvas->onManagePlugins    = [this] {
        if (onManagePlugins) onManagePlugins();
    };
    graphCanvas->onGraphChanged     = [this] {
        if (auto xml = this->saveState()) {
            AppProperties::get()->setValue("nodeGraphState", xml.get());
            AppProperties::saveIfNeeded();
        }
    };
    graphCanvas->onEditNode         = [this] (int /*nodeId*/, NodeType type) {
        showDialog(false, type);
    };

    addAndMakeVisible(*graphCanvas);
}

void MainWindowContent::showDialog(bool create, NodeType type) {
    auto* wnd = new DeviceSelectorWindow(
        type == NodeType::Input ? "Audio Input" : "Audio Output",
        deviceManager,
        type == NodeType::Input ? 256 : 0,
        type == NodeType::Output ? 256 : 0,
        [this,create,type](const String& name) {
            if (create) {
                graphCanvas->addNode(name, type);
            }
            saveState();
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

std::unique_ptr<XmlElement> MainWindowContent::saveState() const {
    return graphCanvas->saveState();
}

void MainWindowContent::loadState(const XmlElement& xml) {
    graphCanvas->loadState(xml);
}
