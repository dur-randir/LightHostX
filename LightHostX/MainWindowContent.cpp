//
//  MainWindowContent.cpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#include "MainWindowContent.hpp"

MainWindowContent::MainWindowContent(AudioDeviceManagerSP      dm,
                                     KnownPluginList&          kpl,
                                     AudioPluginFormatManager& fmt,
                                     AudioProcessorGraphSP     g)
    : deviceManager(dm)
{
    graphCanvas = std::make_unique<NodeGraphCanvas>(dm, kpl, fmt, g);

    graphCanvas->onManagePlugins    = [this] {
        if (onManagePlugins) onManagePlugins();
    };

    addAndMakeVisible(*graphCanvas);
}

void MainWindowContent::paint(Graphics& g) {
    g.fillAll(NP::bg);
}

void MainWindowContent::resized() {
    auto bounds = getLocalBounds();
    graphCanvas->setBounds(bounds);
}

void MainWindowContent::loadState(const XmlElement& xml) {
    graphCanvas->loadState(xml);
}
