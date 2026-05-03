//
//  MainWindowContent.hpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#ifndef MainWindowContent_hpp
#define MainWindowContent_hpp

#include "JuceHeader.h"
#include "NodeGraphCanvas.hpp"

class MainWindowContent : public juce::Component
{
public:
    MainWindowContent(AudioDeviceManagerSP      deviceManager,
                      KnownPluginList&          knownPlugins,
                      AudioPluginFormatManager& formatManager,
                      AudioProcessorGraphSP     graph);
    ~MainWindowContent() override = default;

    void resized() override;
    void paint(Graphics& g) override;

    std::function<void()> onManagePlugins;

    std::unique_ptr<XmlElement> saveState() const;
    void loadState(const XmlElement& xml);

private:
    AudioDeviceManagerSP             deviceManager;
    std::unique_ptr<NodeGraphCanvas> graphCanvas;

    void showDialog(bool create, NodeType type);
};

#endif /* MainWindowContent_hpp */
