#ifndef PluginWindow_h
#define PluginWindow_h

class PluginWindow  : public DocumentWindow
{
public:
    enum WindowFormatType
    {
        Normal = 0,
        Generic,
        Programs,
        Parameters,
        NumTypes
    };

    PluginWindow(Component* pluginEditor, AudioProcessorGraph::Node*, WindowFormatType);
    ~PluginWindow();

    static PluginWindow* getWindowFor(AudioProcessorGraph::Node*, WindowFormatType);

    static void closeCurrentlyOpenWindowsFor(const uint32 nodeId);
    static void closeAllCurrentlyOpenWindows();
    static bool containsActiveWindows();
    
    void closeButtonPressed() override;

private:
    AudioProcessorGraph::Node* owner;
    WindowFormatType type;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

#endif /* PluginWindow_hpp */
