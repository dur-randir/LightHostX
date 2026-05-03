//
//  NodeGraphCanvas.hpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#ifndef NodeGraphCanvas_hpp
#define NodeGraphCanvas_hpp

#include "JuceHeader.h"
#include "DeviceSelectorDialog.hpp"

using AudioProcessorGraphSP = std::shared_ptr<AudioProcessorGraph>;

namespace NP
{
    const Colour bg         { 0xFFE8E8E8 };
    const Colour canvas     { 0xFFECECEC };
    const Colour grid       { 0xFFD8D8D8 };
    const Colour zoneBg     { 0xFFDFDFDF };
    const Colour zoneBorder { 0xFFBBBBBB };
    const Colour zoneHeader { 0xFFD0D0D0 };
    const Colour zoneTitle  { 0xFF333333 };
    const Colour rowBg      { 0xFFCCCCCC };
    const Colour rowText    { 0xFF222222 };
    const Colour nodePlugin { 0xFFDDDDDD };
    const Colour nodeIn     { 0xFFB8CBE0 };
    const Colour nodeOut    { 0xFFD8B8B8 };
    const Colour nodeBorder { 0xFF999999 };
    const Colour nodeText   { 0xFF111111 };
    const Colour nodeHint   { 0xFF888888 };
    const Colour portIn     { 0xFF2980B9 };
    const Colour portOut    { 0xFFE67E22 };
    const Colour wireCol    { 0xFF888888 };
    const Colour wireActive { 0xFFE67E22 };
    const Colour wireBad    { 0xFFCC2222 };
}

//==============================================================================

enum class NodeType { Input, Output, Plugin };

//==============================================================================
/** A visual + audio node. graphNodeId links this to the AudioProcessorGraph. */
struct PluginNode
{
    int        id   { 0 };
    NodeType   type { NodeType::Plugin };
    String     name;
    juce::Point<int> pos  { 200, 100 };

    /** Corresponding AudioProcessorGraph NodeID (0 = not in graph yet). */
    AudioProcessorGraph::NodeID graphNodeId { 0 };

    // Methods to get scaled sizes
    static int getWidth()    { return 170; }
    static int getHeight()   { return 56; }
    static int getSideHeight() { return 40; }
    static int getPortRadius() { return 7; }

    bool hasInputPort()  const { return type != NodeType::Input;  }
    bool hasOutputPort() const { return type != NodeType::Output; }

    juce::Point<int> inputPort()  const { return { pos.x,              pos.y + getHeight() / 2 }; }
    juce::Point<int> outputPort() const { return { pos.x + getWidth(), pos.y + getHeight() / 2 }; }
    juce::Rectangle<int> bounds() const { return { pos.x, pos.y, getWidth(), getHeight() }; }
};

struct NodeWire { int fromNode { -1 }; int toNode { -1 }; };

class PluginParameterListener : public AudioProcessorListener
{
public:
    explicit PluginParameterListener(std::function<void()> callback)
        : onParameterChanged(std::move(callback)) {}
    
    void audioProcessorChanged(AudioProcessor*, const ChangeDetails&) override { }
    void audioProcessorParameterChanged(AudioProcessor*, int, float) override
    {
        if (onParameterChanged) onParameterChanged();
    }

private:
    std::function<void()> onParameterChanged;
};

//==============================================================================
/**
 * NodeGraphCanvas — visual + audio routing graph.
 *   Left  zone: Input device nodes  (fixed, port on right edge)
 *   Centre zone: Plugin nodes (freely movable, double-click = open editor)
 *   Right zone:  Output device nodes (fixed, port on left edge)
 *
 * All wires immediately update the AudioProcessorGraph.
 */
class NodeGraphCanvas : public juce::Component
{
public:
    // Methods to get scaled sizes
    static int getZoneWidth()  { return 170; }
    static int getHeaderHeight() { return 35; }

    static constexpr uint32 kInputNodeUID  = 1000000;
    static constexpr uint32 kOutputNodeUID = 1000001;

    NodeGraphCanvas(AudioDeviceManagerSP      dm,
                    KnownPluginList&          knownPlugins,
                    AudioPluginFormatManager& fmt,
                    AudioProcessorGraphSP     graph);
    ~NodeGraphCanvas() override = default;

    /** Add an Input or Output side-panel node (graphNodeId pre-set to known IDs). */
    void addNode(const String& name, NodeType type);

    const std::vector<PluginNode>& getNodes() const noexcept { return nodes; }

    std::function<void()> onManagePlugins;
    std::function<void()> onDoubleClickLeft;
    std::function<void()> onDoubleClickRight;
    std::function<void(int, NodeType)> onEditNode;
    std::function<void()> onGraphChanged;

    std::unique_ptr<XmlElement> saveState() const;
    void loadState(const XmlElement& xml);

    void paint(Graphics& g) override;
    void mouseDoubleClick(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    bool keyPressed(const KeyPress& key) override;

private:
    AudioDeviceManagerSP      deviceManager;
    KnownPluginList&          knownPlugins;
    AudioPluginFormatManager& formatManager;
    AudioProcessorGraphSP     graph;

    std::vector<PluginNode> nodes;
    std::vector<NodeWire>   wires;

    int nextId                      { 1 };
    // Selection state
    int        selectedNode         { -1 };

    // Drag state
    int        draggingNode         { -1 };
    bool       draggingWire         { false };
    bool       wireDragFromInput    { false };
    int        wireFrom             { -1 };
    juce::Point<int> wireCursor;

    // ---- Zone geometry -----------------------------------------------
    enum class Zone { Left, Center, Right };
    Zone                 zoneAt(juce::Point<int> p) const;
    juce::Point<int>     inputPortPos (const PluginNode& n) const;
    juce::Point<int>     outputPortPos(const PluginNode& n) const;
    juce::Rectangle<int> nodeBounds   (const PluginNode& n) const;

    // ---- Drawing -----------------------------------------------------
    void drawZoneBackgrounds(Graphics& g) const;
    void drawNode(Graphics& g, const PluginNode& n) const;
    void drawWire(Graphics& g, juce::Point<int> a, juce::Point<int> b, bool active) const;

    // ---- Hit testing -------------------------------------------------
    int  nodeAtPoint   (juce::Point<int> p) const;
    bool nearOutputPort(juce::Point<int> p, int& outId) const;
    bool nearInputPort (juce::Point<int> p, int& outId) const;
    bool isValidWire   (int fromId, int toId) const;

    // ---- Graph interaction -------------------------------------------
    /** Connect two canvas nodes in the AudioProcessorGraph (stereo, ch 0+1). */
    void addGraphConnection   (const PluginNode& from, const PluginNode& to);
    /** Disconnect two canvas nodes in the AudioProcessorGraph. */
    void removeGraphConnection(const PluginNode& from, const PluginNode& to);
    /** Remove all graph connections that go into a given node (single-input rule). */
    void clearGraphInputConnections(const PluginNode& to);
    /** Disconnect all wires connected to a node (both input and output). */
    void disconnectNode(int nodeId);

    // ---- Actions -----------------------------------------------------
    void showPluginPicker(juce::Point<int> canvasPos);
    void openPluginEditor(int nodeId);
    void removeNode(int nodeId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeGraphCanvas)
};


#endif /* NodeGraphCanvas_hpp */
