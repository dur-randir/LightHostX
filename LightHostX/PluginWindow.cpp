#include "JuceHeader.h"
#include "PluginWindow.hpp"

class PluginWindow;
static Array <PluginWindow*> activePluginWindows;

PluginWindow::PluginWindow (Component* const pluginEditor,
                            AudioProcessorGraph::Node* const o,
                            WindowFormatType t)
    : DocumentWindow (pluginEditor->getName(), Colours::lightgrey, DocumentWindow::minimiseButton | DocumentWindow::closeButton), owner (o), type (t) 
{
    setSize(400, 300);
    setUsingNativeTitleBar(true);
    setContentOwned(pluginEditor, true);

    setTopLeftPosition(Random::getSystemRandom().nextInt(500), Random::getSystemRandom().nextInt(500));

    setVisible (true);

    activePluginWindows.add(this);
}

void PluginWindow::closeCurrentlyOpenWindowsFor(const uint32 nodeId)
{
    for (int i = activePluginWindows.size(); --i >= 0;) {
        if (activePluginWindows.getUnchecked(i)->owner->nodeID.uid == nodeId) {
            delete activePluginWindows.getUnchecked (i);
        }
    }
}

void PluginWindow::closeAllCurrentlyOpenWindows()
{
    for (const auto& window : activePluginWindows) {
        delete window;
    }
    activePluginWindows.clearQuick();

    Component dummyModalComp;
    dummyModalComp.enterModalState(false);
    MessageManager::getInstance()->runDispatchLoopUntil(50);
}

bool PluginWindow::containsActiveWindows()
{
    return activePluginWindows.size() > 0;
}

void PluginWindow::closeButtonPressed()
{
    delete this;
}

//==============================================================================
class ProcessorProgramPropertyComp : public PropertyComponent,
                                     private AudioProcessorListener
{
public:
    ProcessorProgramPropertyComp (const String& name, AudioProcessor& p, int index_)
        : PropertyComponent (name),
          owner (p),
          index (index_)
    {
        owner.addListener(this);
    }

    ~ProcessorProgramPropertyComp()
    {
        owner.removeListener(this);
    }

    void refresh() override { }
    virtual void audioProcessorChanged (AudioProcessor*, const ChangeDetails&) override { }
    virtual void audioProcessorParameterChanged(AudioProcessor*, int, float) override { }

private:
    AudioProcessor& owner;
    const int index;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorProgramPropertyComp)
};

class ProgramAudioProcessorEditor : public AudioProcessorEditor
{
public:
    ProgramAudioProcessorEditor (AudioProcessor* const p)
        : AudioProcessorEditor (p)
    {
        jassert (p != nullptr);
        setOpaque (true);

        addAndMakeVisible (panel);

        Array<PropertyComponent*> programs;

        const int numPrograms = p->getNumPrograms();
        int totalHeight = 0;

        for (int i = 0; i < numPrograms; ++i) {
            String name(p->getProgramName(i).trim());

            if (name.isEmpty()) {
                name = "Unnamed";
            }

            ProcessorProgramPropertyComp* const pc = new ProcessorProgramPropertyComp (name, *p, i);
            programs.add(pc);
            totalHeight += pc->getPreferredHeight();
        }

        panel.addProperties(programs);

        setSize(400, jlimit(25, 400, totalHeight));
    }

    void paint (Graphics& g)
    {
        g.fillAll (Colours::grey);
    }

    void resized()
    {
        panel.setBounds(getLocalBounds());
    }

private:
    PropertyPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramAudioProcessorEditor)
};

//==============================================================================
PluginWindow* PluginWindow::getWindowFor (AudioProcessorGraph::Node* const node,
                                          WindowFormatType type)
{
    jassert (node != nullptr);

    for (int i = activePluginWindows.size(); --i >= 0;) {
        if (activePluginWindows.getUnchecked(i)->owner == node
            && activePluginWindows.getUnchecked(i)->type == type) {
            return activePluginWindows.getUnchecked(i);
        }
    }

    AudioProcessor* processor = node->getProcessor();
    AudioProcessorEditor* ui = nullptr;

    if (type == Normal) {
        ui = processor->createEditorIfNeeded();

        if (ui == nullptr) {
            type = Generic;
        }
    }

    if (ui == nullptr) {
        if (type == Generic || type == Parameters) {
            ui = new GenericAudioProcessorEditor (processor);
        } else if (type == Programs) {
            ui = new ProgramAudioProcessorEditor (processor);
        }
    }

    if (ui != nullptr) {
        if (AudioPluginInstance* const plugin = dynamic_cast<AudioPluginInstance*> (processor)) {
            ui->setName(plugin->getName());
        }

        return new PluginWindow(ui, node, type);
    }

    return nullptr;
}

PluginWindow::~PluginWindow()
{
    activePluginWindows.removeFirstMatchingValue(this);
    clearContentComponent();
}
