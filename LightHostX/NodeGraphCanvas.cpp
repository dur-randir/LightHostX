//
//  NodeGraphCanvas.cpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#include "NodeGraphCanvas.hpp"
#include "PluginWindow.hpp"
#include "DeviceSelectorWindow.hpp"
#include "AppProperties.hpp"

// Global map to hold plugin listeners and keep them alive
static std::map<uint32, std::unique_ptr<PluginParameterListener>> g_pluginListeners;

// ============================================================
// NodeGraphCanvas — constructor
// ============================================================

NodeGraphCanvas::NodeGraphCanvas(AudioDeviceManagerSP dm,
                                 KnownPluginList& kpl,
                                 AudioPluginFormatManager& fmt,
                                 AudioProcessorGraphSP g)
    : deviceManager(dm), knownPlugins(kpl), formatManager(fmt), graph(g), posted(0)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);  // Enable keyboard focus for Delete key handling

    // set up permanent nodes
    inputNode  = graph->addNode(std::make_unique<IOProcessor>(IOProcessor::audioInputNode),
                     AudioProcessorGraph::NodeID(NodeGraphCanvas::kInputNodeUID));
    outputNode = graph->addNode(std::make_unique<IOProcessor>(IOProcessor::audioOutputNode),
                     AudioProcessorGraph::NodeID(NodeGraphCanvas::kOutputNodeUID));

    auto && [sr, bs] = Rates::getRates(deviceManager);

    inputNode->getProcessor()->setPlayConfigDetails(1, 2, sr, bs);
    inputNode->getProcessor()->prepareToPlay(sr, bs);
    outputNode->getProcessor()->setPlayConfigDetails(2, 2, sr, bs);
    outputNode->getProcessor()->prepareToPlay(sr, bs);

    auto deviceSetup = deviceManager->getAudioDeviceSetup();
    addNode(deviceSetup.inputDeviceName, NodeType::Input);
    addNode(deviceSetup.outputDeviceName, NodeType::Output);
}

// ============================================================
// Geometry helpers
// ============================================================

NodeGraphCanvas::Zone NodeGraphCanvas::zoneAt(Point<int> p) const
{
    if (p.x < getZoneWidth())              return Zone::Left;
    if (p.x > getWidth() - getZoneWidth()) return Zone::Right;
    return Zone::Center;
}

Point<int> NodeGraphCanvas::inputPortPos(const PluginNode& n) const
{
    if (n.type == NodeType::Input)  return { -999, -999 };
    if (n.type == NodeType::Output) { auto b = nodeBounds(n); return { b.getX(), b.getCentreY() }; }
    return n.inputPort();
}

Point<int> NodeGraphCanvas::outputPortPos(const PluginNode& n) const
{
    if (n.type == NodeType::Output) return { -999, -999 };
    if (n.type == NodeType::Input)  { auto b = nodeBounds(n); return { b.getRight(), b.getCentreY() }; }
    return n.outputPort();
}

Rectangle<int> NodeGraphCanvas::nodeBounds(const PluginNode& n) const
{
    if (n.type == NodeType::Input || n.type == NodeType::Output)
    {
        // 計算此節點在同類型中的排位（slot），確保 Y 坐標隨 scale 動態更新
        int slot = 0;
        for (const auto& nd : nodes)
        {
            if (nd.type == n.type)
            {
                if (nd.id == n.id) break;
                ++slot;
            }
        }
        int y = getHeaderHeight() + 6 + slot * (PluginNode::getSideHeight() + 6);
        int x = (n.type == NodeType::Input) ? 0 : getWidth() - getZoneWidth();
        return { x, y, getZoneWidth(), PluginNode::getSideHeight() };
    }
    return n.bounds();
}

// ============================================================
// addNode — visual + audio graph
// ============================================================

void NodeGraphCanvas::addNode(const String& name, NodeType type)
{
    PluginNode n;
    n.id   = nextId++;
    n.type = type;
    n.name = name;

    // Set position
    if (type == NodeType::Input)
    {
        int cnt = 0;
        for (const auto& x : nodes) if (x.type == NodeType::Input) ++cnt;
        n.pos = { 0, getHeaderHeight() + 6 + cnt * (PluginNode::getSideHeight() + 6) };
        n.graphNodeId = AudioProcessorGraph::NodeID(kInputNodeUID);
    }
    else if (type == NodeType::Output)
    {
        int cnt = 0;
        for (const auto& x : nodes) if (x.type == NodeType::Output) ++cnt;
        n.pos = { getWidth() - getZoneWidth(), getHeaderHeight() + 6 + cnt * (PluginNode::getSideHeight() + 6) };
        n.graphNodeId = AudioProcessorGraph::NodeID(kOutputNodeUID);
    }
    // Plugin nodes are added via showPluginPicker, which sets graphNodeId itself.

    nodes.push_back(n);
    onGraphChanged();
    repaint();
}

// ============================================================
// AudioProcessorGraph helpers
// ============================================================

void NodeGraphCanvas::addGraphConnection(const PluginNode& from, const PluginNode& to)
{
    // Verify that nodes exist in the graph
    if (!graph->getNodeForId(from.graphNodeId)) {
        DBG("WARNING: Source node " << String(from.graphNodeId.uid) << " not found in graph!");
        return;
    }
    if (!graph->getNodeForId(to.graphNodeId)) {
        DBG("WARNING: Target node " << String(to.graphNodeId.uid) << " not found in graph!");
        return;
    }

    DBG("Adding connection from " << String(from.graphNodeId.uid) << " to " << String(to.graphNodeId.uid));

    if (!graph->addConnection({ { from.graphNodeId, 0 }, { to.graphNodeId, 0 } })) {
        DBG("WARNING: Failed to add connection (channel 0)");
    }

    int from_ch = (from.type == NodeType::Input) ? 0 : 1;
    if (!graph->addConnection({ { from.graphNodeId, from_ch }, { to.graphNodeId, 1 } })) {
        DBG("WARNING: Failed to add connection (channel 1)");
    }

    graph->rebuild();  // Rebuild the graph topology to apply new connections
}

void NodeGraphCanvas::removeGraphConnection(const PluginNode& from, const PluginNode& to)
{
    graph->removeConnection({ { from.graphNodeId, 0 }, { to.graphNodeId, 0 } });
    graph->removeConnection({ { from.graphNodeId, 1 }, { to.graphNodeId, 1 } });
    graph->rebuild();  // Rebuild the graph topology after removing connections
}

void NodeGraphCanvas::clearGraphInputConnections(const PluginNode& toNode)
{
    // Remove all existing connections going INTO toNode from the graph
    for (const auto& w : wires)
    {
        if (w.toNode != toNode.id) continue;
        for (const auto& fn : nodes)
        {
            if (fn.id == w.fromNode)
            {
                removeGraphConnection(fn, toNode);
                break;
            }
        }
    }
    // Remove from wire list
    wires.erase(std::remove_if(wires.begin(), wires.end(),
        [&toNode](const NodeWire& w) { return w.toNode == toNode.id; }),
        wires.end());
}

void NodeGraphCanvas::disconnectNode(int nodeId)
{
    // Disconnect all wires connected to this node (both input and output)
    std::vector<int> wiresToRemove;

    for (size_t i = 0; i < wires.size(); ++i)
    {
        if (wires[i].fromNode == nodeId || wires[i].toNode == nodeId)
            wiresToRemove.push_back((int)i);
    }

    // Remove wires in reverse order to maintain indices
    for (int idx = (int)wiresToRemove.size() - 1; idx >= 0; --idx)
    {
        const auto& w = wires[wiresToRemove[idx]];

        // Find the from and to nodes and remove the graph connection
        const PluginNode *frNode = nullptr, *toNode = nullptr;
        for (const auto& nd : nodes)
        {
            if (nd.id == w.fromNode) frNode = &nd;
            if (nd.id == w.toNode)   toNode = &nd;
        }

        if (frNode && toNode)
            removeGraphConnection(*frNode, *toNode);

        wires.erase(wires.begin() + wiresToRemove[idx]);
    }

    graph->rebuild();
    onGraphChanged();
    repaint();
}

// ============================================================
// Drawing
// ============================================================

void NodeGraphCanvas::drawZoneBackgrounds(Graphics& g) const
{
    using LAF = lighthost::ui::LightHostLookAndFeel;
    const int w = getWidth(), h = getHeight(), zw = getZoneWidth(), hh = getHeaderHeight();

    // Side rails: recessed, with a hairline separator towards the canvas.
    g.setColour(NP::zoneBg);
    g.fillRect(0, 0, zw, h);
    g.fillRect(w - zw, 0, zw, h);
    g.setColour(NP::zoneBorder);
    g.fillRect(zw - 1, 0, 1, h);
    g.fillRect(w - zw, 0, 1, h);

    // Rail headers in the title-bar style: gradient, bottom border, accent diamond + mono caption.
    auto drawHeader = [&](int x, const String& title)
    {
        const Rectangle<int> hdr(x, 0, zw, hh);
        g.setGradientFill(ColourGradient(LAF::titleTop(), 0.f, 0.f, LAF::titleBottom(), 0.f, (float)hh, false));
        g.fillRect(hdr);
        g.setColour(LAF::titleBorder());
        g.fillRect(hdr.withTop(hh - 1));

        g.setFont(LAF::mono(10.5f));
        const int textW = GlyphArrangement::getStringWidthInt(g.getCurrentFont(), title);
        const float diamond = 7.f;
        const float startX = (float)hdr.getCentreX() - (diamond + 8.f + (float)textW) * 0.5f;

        Path d;
        d.addRectangle(-diamond * 0.5f, -diamond * 0.5f, diamond, diamond);
        g.setColour(LAF::accent());
        g.fillPath(d, AffineTransform::rotation(MathConstants<float>::pi * 0.25f)
                          .translated(startX + diamond * 0.5f, (float)hdr.getCentreY()));

        g.setColour(LAF::textSecond());
        g.drawText(title, Rectangle<int>((int)(startX + diamond + 8.f), 0, textW + 2, hh - 1),
                   Justification::centredLeft, false);
    };
    drawHeader(0, "INPUT");
    drawHeader(w - zw, "OUTPUT");

    bool hasIn = false, hasOut = false;
    for (const auto& nd : nodes)
    {
        if (nd.type == NodeType::Input)  hasIn  = true;
        if (nd.type == NodeType::Output) hasOut = true;
    }
    g.setColour(LAF::textTert());
    g.setFont(LAF::mono(9.5f));
    if (!hasIn) {
        g.drawText("double-click to add", Rectangle<int>(0, hh + 4, zw, 22), Justification::centred, false);
    }
    if (!hasOut) {
        g.drawText("double-click to add", Rectangle<int>(w - zw, hh + 4, zw, 22), Justification::centred, false);
    }
}

static void drawPortDot(Graphics& g, Point<int> pt, bool hollow = false)
{
    const float pr = (float)PluginNode::getPortRadius();
    const auto r = Rectangle<float>(pr * 2.f, pr * 2.f).withCentre(pt.toFloat());
    g.setColour(NP::portRing);
    g.fillEllipse(r);
    if (hollow) {
        g.setColour(NP::bypassEdge);
        g.drawEllipse(r.reduced(2.5f), 1.5f);
    } else {
        g.setColour(NP::portIdle);
        g.fillEllipse(r.reduced(2.5f));
    }
}

static bool isNodeBypassed(const AudioProcessorGraph& graph, const PluginNode& n)
{
    if (n.type != NodeType::Plugin) return false;
    if (auto* graphNode = graph.getNodeForId(n.graphNodeId)) {
        return graphNode->isBypassed();
    }
    return false;
}

void NodeGraphCanvas::drawNode(Graphics& g, const PluginNode& n) const
{
    using LAF = lighthost::ui::LightHostLookAndFeel;
    const bool selected = (selectedNode == n.id);

    if (n.type == NodeType::Input || n.type == NodeType::Output) {
        // --- Endpoint rows on the side rails: raised, lighter than the rail, with an
        //     accent stripe on the outer edge so they read as the signal's source/sink.
        const bool isInput = (n.type == NodeType::Input);
        const auto b  = nodeBounds(n);
        const auto rf = b.reduced(8, 3).toFloat();

        g.setColour(NP::shadow);
        g.fillRoundedRectangle(rf.translated(0.f, 2.f), 8.f);

        g.setGradientFill(ColourGradient(NP::endpointTop, 0.f, rf.getY(), NP::endpointBottom, 0.f, rf.getBottom(), false));
        g.fillRoundedRectangle(rf, 8.f);
        g.setColour(LAF::cardEdge().withAlpha(0.8f));
        g.drawLine(rf.getX() + 8.f, rf.getY() + 1.f, rf.getRight() - 8.f, rf.getY() + 1.f, 1.f);

        {
            Graphics::ScopedSaveState ss(g);
            Path clip;
            clip.addRoundedRectangle(rf, 8.f);
            g.reduceClipRegion(clip);
            g.setColour(LAF::accent());
            g.fillRect(isInput ? rf.withWidth(3.f) : rf.withTrimmedLeft(rf.getWidth() - 3.f));
        }

        g.setColour(selected ? NP::selection : NP::endpointBorder);
        g.drawRoundedRectangle(rf.reduced(0.5f), 8.f, selected ? 1.5f : 1.f);

        // Accent status dot with glow.
        const auto dot = Rectangle<float>(7.f, 7.f).withCentre({ isInput ? rf.getX() + 15.f : rf.getRight() - 15.f, rf.getCentreY() });
        g.setColour(LAF::accent().withAlpha(0.4f));
        g.fillEllipse(dot.expanded(3.f));
        g.setColour(LAF::accent());
        g.fillEllipse(dot);

        const auto textRect = isInput
            ? b.reduced(8, 0).withTrimmedLeft(24).withTrimmedRight(12)
            : b.reduced(8, 0).withTrimmedLeft(12).withTrimmedRight(24);
        g.setColour(Colour(0xfff4f0e9));
        g.setFont(LAF::display(12.5f, true));
        g.drawText(n.name.isEmpty() ? String("<none>") : n.name, textRect,
                   isInput ? Justification::centredLeft : Justification::centredRight, true);

        drawPortDot(g, isInput ? outputPortPos(n) : inputPortPos(n));
    } else if (isNodeBypassed(*graph, n)) {
        // --- Bypassed plugin: hollow, hatched, dashed outline — clearly "out of the chain".
        const auto bf = n.bounds().toFloat().reduced(0.5f);
        Path body;
        body.addRoundedRectangle(bf, 9.f);

        g.setColour(NP::bypassFill);
        g.fillPath(body);
        {
            Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(body);
            g.setColour(NP::bypassHatch);
            for (float x = bf.getX() - bf.getHeight(); x < bf.getRight(); x += 9.f) {
                g.drawLine(x, bf.getBottom(), x + bf.getHeight(), bf.getY(), 1.5f);
            }
        }

        Path dashed;
        const float dashes[] = { 5.f, 4.f };
        PathStrokeType(selected ? 1.5f : 1.2f).createDashedStroke(dashed, body, dashes, 2);
        g.setColour(selected ? NP::selection : NP::bypassEdge);
        g.fillPath(dashed);
        if (selected) {
            g.setColour(NP::selection.withAlpha(0.25f));
            g.drawRoundedRectangle(bf.expanded(2.5f), 11.f, 2.f);
        }

        // Hollow status ring.
        const auto dot = Rectangle<float>(8.f, 8.f).withCentre({ bf.getX() + 22.f, bf.getCentreY() });
        g.setColour(NP::bypassEdge);
        g.drawEllipse(dot, 1.5f);

        auto textArea = n.bounds().withTrimmedLeft(34).withTrimmedRight(PluginNode::getPortRadius() + 8).reduced(0, 10);
        g.setColour(LAF::textSecond());
        g.setFont(LAF::display(13.f, true));
        g.drawText(n.name, textArea.removeFromTop(textArea.getHeight() / 2 + 2), Justification::bottomLeft, true);

        // "BYPASSED" pill badge.
        g.setFont(LAF::mono(8.5f));
        const auto label = String("BYPASSED");
        const int pillW = GlyphArrangement::getStringWidthInt(g.getCurrentFont(), label) + 12;
        const auto pill = Rectangle<int>(textArea.getX(), textArea.getY() + 1, pillW, 13).toFloat();
        g.setColour(LAF::accent().withAlpha(0.18f));
        g.fillRoundedRectangle(pill, 6.5f);
        g.setColour(LAF::accent().withAlpha(0.7f));
        g.drawRoundedRectangle(pill.reduced(0.5f), 6.f, 1.f);
        g.setColour(LAF::accent());
        g.drawText(label, pill.toNearestInt(), Justification::centred, false);

        drawPortDot(g, n.inputPort(),  true);
        drawPortDot(g, n.outputPort(), true);
    } else {
        // --- Active plugin card (NodeCard style) ---
        const auto bf = n.bounds().toFloat().reduced(0.5f);

        g.setColour(NP::shadow);
        g.fillRoundedRectangle(bf.translated(0.f, 3.f).expanded(1.f), 10.f);

        g.setGradientFill(ColourGradient(NP::cardTop, 0.f, bf.getY(), LAF::cardBottom(), 0.f, bf.getBottom(), false));
        g.fillRoundedRectangle(bf, 9.f);
        g.setColour(LAF::cardEdge().withAlpha(0.7f));
        g.drawLine(bf.getX() + 8.f, bf.getY() + 1.f, bf.getRight() - 8.f, bf.getY() + 1.f, 1.f);
        g.setColour(selected ? NP::selection : NP::cardBorder);
        g.drawRoundedRectangle(bf, 9.f, selected ? 1.5f : 1.f);
        if (selected) {
            g.setColour(NP::selection.withAlpha(0.25f));
            g.drawRoundedRectangle(bf.expanded(2.5f), 11.f, 2.f);
        }

        const auto dot = Rectangle<float>(8.f, 8.f).withCentre({ bf.getX() + 22.f, bf.getCentreY() });
        g.setColour(LAF::accent().withAlpha(0.35f));
        g.fillEllipse(dot.expanded(2.f));
        g.setColour(LAF::accent());
        g.fillEllipse(dot);

        auto textArea = n.bounds().withTrimmedLeft(34).withTrimmedRight(PluginNode::getPortRadius() + 8).reduced(0, 10);
        g.setColour(LAF::textPrimary());
        g.setFont(LAF::display(13.f, true));
        g.drawText(n.name, textArea.removeFromTop(textArea.getHeight() / 2 + 2), Justification::bottomLeft, true);

        g.setColour(LAF::textSecond());
        g.setFont(LAF::mono(9.f));
        g.drawText("DOUBLE-CLICK TO EDIT", textArea, Justification::topLeft, true);

        drawPortDot(g, n.inputPort());
        drawPortDot(g, n.outputPort());
    }
}

void NodeGraphCanvas::drawWire(Graphics& g, Point<int> a, Point<int> b, bool active) const
{
    g.setColour(active ? NP::selection : NP::wireCol);
    Path p;
    p.startNewSubPath(a.toFloat());
    const float cx = (a.x + b.x) * 0.5f;
    p.cubicTo(cx, (float)a.y, cx, (float)b.y, (float)b.x, (float)b.y);
    g.strokePath(p, PathStrokeType(2.4f, PathStrokeType::curved, PathStrokeType::rounded));
}

void NodeGraphCanvas::paint(Graphics& g)
{
    using LAF = lighthost::ui::LightHostLookAndFeel;

    // Radial "tactile" backdrop, as in the LightHost canvas.
    g.setGradientFill(ColourGradient(LAF::appBgTop(), getWidth() * 0.5f, -40.f,
                                     LAF::appBgBottom(), getWidth() * 0.5f, (float)getHeight(), true));
    g.fillAll();

    g.setColour(NP::grid);
    for (int x = getZoneWidth() + 16; x < getWidth() - getZoneWidth(); x += 24) {
        for (int y = 12; y < getHeight(); y += 24) {
            g.fillRect(x, y, 2, 2);
        }
    }

    drawZoneBackgrounds(g);

    // Committed wires
    for (const auto& wire : wires) {
        const PluginNode *fr = nullptr, *to = nullptr;
        for (const auto& nd : nodes) {
            if (nd.id == wire.fromNode) fr = &nd;
            if (nd.id == wire.toNode)   to = &nd;
        }
        if (!fr || !to) continue;

        if (isNodeBypassed(*graph, *fr) || isNodeBypassed(*graph, *to)) {
            const auto a = outputPortPos(*fr), b = inputPortPos(*to);
            Path p, dashed;
            const float cx = (a.x + b.x) * 0.5f;
            p.startNewSubPath(a.toFloat());
            p.cubicTo(cx, (float)a.y, cx, (float)b.y, (float)b.x, (float)b.y);
            const float dashes[] = { 6.f, 5.f };
            PathStrokeType(2.f, PathStrokeType::curved, PathStrokeType::rounded).createDashedStroke(dashed, p, dashes, 2);
            g.setColour(NP::wireCol.withAlpha(0.6f));
            g.fillPath(dashed);
        } else {
            drawWire(g, outputPortPos(*fr), inputPortPos(*to), false);
        }
    }

    // Live drag: accent while free, green/red while hovering a port.
    if (draggingWire && wireFrom >= 0) {
        for (const auto& nd : nodes)
        {
            if (nd.id != wireFrom) continue;
            int target = -1;
            bool overPort, valid;
            Point<int> anchor;
            if (!wireDragFromInput) {
                anchor   = outputPortPos(nd);
                overPort = nearInputPort(wireCursor, target);
                valid    = overPort && isValidWire(wireFrom, target);
            } else {
                anchor   = inputPortPos(nd);
                overPort = nearOutputPort(wireCursor, target);
                valid    = overPort && isValidWire(target, wireFrom);
            }
            const Colour c = !overPort ? LAF::accent() : (valid ? NP::wireValid : NP::wireBad);
            g.setColour(c.withAlpha(0.85f));
            Path p;
            const float cx = (anchor.x + wireCursor.x) * 0.5f;
            p.startNewSubPath(anchor.toFloat());
            p.cubicTo(cx, (float)anchor.y, cx, (float)wireCursor.y,
                      (float)wireCursor.x, (float)wireCursor.y);
            g.strokePath(p, PathStrokeType(2.4f, PathStrokeType::curved, PathStrokeType::rounded));
            break;
        }
    }

    for (const auto& nd : nodes) drawNode(g, nd);

    bool hasPlugin = false;
    for (const auto& nd : nodes) {
        if (nd.type == NodeType::Plugin) {
            hasPlugin = true;
            break;
        }
    }

    if (!hasPlugin) {
        g.setColour(LAF::textTert());
        g.setFont(LAF::mono(10.5f));
        g.drawText("Right-click to add a plugin",
            Rectangle<int>(getZoneWidth(), 0, getWidth() - 2 * getZoneWidth(), getHeight()), Justification::centred, false
        );
    }
}

// ============================================================
// Hit testing
// ============================================================

PluginNode NodeGraphCanvas::nodeAtPoint(Point<int> p) const
{
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
        if (nodeBounds(*it).contains(p)) return *it;
    return {};
}

bool NodeGraphCanvas::nearOutputPort(Point<int> p, int& outId) const
{
    const int kSnap = PluginNode::getPortRadius() + 6;
    for (const auto& nd : nodes)
        if (nd.hasOutputPort() && outputPortPos(nd).getDistanceFrom(p) <= kSnap)
        { outId = nd.id; return true; }
    return false;
}

bool NodeGraphCanvas::nearInputPort(Point<int> p, int& outId) const
{
    const int kSnap = PluginNode::getPortRadius() + 6;
    for (const auto& nd : nodes)
        if (nd.hasInputPort() && inputPortPos(nd).getDistanceFrom(p) <= kSnap)
        { outId = nd.id; return true; }
    return false;
}

bool NodeGraphCanvas::isValidWire(int fromId, int toId) const
{
    if (fromId == toId) return false;
    const PluginNode *fr = nullptr, *to = nullptr;
    for (const auto& nd : nodes)
    {
        if (nd.id == fromId) fr = &nd;
        if (nd.id == toId)   to = &nd;
    }
    if (!fr || !to) return false;
    if (!fr->hasOutputPort() || !to->hasInputPort())                         return false;
    if (fr->type == NodeType::Input  && to->type == NodeType::Input)         return false;
    if (fr->type == NodeType::Output && to->type == NodeType::Output)        return false;
    return true;
}

// ============================================================
// Mouse events
// ============================================================

void NodeGraphCanvas::mouseDoubleClick(const MouseEvent& e)
{
    PluginNode nd = nodeAtPoint(e.getPosition());
    if (nd.id)
    {
        if (nd.type == NodeType::Plugin) {
            openPluginEditor(nd.id);
        } else {
            showDeviceDialog(nd.type, localPointToGlobal(e.getPosition()));
        }
    }
}

void NodeGraphCanvas::mouseDown(const MouseEvent& e)
{
    PluginNode nd = nodeAtPoint(e.getPosition());
    int hitNode = nd.id; // for capture
    selectedNode = nd.id ? nd.id : -1;
    repaint();

    // ================== RIGHT-CLICK: Context Menu ==================
    if (e.mods.isRightButtonDown())
    {
        const auto screenPos = localPointToGlobal(e.getPosition());

        if (nd.id)
        {
            // Right-click on a node
                if (nd.type == NodeType::Input || nd.type == NodeType::Output)
                {
                    // Input node: New Input, Disconnect, Delete
                    PopupMenu m;
                    m.addItem(1, "Disconnect All Wires");
                    m.showMenuAsync(PopupMenu::Options().withTargetScreenArea({screenPos.x, screenPos.y, 1, 1}),
                        [this, hitNode](int result) {
                            if (result == 1) disconnectNode(hitNode);
                        });
                }
                else if (nd.type == NodeType::Plugin)
                {
                    // Plugin node: New Plugin, Disconnect, Delete
                    PopupMenu m;
                    m.addItem(1, "Toggle Bypass");
                    m.addItem(2, "Disconnect All Wires");
                    m.addSeparator();
                    m.addItem(3, "Delete");
                    m.showMenuAsync(PopupMenu::Options().withTargetScreenArea({screenPos.x, screenPos.y, 1, 1}),
                        [this, hitNode, ePos = e.getPosition()](int result) {
                            if (result == 1) toggleBypass(hitNode);
                            else if (result == 2) disconnectNode(hitNode);
                            else if (result == 3) removeNode(hitNode);
                        });
                }
                return;
        }
        else
        {
            // Right-click on empty area
            const Zone zone = zoneAt(e.getPosition());

            if (zone == Zone::Center)
            {
                // 右鍵在中間區域顯示外掛程式選單
                showPluginPicker(e.getPosition());
            }
        }
        return;
    }

    // ================== LEFT-CLICK: Select, Drag, Wire ==================

    // Check if clicking near a port (to start wire drag)
    int portNode = -1;
    if (nearOutputPort(e.getPosition(), portNode))
    {
        draggingWire      = true;
        wireDragFromInput = false;
        wireFrom          = portNode;
        wireCursor        = e.getPosition();
        draggingNode      = -1;
        return;
    }
    if (nearInputPort(e.getPosition(), portNode))
    {
        draggingWire      = true;
        wireDragFromInput = true;
        wireFrom          = portNode;
        wireCursor        = e.getPosition();
        draggingNode      = -1;
        return;
    }

    // Not near a port: handle node selection and dragging
    if (selectedNode >= 0)
    {
        // Enable keyboard focus for DEL key
        grabKeyboardFocus();

        // For Plugin nodes, prepare to drag
        for (const auto& nd : nodes)
            if (nd.id == selectedNode && nd.type == NodeType::Plugin)
            {
                draggingNode = selectedNode;
                break;
            }
    }
    draggingWire = false;
}

void NodeGraphCanvas::mouseDrag(const MouseEvent& e)
{
    if (draggingWire) { wireCursor = e.getPosition(); repaint(); return; }
    if (draggingNode >= 0)
    {
        for (auto& nd : nodes)
        {
            if (nd.id != draggingNode) continue;
            const int lo = getZoneWidth();
            const int hi = getWidth() - getZoneWidth() - PluginNode::getWidth();

            auto newPos = Point<int>(std::max(lo, std::min(hi, e.x - PluginNode::getWidth() / 2)),
                                     std::max(0,  std::min(getHeight() - PluginNode::getHeight(), e.y - PluginNode::getHeight() / 2)));
            if (nd.pos != newPos)
            {
                nd.pos = newPos;
                onGraphChanged();
                repaint();
            }
            break;
        }
    }
}

void NodeGraphCanvas::mouseUp(const MouseEvent& e)
{
    if (draggingWire && wireFrom >= 0)
    {
        int target = -1;
        if (!wireDragFromInput)
        {
            if (nearInputPort(e.getPosition(), target) && isValidWire(wireFrom, target))
            {
                // Find from/to nodes
                const PluginNode *frNode = nullptr, *toNode = nullptr;
                for (const auto& nd : nodes)
                {
                    if (nd.id == wireFrom) frNode = &nd;
                    if (nd.id == target)   toNode = &nd;
                }
                if (frNode && toNode)
                {
                    DBG("Wire connection: " << frNode->name << " -> " << toNode->name);
                    clearGraphInputConnections(*toNode);   // remove old wires (visual+audio)
                    addGraphConnection(*frNode, *toNode);  // add new audio connection
                    wires.push_back({ wireFrom, target }); // add visual wire
                    onGraphChanged();
                }
            }
        }
        else
        {
            if (nearOutputPort(e.getPosition(), target) && isValidWire(target, wireFrom))
            {
                const PluginNode *frNode = nullptr, *toNode = nullptr;
                for (const auto& nd : nodes)
                {
                    if (nd.id == target)   frNode = &nd;
                    if (nd.id == wireFrom) toNode = &nd;
                }
                if (frNode && toNode)
                {
                    DBG("Wire connection: " << frNode->name << " -> " << toNode->name);
                    clearGraphInputConnections(*toNode);
                    addGraphConnection(*frNode, *toNode);
                    wires.push_back({ target, wireFrom });
                    onGraphChanged();
                }
            }
        }
    }
    draggingWire      = false;
    wireDragFromInput = false;
    wireFrom          = -1;
    draggingNode      = -1;
    repaint();
}

// ============================================================
// Plugin picker
// ============================================================

void NodeGraphCanvas::showPluginPicker(Point<int> canvasPos)
{
    PopupMenu m;
    auto types = knownPlugins.getTypes();
    if (!types.isEmpty())
    {
        m.addSectionHeader("Available Plugins");
        for (int i = 0; i < types.size(); ++i)
            m.addItem(i + 1, types[i].name + "  [" + types[i].pluginFormatName + "]");
        m.addSeparator();
    }
    const int kManage = 100000;
    m.addItem(kManage, "Add / Manage Plugins...");

    const auto sc = localPointToGlobal(canvasPos);
    m.showMenuAsync(PopupMenu::Options().withTargetScreenArea({sc.x, sc.y, 1, 1}),
        [this, types](int result)
        {
            if (result == kManage && onManagePlugins) { onManagePlugins(); return; }
            if (result <= 0 || result > types.size()) return;

            const auto& desc = types[result - 1];

            String err;
            auto && [sr, bs] = Rates::getRates(deviceManager);

            auto instance = formatManager.createPluginInstance(desc, sr, bs, err);
            if (!instance)
            {
                AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                    "Cannot load plugin", err.isEmpty() ? "Unknown error" : err
                );
                return;
            }
            instance->setPlayConfigDetails(2, 2, sr, bs);
            instance->prepareToPlay(sr, bs);

            // addNode() returns Node::Ptr (ReferenceCountedObjectPtr), not Node*
            auto nodePtr = graph->addNode(
                std::unique_ptr<AudioProcessor>(std::move(instance)));
            if (!nodePtr) return;

            // Add parameter change listener so we save when plugin params change
            if (auto* proc = nodePtr->getProcessor())
            {
                auto listener = std::make_unique<PluginParameterListener>(
                    [this] { onGraphChanged(); }
                );
                proc->addListener(listener.get());
                // Store the listener in global map to keep it alive
                g_pluginListeners[nodePtr->nodeID.uid] = std::move(listener);
            }

            PluginNode n;
            n.id          = nextId++;
            n.type        = NodeType::Plugin;
            n.name        = desc.name;
            n.graphNodeId = nodePtr->nodeID;

            const int cx = (getWidth() - getZoneWidth() * 2) / 2 + getZoneWidth() - PluginNode::getWidth() / 2;
            int cnt = 0;
            for (const auto& x : nodes) if (x.type == NodeType::Plugin) ++cnt;
            n.pos = { cx, 60 + cnt * (PluginNode::getHeight() + 20) };

            nodes.push_back(n);
            onGraphChanged();
            repaint();
        });
}

// ============================================================
// Plugin editor
// ============================================================

void NodeGraphCanvas::openPluginEditor(int nodeId)
{
    const PluginNode* cn = nullptr;
    for (const auto& nd : nodes)
        if (nd.id == nodeId) { cn = &nd; break; }
    if (!cn || cn->type != NodeType::Plugin) return;

    auto* graphNode = graph->getNodeForId(cn->graphNodeId);
    if (!graphNode) return;

    // Open the plugin editor (Normal if available, otherwise Generic)
    PluginWindow::getWindowFor(graphNode, PluginWindow::Normal);
}

void NodeGraphCanvas::toggleBypass(int nodeId) {
    for (const auto& nd : nodes) {
        if (nd.id == nodeId) {
            if (auto* graphNode = graph->getNodeForId(nd.graphNodeId)) {
                graphNode->setBypassed(!graphNode->isBypassed());
            }

            repaint();
        }
    }
}

// ============================================================
// Remove node
// ============================================================

void NodeGraphCanvas::removeNode(int nodeId)
{
    // Allow removing all node types (Plugin, Input, Output)
    for (const auto& nd : nodes)
    {
        if (nd.id == nodeId)
        {
            // Remove from audio graph if it's in the graph
            if (nd.graphNodeId != AudioProcessorGraph::NodeID(0))
            {
                // Clean up the listener
                g_pluginListeners.erase(nd.graphNodeId.uid);
                if (auto* graphNode = graph->getNodeForId(nd.graphNodeId))
                    graph->removeNode(graphNode);
            }

            // Remove from visual nodes
            nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                [nodeId](const PluginNode& n) { return n.id == nodeId; }),
                nodes.end());

            // Remove wires connected to this node
            wires.erase(std::remove_if(wires.begin(), wires.end(),
                [nodeId](const NodeWire& w) { return w.fromNode == nodeId || w.toNode == nodeId; }),
                wires.end());

            selectedNode = -1;
            graph->rebuild();  // Rebuild graph after removing node
            onGraphChanged();
            repaint();
            return;
        }
    }
}

void NodeGraphCanvas::showDeviceDialog(NodeType type, Point<int> pos) {
    auto* wnd = new DeviceSelectorWindow(
        type == NodeType::Input ? "Audio Input" : "Audio Output",
        deviceManager,
        type == NodeType::Input ? 256 : 0,
        type == NodeType::Output ? 256 : 0,
        pos,
        [this]() {
            updateInOutNames();
        });
    wnd->setAlwaysOnTop(true); // before showing, so it never appears behind the floating editor
    wnd->setVisible(true);
    wnd->enterModalState();
}


// ============================================================
// Keyboard events
// ============================================================

bool NodeGraphCanvas::keyPressed(const KeyPress& key)
{
    if ((key.getKeyCode() == KeyPress::deleteKey || key.getKeyCode() == KeyPress::backspaceKey) && selectedNode >= 0)
    {
        removeNode(selectedNode);
        return true;
    }
    return false;
}

// ============================================================
// State management
// ============================================================

std::unique_ptr<XmlElement> NodeGraphCanvas::getState() const
{
    auto xml = std::make_unique<XmlElement>("NodeGraph");
    auto* xNodes = new XmlElement("Nodes");
    xml->addChildElement(xNodes);

    for (const auto& n : nodes)
    {
        auto* xn = new XmlElement("Node");
        xn->setAttribute("id", n.id);
        xn->setAttribute("type", static_cast<int>(n.type));
        xn->setAttribute("name", n.name);
        xn->setAttribute("x", n.pos.x);
        xn->setAttribute("y", n.pos.y);

        if (n.type == NodeType::Plugin)
        {
            if (auto* gNode = graph->getNodeForId(n.graphNodeId))
            {
                if (auto* proc = gNode->getProcessor())
                {
                    PluginDescription desc;
                    if (auto* pi = dynamic_cast<AudioPluginInstance*>(proc))
                        pi->fillInPluginDescription(desc);
                    xn->setAttribute("pluginName", desc.name);
                    xn->setAttribute("pluginFormat", desc.pluginFormatName);
                    xn->setAttribute("pluginFileOrIdentifier", desc.fileOrIdentifier);

                    MemoryBlock mb;
                    proc->getStateInformation(mb);
                    auto* stateXml = new XmlElement("PluginState");
                    stateXml->addTextElement(mb.toBase64Encoding());
                    xn->addChildElement(stateXml);
                }
            }
        }
        xNodes->addChildElement(xn);
    }

    auto* xWires = new XmlElement("Wires");
    xml->addChildElement(xWires);
    for (const auto& w : wires)
    {
        auto* xw = new XmlElement("Wire");
        xw->setAttribute("from", w.fromNode);
        xw->setAttribute("to", w.toNode);
        xWires->addChildElement(xw);
    }

    return xml;
}

void NodeGraphCanvas::loadState(const XmlElement& xml)
{
    // IMPORTANT: Do NOT call graph.clear() here!
    // The INPUT/OUTPUT nodes (UIDs 1000000-1000001) must remain in the graph
    // IconMenu's loadActivePlugins() already created them and we depend on them

    nodes.clear();
    wires.clear();

    // The graph must have fixed input and output nodes first (created by IconMenu)
    // We assume they already exist in the graph. We just map them to canvas.

    const auto* xNodes = xml.getChildByName("Nodes");
    if (!xNodes) return;

    for (auto* xn : xNodes->getChildIterator())
    {
        PluginNode n;
        n.id   = xn->getIntAttribute("id");
        n.type = static_cast<NodeType>(xn->getIntAttribute("type"));
        n.name = xn->getStringAttribute("name");
        n.pos  = { xn->getIntAttribute("x"), xn->getIntAttribute("y") };

        if (n.id >= nextId) nextId = n.id + 1;

        if (n.type == NodeType::Input)
            n.graphNodeId = AudioProcessorGraph::NodeID(kInputNodeUID);
        else if (n.type == NodeType::Output)
            n.graphNodeId = AudioProcessorGraph::NodeID(kOutputNodeUID);
        else if (n.type == NodeType::Plugin)
        {
            // Restore plugin
            String pluginName = xn->getStringAttribute("pluginName");
            String formatName = xn->getStringAttribute("pluginFormat");
            String identifier = xn->getStringAttribute("pluginFileOrIdentifier");

            // Look up the full PluginDescription from knownPlugins
            PluginDescription desc;
            desc.fileOrIdentifier = identifier;
            desc.name = pluginName;
            desc.pluginFormatName = formatName;

            // Try to find a better description match in our known list
            for (const auto& d : knownPlugins.getTypes())
            {
                if (d.fileOrIdentifier == identifier)
                {
                    desc = d;
                    break;
                }
            }

            String err;
            auto && [sr, bs] = Rates::getRates(deviceManager);

            DBG("Restoring plugin: " << desc.name << " [" << desc.pluginFormatName << "]");
            auto instance = formatManager.createPluginInstance(desc, sr, bs, err);
            if (instance)
            {
                if (const auto* xState = xn->getChildByName("PluginState"))
                {
                    MemoryBlock mb;
                    mb.fromBase64Encoding(xState->getAllSubText());
                    instance->setStateInformation(mb.getData(), (int)mb.getSize());
                }
                instance->setPlayConfigDetails(2, 2, sr, bs);
                instance->prepareToPlay(sr, bs);
                auto nodePtr = graph->addNode(std::unique_ptr<AudioProcessor>(std::move(instance)));
                if (nodePtr)
                {
                    n.graphNodeId = nodePtr->nodeID;

                    // Add parameter change listener so we save when plugin params change
                    if (auto* proc = nodePtr->getProcessor())
                    {
                        auto listener = std::make_unique<PluginParameterListener>(
                            [this] { onGraphChanged(); });
                        proc->addListener(listener.get());
                        // Store the listener in global map to keep it alive
                        g_pluginListeners[nodePtr->nodeID.uid] = std::move(listener);
                    }

                    DBG("Successfully added plugin node: " << n.name);
                    DBG(" ID: " << String(n.graphNodeId.uid));
                }
                else
                {
                    DBG("FAILED to add plugin node to graph: " << n.name);
                }
            }
            else
            {
                DBG("FAILED to create plugin instance: " << desc.name << " Error: " << err);
            }
        }
        nodes.push_back(n);
    }

    const auto* xWires = xml.getChildByName("Wires");
    if (xWires)
    {
        for (auto* xw : xWires->getChildIterator())
        {
            NodeWire w;
            w.fromNode = xw->getIntAttribute("from");
            w.toNode   = xw->getIntAttribute("to");
            wires.push_back(w);

            const PluginNode *fr = nullptr, *to = nullptr;
            for (const auto& nd : nodes)
            {
                if (nd.id == w.fromNode) fr = &nd;
                if (nd.id == w.toNode)   to = &nd;
            }
            if (fr && to && fr->graphNodeId.uid != 0 && to->graphNodeId.uid != 0)
            {
                DBG("Restoring wire: " << fr->name << " -> " << to->name);
                addGraphConnection(*fr, *to);
            }
        }
    }

    graph->rebuild();
    repaint();
}

void NodeGraphCanvas::updateInOutNames() {
    auto deviceSetup = deviceManager->getAudioDeviceSetup();

    for (auto& nd : nodes) {
        if (nd.type == NodeType::Input) {
            nd.name = deviceSetup.inputDeviceName;
        } else if (nd.type == NodeType::Output) {
            nd.name = deviceSetup.outputDeviceName;
        }
    }

    onGraphChanged();
    repaint();
}

void NodeGraphCanvas::onGraphChanged() {
    if (posted) return;
    posted++;

    MessageManager::callAsync([this] {
        if (auto xml = getState()) {
            AppProperties::get()->setValue("nodeGraphState", xml.get());
            AppProperties::saveIfNeeded();
        }
        posted = 0;
    });
}
