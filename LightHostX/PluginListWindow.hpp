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
class PluginListWindow : public DocumentWindow, public ChangeListener
{
public:
    PluginListWindow(KnownPluginList& list, AudioPluginFormatManager& pluginFormatManager);
    ~PluginListWindow();
    void closeButtonPressed() override;
    bool keyPressed(const KeyPress& key) override;
    void changeListenerCallback(ChangeBroadcaster* changed) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListWindow)
};

#endif /* PluginListWindow_hpp */
