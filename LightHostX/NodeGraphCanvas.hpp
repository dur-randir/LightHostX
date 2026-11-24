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
#include "LightHostLookAndFeel.h"

using AudioProcessorGraphSP = std::shared_ptr<AudioProcessorGraph>;
using IOProcessor = AudioProcessorGraph::AudioGraphIOProcessor;

/** Canvas palette — the LightHost warm-dark "tactile" look (see LightHostLookAndFeel.h). */
namespace NP
{
    using LAF = lighthost::ui::LightHostLookAndFeel;

    const Colour bg             = LAF::appBg();
    const Colour zoneBg         { 0xFF0F0E0D };        // side rails, well below the canvas
    const Colour zoneBorder     { 0xFF433D36 };
    const Colour grid           { 0x12FFFFFF };        // faint dot grid
    const Colour selection      = LAF::accent();
    const Colour shadow         { 0x73000000 };
    const Colour portIdle       { 0xFFB0A498 };
    const Colour portRing       = LAF::nodeBottom();
    const Colour wireCol        { 0xFF8A7E72 };
    const Colour wireValid      { 0xFF5EC27A };

    // Endpoint (in/out device) rows: lifted well above the rail.
    const Colour endpointTop    { 0xFF46403A };
    const Colour endpointBottom { 0xFF36312C };
    const Colour endpointBorder { 0xFF625A50 };

    // Active plugin card.
    const Colour cardTop        { 0xFF3A3530 };
    const Colour cardBorder     { 0xFF56504A };

    // Bypassed plugin card: hollow + hatched + dashed.
    const Colour bypassFill     { 0xFF1A1816 };
    const Colour bypassHatch    { 0x0FFFFFFF };
    const Colour bypassEdge     { 0xFF6E665D };
    const Colour wireBad    { 0xFFD25B5B };
}

//==============================================================================

struct Rates {
    double sr;
    int bs;

    static Rates getRates(AudioDeviceManagerSP dm) {
        double sr = 44100.0;
        int    bs = 512;

        if (auto* dev = dm->getCurrentAudioDevice()) {
            sr = dev->getCurrentSampleRate();
            bs = dev->getCurrentBufferSizeSamples();
        }

        return {sr, bs};
    }
};


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

    std::unique_ptr<XmlElement> getState() const;
    void loadState(const XmlElement& xml);
    void updateInOutNames();

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

    AudioProcessorGraph::Node::Ptr inputNode;
    AudioProcessorGraph::Node::Ptr outputNode;

    std::vector<PluginNode> nodes;
    std::vector<NodeWire>   wires;
    std::atomic<int> posted;

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
    PluginNode  nodeAtPoint   (juce::Point<int> p) const;
    bool        nearOutputPort(juce::Point<int> p, int& outId) const;
    bool        nearInputPort (juce::Point<int> p, int& outId) const;
    bool        isValidWire   (int fromId, int toId) const;

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
    void toggleBypass(int nodeId);
    void showDeviceDialog(NodeType type, juce::Point<int> pos);
    void onGraphChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeGraphCanvas)
};


#endif /* NodeGraphCanvas_hpp */
