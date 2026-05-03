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
    item.button.image = [NSImage imageNamed:@"OldMenuIcon"];
    [item.button.image setTemplate:TRUE];
    item.button.title = @"";

    appMenu = [NSMenu new];
    [item setMenu:appMenu];
    [item retain];
    [self fillMenu];

    juceMessageThread = new ScopedJuceInitialiser_GUI();
    
    deviceManager = std::make_shared<AudioDeviceManager>();
    graph = std::make_shared<AudioProcessorGraph>();
    player = std::make_unique<AudioProcessorPlayer>();
    formatManager = std::make_unique<AudioPluginFormatManager>();
    knownPluginList = std::make_unique<KnownPluginList>();
    lookAndFeel = std::make_unique<LookAndFeel_V3>();

    LookAndFeel::setDefaultLookAndFeel(lookAndFeel.get());

    // Audio device
    formatManager->addDefaultFormats();
    
    // Audio device
    std::unique_ptr<XmlElement> savedAudioState (AppProperties::get()->getXmlValue("audioDeviceState"));
    deviceManager->initialise(2, 2, savedAudioState.get(), true);
    
    // Plugins - all
    std::unique_ptr<XmlElement> savedPluginList(AppProperties::get()->getXmlValue("pluginList"));
    if (savedPluginList) {
        knownPluginList->recreateFromXml(*savedPluginList);
    }
    //knownPluginList.addChangeListener(self);
    
    // Setup the main content and bind the graph change callback for saving
    mainContent = std::make_unique<MainWindowContent>(deviceManager, *knownPluginList, *formatManager, graph);
        
    std::unique_ptr<XmlElement> savedGraphState(AppProperties::get()->getXmlValue("nodeGraphState"));
    if (savedGraphState) {
        mainContent->loadState(*savedGraphState);
    }

    deviceManager->addAudioCallback(player.get());
    player->setProcessor(graph.get());
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    PluginWindow::closeAllCurrentlyOpenWindows();

    [self savePluginStates];

    // clear window before tearing down device manager & graph
    mainWindow = nullptr;
    mainContent = nullptr;
    pluginListWindow = nullptr;

    player->setProcessor(nullptr);
    deviceManager->removeAudioCallback(player.get());
    
    deviceManager = nullptr;
    player = nullptr;
    graph = nullptr;

    formatManager = nullptr;
    knownPluginList = nullptr;
    lookAndFeel = nullptr;

    AppProperties::destroy();
    LookAndFeel::setDefaultLookAndFeel(nullptr);
    delete juceMessageThread;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    return YES;
}

- (void)showEditGraph {
    if (!mainWindow) {
        mainWindow = std::make_unique<MainWindow>(mainContent.get());
        mainWindow->setAlwaysOnTop(true);
    }

    mainWindow->setVisible(true);
    mainWindow->grabKeyboardFocus();
}

- (void)showEditPlugins {
    if (!pluginListWindow) {
        pluginListWindow = std::make_unique<PluginListWindow>(*knownPluginList, *formatManager);
        pluginListWindow->setAlwaysOnTop(true);
    }

    pluginListWindow->setVisible(true);
    pluginListWindow->grabKeyboardFocus();
}

- (void)fillMenu {
    [appMenu removeAllItems];

    [appMenu addItemWithTitle: @"Edit Graph" action:@selector(showEditGraph)keyEquivalent:@"E"];
    [appMenu addItemWithTitle: @"Plugin Finder" action:@selector(showEditPlugins)keyEquivalent:@"P"];

    [appMenu addItem:[NSMenuItem separatorItem]];

    [appMenu addItemWithTitle: @"Delete Plugin States" action:@selector(terminate:) keyEquivalent:@"d"];
    [appMenu addItemWithTitle: @"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
}

- (void)savePluginStates {
    // The graph now saves via nodeGraphState XML which includes plugin state info
    // This method iterates actual graph nodes and creates a plugin state backup
    // in case the node graph XML gets corrupted
    
    for (const auto* node : graph->getNodes())
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

@end
