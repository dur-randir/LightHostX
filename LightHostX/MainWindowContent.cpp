//
//  MainWindowContent.cpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#include "MainWindowContent.hpp"
#include "AppProperties.hpp"
#include "DeviceSelectorWindow.hpp"

MainWindowContent::MainWindowContent(AudioDeviceManager&      dm,
                                     KnownPluginList&          kpl,
                                     AudioPluginFormatManager& fmt,
                                     AudioProcessorGraph&      g)
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
        if (onGraphChanged) onGraphChanged();
    };
    graphCanvas->onEditNode         = [this] (int /*nodeId*/, NodeType type) {
        showDialog(false, type);
    };

    addAndMakeVisible(*graphCanvas);
}

void MainWindowContent::saveState() {
    // Save audio device configuration
    std::unique_ptr<XmlElement> audioState(deviceManager.createStateXml());
    AppProperties::get()->setValue("audioDeviceState", audioState.get());
    AppProperties::saveIfNeeded();
}

void MainWindowContent::showDialog(bool create, NodeType type) {
    String name_ = type == NodeType::Input ? "Audio Input" : "Audio Output";
    
    auto* wnd = new DeviceSelectorWindow(
        name_,
        deviceManager,
        type == NodeType::Input ? 256 : 0,
        type == NodeType::Output ? 256 : 0,
        [this,create,type](const String& name) {
            if (create) {
                graphCanvas->addNode(name, type);
            }
            saveState();
        });
    wnd->addToDesktop(ComponentPeer::windowIsResizable);
    wnd->setVisible(true);
    wnd->toFront(true);
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
