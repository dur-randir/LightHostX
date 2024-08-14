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
    item.button.title = @"";
    
    appMenu = [NSMenu new];
    [item setMenu:appMenu];
    [item retain];
    
    juceMessageThread = new ScopedJuceInitialiser_GUI();
    wPrefs = nil;
    wPlugins = nil;

    lookAndFeel = std::make_unique<LookAndFeel_V4>();
    lookAndFeel->setColourScheme(LookAndFeel_V4::getMidnightColourScheme());
    LookAndFeel::setDefaultLookAndFeel(lookAndFeel.get());
    
    settings = [NSUserDefaults standardUserDefaults];

    deviceManager = std::make_unique<AudioDeviceManager>();
    formatManager = std::make_unique<AudioPluginFormatManager>();
    knownPluginList = std::make_unique<KnownPluginList>();
    activePluginList = std::make_unique<KnownPluginList>();
    graph = std::make_unique<AudioProcessorGraph>();
    player = std::make_unique<AudioProcessorPlayer>();

    // Audio device
    formatManager.get()->addDefaultFormats();
    std::unique_ptr<XmlElement> savedAudioState = [self getXmlValue:@"audioDeviceState"];
    deviceManager.get()->initialise(256, 256, savedAudioState.get(), true);
    player->setProcessor(graph.get());
    deviceManager->addAudioCallback(player.get());

    // Plugins - all
    std::unique_ptr<XmlElement> savedPluginList = [self getXmlValue:@"pluginList"];
    if (savedPluginList != nil){
        knownPluginList->recreateFromXml(*savedPluginList);
    }

    // Plugins - active
    std::unique_ptr<XmlElement> savedPluginListActive = [self getXmlValue:@"pluginListActive"];
    if (savedPluginListActive != nil) {
        activePluginList->recreateFromXml(*savedPluginListActive);
    }

    [self loadActivePlugins];
    [self fillMenu];
}


- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application

    if (wPrefs) {
        wPrefs->exitModalState(-1);
        delete wPrefs;
    }
    if (wPlugins) {
        wPlugins->exitModalState(-1);
        delete wPlugins;
    }
    //PluginWindow::closeAllCurrentlyOpenWindows();

    [self savePluginStates];
    
    player->setProcessor(nil);
    deviceManager->removeAudioCallback(player.get());
    
    player = nil;
    graph = nil;
    activePluginList = nil;
    knownPluginList = nil;
    formatManager = nil;
    deviceManager = nil;
    
    inputNode = nil;
    outputNode = nil;

    lookAndFeel = nil;
    delete juceMessageThread;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    return YES;
}

- (void)loadActivePlugins {
    AudioProcessorGraph::NodeID INPUT(1000000);
    AudioProcessorGraph::NodeID OUTPUT(INPUT.uid + 1);
    const int CHANNEL_ONE = 0;
    const int CHANNEL_TWO = 1;
    
    graph->clear();
    inputNode = graph->addNode(std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode), INPUT);
    outputNode = graph->addNode(std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode), OUTPUT);
    
    if (activePluginList->getNumTypes() == 0) {
        graph->addConnection({{INPUT, CHANNEL_ONE}, {OUTPUT, CHANNEL_ONE}}, AudioProcessorGraph::UpdateKind::async);
        graph->addConnection({{INPUT, CHANNEL_TWO}, {OUTPUT, CHANNEL_TWO}}, AudioProcessorGraph::UpdateKind::async);
        
        graph->rebuild();
        return;
    }
    
    // NOTE: Node ids cannot begin at 0.
    AudioProcessorGraph::NodeID lastId(0);
    bool hasInputConnected = false;

    auto sortedPluginList = [self getActiveSortedList];
    std::sort(sortedPluginList.begin(), sortedPluginList.end(), [&](const PluginDescription& a, const PluginDescription& b){
        return [self getIntValue:[self getKey:@"order" :a]] < [self getIntValue:[self getKey:@"order" :b]];
    });

    for (auto& plugin : sortedPluginList) {
        String errorMessage;
        std::unique_ptr<AudioPluginInstance> instance = formatManager->createPluginInstance(plugin, graph->getSampleRate(), graph->getBlockSize(), errorMessage);
        NSString* pluginUid = [self getKey:@"state" :plugin];
        if (!instance) {
            [self removeValue:pluginUid];
            continue;
        }
        NSString* savedPluginState = [self getValue:pluginUid];
        if (savedPluginState) {
            MemoryBlock savedPluginBinary;
            savedPluginBinary.fromBase64Encoding([savedPluginState UTF8String]);
            instance->setStateInformation(savedPluginBinary.getData(), (int)savedPluginBinary.getSize());
        }

        AudioProcessorGraph::NodeID nextId(lastId.uid + 1);
        graph->addNode(std::move(instance), nextId);
        NSString* key = [self getKey:@"bypass" :plugin];
        bool bypass = [self getBoolValue:key];
        
        // Input to plugin
        if ((!hasInputConnected) && (!bypass)) {
            graph->addConnection({{INPUT, CHANNEL_ONE}, {nextId, CHANNEL_ONE}}, AudioProcessorGraph::UpdateKind::async);
            graph->addConnection({{INPUT, CHANNEL_TWO}, {nextId, CHANNEL_TWO}}, AudioProcessorGraph::UpdateKind::async);
            hasInputConnected = true;
        }
        // Connect previous plugin to current
        else if (!bypass) {
            graph->addConnection({{lastId, CHANNEL_ONE}, {nextId, CHANNEL_ONE}}, AudioProcessorGraph::UpdateKind::async);
            graph->addConnection({{lastId, CHANNEL_TWO}, {nextId, CHANNEL_TWO}}, AudioProcessorGraph::UpdateKind::async);
        }
        if (!bypass) {
            lastId = nextId;
        }
    }
    
    if (lastId.uid > 0) {
        // Last active plugin to output
        graph->addConnection({{lastId, CHANNEL_ONE}, {OUTPUT, CHANNEL_ONE}}, AudioProcessorGraph::UpdateKind::async);
        graph->addConnection({{lastId, CHANNEL_TWO}, {OUTPUT, CHANNEL_TWO}}, AudioProcessorGraph::UpdateKind::async);
    }
    
    graph->rebuild();
}

- (void)savePluginStates {
    auto sortedPluginList = [self getActiveSortedList];
    AudioProcessorGraph::NodeID id(1);
    
    for (auto& plugin : sortedPluginList) {
        AudioProcessorGraph::Node* node = graph->getNodeForId(id);
        if (node == nullptr) {
            continue; // EARLY CALL TO SAVE STATE??
        }
        AudioProcessor& processor = *node->getProcessor();
        NSString* key = [self getKey:@"state" :plugin];
        MemoryBlock savedStateBinary;
        processor.getStateInformation(savedStateBinary);
        [self setValue:key :savedStateBinary.toBase64Encoding()];
        id.uid = id.uid + 1;
    }
}

- (void)initAudioSettings {
    AudioDeviceSelectorComponent* audioSettingsComp = new AudioDeviceSelectorComponent(*deviceManager, 0, 256, 0, 256, false, false, true, true);
    audioSettingsComp->setSize(500, 450);
    
    DialogWindow::LaunchOptions o;
    o.content.setOwned(audioSettingsComp);
    o.dialogTitle                   = "Audio Settings";
    o.dialogBackgroundColour = lookAndFeel->getCurrentColourScheme().getUIColour(LookAndFeel_V4::ColourScheme::windowBackground); // WTF
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = false;

    wPrefs = o.create();
}

- (void)showAudioSettings {
    if (!wPrefs) {
        [self initAudioSettings];
    }

    if (wPrefs->runModalLoop() != -1) {
        std::unique_ptr<XmlElement> audioState = deviceManager->createStateXml();
        //const String& state = audioState->toString();
        //[self setValue:@"audioDeviceState" :state];
    }
}

- (void)initEditPlugins {
    DialogWindow::LaunchOptions o;

    const File deadMansPedalFile([self getFilePath:@"pedal.dat"]);
    if (!deadMansPedalFile.getSize()) {
        deadMansPedalFile.create();
    }
    
    PluginListComponent* comp = new PluginListComponent(*formatManager,
                                                        *knownPluginList,
                                                        deadMansPedalFile,
                                                        nil, true);
    comp->setSize(900, 500);
    //comp->setNumberOfThreadsForScanning(4);

    o.content.setOwned(comp);
    o.dialogTitle                   = "Available Plugins";
    o.dialogBackgroundColour = lookAndFeel->getCurrentColourScheme().getUIColour(LookAndFeel_V4::ColourScheme::windowBackground); // WTF
    o.escapeKeyTriggersCloseButton  = true;
    o.resizable                     = true;
    o.useNativeTitleBar             = true;

    wPlugins = o.create();
    //wPlugins->restoreWindowStateFromString([self getValue:@""]);
}

- (void)showEditPlugins {
    if (!wPlugins) {
        [self initEditPlugins];
    }

    int ret = wPlugins->runModalLoop();
    [self removePluginsLackingInputOutput];
    
    if (ret != -1) {
        [self fillMenu];
        //[self setValue:@"" :w->getWindowStateAsString()];
    }
}

- (void)fillMenu {
    [appMenu removeAllItems];

    [appMenu addItemWithTitle: @"Preferences" action:@selector(showAudioSettings)keyEquivalent:@"P"];
    [appMenu addItemWithTitle: @"Edit Plugins" action:@selector(showEditPlugins)keyEquivalent:@"E"];
    [appMenu addItem:[NSMenuItem separatorItem]];

    if (@available(macOS 14.0, *)) {
        [appMenu addItem:[NSMenuItem sectionHeaderWithTitle:@"Active Plugins"]];
    } else {
        [[appMenu addItemWithTitle: @"Active Plugins" action:nil keyEquivalent:@""] setEnabled:NO];
    }
    auto sortedPluginList = [self getActiveSortedList];
    for (auto& plugin : sortedPluginList) {
        NSMenuItem* item = [appMenu addItemWithTitle:[NSString stringWithUTF8String:plugin.name.toStdString().c_str()] action:nil keyEquivalent:@""];
        //item.indentationLevel = 1;
        //[item.attributedTitle

        [item setRepresentedObject:[NSString stringWithUTF8String: plugin.createIdentifierString().toStdString().c_str()]];

        NSMenu *submenu = [[NSMenu alloc] init];
        [item setSubmenu:submenu];

        NSString* key = [self getKey:@"bypass" :plugin];
        BOOL bypass = [self getBoolValue:key];
        
        [submenu addItemWithTitle:@"Edit" action:nil keyEquivalent:@""];
        [[submenu addItemWithTitle:@"Bypass" action:nil keyEquivalent:@""] setState:(bypass ? NSControlStateValueOn : NSControlStateValueOff)];
        [submenu addItem:[NSMenuItem separatorItem]];
        [submenu addItemWithTitle:@"Move Up" action:nil keyEquivalent:@""];
        [submenu addItemWithTitle:@"Move Down" action:nil keyEquivalent:@""];
        [submenu addItem:[NSMenuItem separatorItem]];
        [submenu addItemWithTitle:@"Delete" action:nil keyEquivalent:@""];
    }
    [appMenu addItem:[NSMenuItem separatorItem]];

    if (@available(macOS 14.0, *)) {
        [appMenu addItem:[NSMenuItem sectionHeaderWithTitle:@"Avaliable Plugins"]];
    } else {
        [[appMenu addItemWithTitle: @"Avaliable Plugins" action:nil keyEquivalent:@""] setEnabled:NO];
    }

    auto pluginTree = KnownPluginList::createTree(knownPluginList->getTypes(), KnownPluginList::sortByManufacturer);
    for (auto& sub : pluginTree->subFolders) {
        NSMenuItem* item = [appMenu addItemWithTitle:[NSString stringWithUTF8String:sub->folder.toStdString().c_str()] action:nil keyEquivalent:@""];
        NSMenu *submenu = [[NSMenu alloc] init];
        [item setSubmenu:submenu];
        
        for (auto& plugin : sub->plugins) {
            NSMenuItem* pItem = [submenu addItemWithTitle:[NSString stringWithUTF8String:plugin.name.toStdString().c_str()] action:@selector(onClickAdd:) keyEquivalent:@""];
            [pItem setRepresentedObject:[NSString stringWithUTF8String: plugin.createIdentifierString().toStdString().c_str()]];
        }
    }

    [appMenu addItem:[NSMenuItem separatorItem]];

    [appMenu addItemWithTitle: @"Delete Plugin States" action:@selector(terminate:) keyEquivalent:@"d"];
    [appMenu addItemWithTitle: @"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
}

- (void)onClickEdit:(id)sender {
    String idString = [[[sender parentItem] representedObject] UTF8String];
    auto plugin = knownPluginList->getTypeForIdentifierString(idString);

}

- (void)onClickAdd:(id)sender {
    String idString = [[sender representedObject] UTF8String];
    auto plugin = knownPluginList->getTypeForIdentifierString(idString);
    
    if (!activePluginList->addType(*plugin)) {
        return; // already present
    }

    NSString* key = [self getKey:@"order" :*plugin];
    [self setValue:key :"time"];

    [self savePluginStates];
    [self loadActivePlugins];
    [self fillMenu];
}

- (Array<PluginDescription>)getActiveSortedList {
    auto sortedPluginList = activePluginList->getTypes();
    std::sort(sortedPluginList.begin(), sortedPluginList.end(), [&](const PluginDescription& a, const PluginDescription& b){
        return [self getIntValue:[self getKey:@"order" :a]] < [self getIntValue:[self getKey:@"order" :b]];
    });

    return sortedPluginList;
}

- (void)removePluginsLackingInputOutput {
    std::vector<PluginDescription> removeIndex;
    for (auto& plugin : knownPluginList->getTypes()) {
        if (plugin.numInputChannels < 2 || plugin.numOutputChannels < 2) {
            removeIndex.push_back(plugin);
        }
    }
    for (auto& plugin : removeIndex) {
        knownPluginList->removeType(plugin);
    }
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

