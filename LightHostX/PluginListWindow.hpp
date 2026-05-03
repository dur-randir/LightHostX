//
//  PluginListWindow.hpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#ifndef PluginListWindow_hpp
#define PluginListWindow_hpp

#include "JuceHeader.h"

class IconMenu;
class PluginListWindow : public DocumentWindow
{
public:
    PluginListWindow(KnownPluginList& list, AudioPluginFormatManager& pluginFormatManager);
    ~PluginListWindow();
    void closeButtonPressed();
    bool keyPressed(const KeyPress& key) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListWindow)
};

#endif /* PluginListWindow_hpp */
