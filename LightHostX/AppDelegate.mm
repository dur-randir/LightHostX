//
//  AppDelegate.m
//  LightHostX
//
//  Created by dur-randir on 10.08.2024.
//

#import "AppDelegate.h"

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // Insert code here to initialize your application
    
    NSStatusBar *bar = [NSStatusBar systemStatusBar];
    NSStatusItem *item = [bar statusItemWithLength:NSSquareStatusItemLength];
    item.button.image = [NSImage imageNamed:@"MenuIcon"];
    [item.button.image setTemplate:TRUE];
    item.button.title = @"LightHostX";
    
    appMenu = [NSMenu new];
    [item setMenu:appMenu];
    [item retain];
    
    juceMessageThread = new ScopedJuceInitialiser_GUI();
    
    NSString* osxMode = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
    
    // getMidnightColourScheme with dark menu bg
    lookAndFeel.setColourScheme((osxMode && [osxMode isEqualToString:@"Dark"]) ? LookAndFeel_V4::ColourScheme({ 0xff2f2f3a, 0xff191926, 0xff191926,
        0xff66667c, 0xc8ffffff, 0xffd8d8d8,
        0xffffffff, 0xff606073, 0xffd6d6d6 }) : LookAndFeel_V4::getLightColourScheme());
    LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
    
    settings = [NSUserDefaults standardUserDefaults];
    
    deviceManager = std::make_shared<AudioDeviceManager>();
    
    // Audio device
    formatManager.addDefaultFormats();
    
    // Audio device
    std::unique_ptr<XmlElement> savedAudioState (AppProperties::get()->getXmlValue("audioDeviceState"));
    deviceManager->initialise(2, 2, savedAudioState.get(), true);
    
    // Plugins - all
    std::unique_ptr<XmlElement> savedPluginList(AppProperties::get()->getXmlValue("pluginList"));
    if (savedPluginList) {
        knownPluginList.recreateFromXml(*savedPluginList);
    }
    knownPluginList.addChangeListener(self);
    
    // Setup the main content and bind the graph change callback for saving
    mainContent = std::make_unique<MainWindowContent>(deviceManager, knownPluginList, formatManager, graph);
    
    [self loadActivePlugins];
    [self fillMenu];
    
    std::unique_ptr<XmlElement> savedGraphState(AppProperties::get()->getXmlValue("nodeGraphState"));
    if (savedGraphState) {
        mainContent->loadState(*savedGraphState);
    }
    
    deviceManager->addAudioCallback(&player);
    player.setProcessor(&graph);

}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    PluginWindow::closeAllCurrentlyOpenWindows();

    [self savePluginStates];

    // clear window before tearing down device manager & graph
    mainWindow.reset();
    mainContent.reset();

    player.setProcessor(nil);
    deviceManager->removeAudioCallback(&player);
    
    deviceManager = nil;
    inputNode = nil;
    outputNode = nil;

    LookAndFeel::setDefaultLookAndFeel(nullptr);
    delete juceMessageThread;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    return YES;
}

- (void)loadActivePlugins {
    // Set up the graph's fixed I/O nodes.
    // Audio routing is now driven by the NodeGraphCanvas UI.
    inputNode  = graph.addNode(std::make_unique<IOProcessor>(IOProcessor::audioInputNode),
                     AudioProcessorGraph::NodeID(NodeGraphCanvas::kInputNodeUID));
    outputNode = graph.addNode(std::make_unique<IOProcessor>(IOProcessor::audioOutputNode),
                     AudioProcessorGraph::NodeID(NodeGraphCanvas::kOutputNodeUID));

    double sr = 44100.0;
    int    bs = 512;
    if (auto* dev = deviceManager->getCurrentAudioDevice())
    {
        sr = dev->getCurrentSampleRate();
        bs = dev->getCurrentBufferSizeSamples();
    }

    inputNode->getProcessor()->setPlayConfigDetails(1, 2, sr, bs);
    inputNode->getProcessor()->prepareToPlay(sr, bs);
    outputNode->getProcessor()->setPlayConfigDetails(2, 2, sr, bs);
    outputNode->getProcessor()->prepareToPlay(sr, bs);
    
    // NOTE: Plugin loading and connection routing is now handled by
    // the NodeGraphCanvas (MainWindowContent). Draw wires in the canvas
    // to route audio: Input → Plugin → Output.
}

- (void)showEditGraph {
    if (!mainWindow) {
        mainWindow = std::make_unique<MainWindow>(mainContent.get());
    } else {
        mainWindow->toFront(true);
    }
}

- (void)showEditPlugins {
    [self reloadPlugins];
}

- (void)fillMenu {
    [appMenu removeAllItems];

    [appMenu addItemWithTitle: @"Edit graph" action:@selector(showEditGraph)keyEquivalent:@"P"];
    [appMenu addItemWithTitle: @"Plugin Finder" action:@selector(showEditPlugins)keyEquivalent:@"E"];

    [appMenu addItem:[NSMenuItem separatorItem]];

    [appMenu addItemWithTitle: @"Delete Plugin States" action:@selector(terminate:) keyEquivalent:@"d"];
    [appMenu addItemWithTitle: @"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
}

- (void)savePluginStates {
    // The graph now saves via nodeGraphState XML which includes plugin state info
    // This method iterates actual graph nodes and creates a plugin state backup
    // in case the node graph XML gets corrupted
    
    for (const auto* node : graph.getNodes())
    {
        if (node == nullptr || node->getProcessor() == nullptr)
            continue;
            
        auto* proc = node->getProcessor();
        bool isInputOrOutput = (dynamic_cast<IOProcessor*>(proc) != nullptr);
        if (isInputOrOutput)
            continue;  // Skip input/output nodes
    }
    
    AppProperties::saveIfNeeded();
}

- (void)reloadPlugins {
    if (!pluginListWindow) {
        pluginListWindow.reset(new PluginListWindow(knownPluginList, formatManager));
    }
    pluginListWindow->toFront(true);
}

- (void)removePluginsLackingInputOutput {
#if 0
    std::vector<PluginDescription> removeIndex;
    for (auto& plugin : knownPluginList.getTypes()) {
        if (plugin.numInputChannels < 2 || plugin.numOutputChannels < 2) {
            removeIndex.push_back(plugin);
        }
    }
    for (auto& plugin : removeIndex) {
        knownPluginList.removeType(plugin);
    }
#endif
}

- (NSURL*)getSupportFolder {
    NSError *error;
    NSFileManager *manager = [NSFileManager defaultManager];
    NSURL *applicationSupport = [manager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:false error:&error];
    NSString *identifier = [[NSBundle mainBundle] bundleIdentifier];
    NSURL *folder = [applicationSupport URLByAppendingPathComponent:identifier];
    [manager createDirectoryAtURL:folder withIntermediateDirectories:true attributes:nil error:&error];
    return folder;
}
    
- (String)getFilePath:(NSString*)name {
    NSURL* fullPath = [[self getSupportFolder] URLByAppendingPathComponent:name];
    return [fullPath.path UTF8String];
}

- (NSString*)getKey:(NSString*)type :(PluginDescription)plugin {
    return [NSString stringWithFormat:@"plugin-%@-%s%s%s", [type lowercaseString], plugin.name.toStdString().c_str(), plugin.version.toStdString().c_str(), plugin.pluginFormatName.toStdString().c_str()];
}

- (std::unique_ptr<XmlElement>)getXmlValue:(NSString*)keyName {
    NSString* xml = [settings stringForKey:keyName];
    //return parseXML([xml cStringUsingEncoding:NSUTF8StringEncoding]);
    return parseXML([xml UTF8String]);
}

- (BOOL)getBoolValue:(NSString*)keyName {
    return [settings boolForKey:keyName];
}

- (NSString*)getValue:(NSString*)keyName {
    NSString* ret = [settings stringForKey:keyName];
    return ret;
}

- (NSInteger)getIntValue:(NSString*)keyName {
    return [settings integerForKey:keyName];
}

- (void)setValue:(NSString*)keyName :(const String&)data {
    if ((0))
    [settings setValue:[NSString stringWithUTF8String:data.toStdString().c_str()] forKey:keyName];
}

- (void)removeValue:(NSString*)keyName {
    [settings removeObjectForKey:keyName];
}

@end
