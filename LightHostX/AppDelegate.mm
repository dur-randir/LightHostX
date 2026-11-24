//
//  AppDelegate.m
//  LightHostX
//
//  Created by dur-randir on 10.08.2024.
//

#import "AppDelegate.h"

static constexpr int kNumChannels = 2;

void lighthost::ui::applyDarkWindowAppearance(juce::Component& topLevelWindow) {
    if (auto* peer = topLevelWindow.getPeer()) {
        NSView* view = (NSView*) peer->getNativeHandle();
        if (NSWindow* window = view.window) {
            window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        }
    }
}

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

    juceMessageThread = new ScopedJuceInitialiser_GUI();

    deviceManager = std::make_shared<AudioDeviceManager>();
    graph = std::make_shared<AudioProcessorGraph>();
    player = std::make_unique<AudioProcessorPlayer>();
    formatManager = std::make_unique<AudioPluginFormatManager>();
    knownPluginList = std::make_unique<KnownPluginList>();
    lookAndFeel = std::make_unique<lighthost::ui::LightHostLookAndFeel>();

    LookAndFeel::setDefaultLookAndFeel(lookAndFeel.get());

    // Audio device
    addDefaultFormatsToManager(*formatManager);

    // Audio device
    std::unique_ptr<XmlElement> savedAudioState (AppProperties::get()->getXmlValue("audioDeviceState"));
    deviceManager->initialise(kNumChannels, kNumChannels, savedAudioState.get(), true);

    // Plugins - all
    std::unique_ptr<XmlElement> savedPluginList(AppProperties::get()->getXmlValue("pluginList"));
    if (savedPluginList) {
        knownPluginList->recreateFromXml(*savedPluginList);
        knownPluginList->sort(KnownPluginList::SortMethod::sortAlphabetically, true);
    }

    // Setup the main content and bind the graph change callback for saving
    mainContent = std::make_unique<MainWindowContent>(deviceManager, *knownPluginList, *formatManager, graph);
    mainContent->onManagePlugins = [self] { [self showEditPlugins ]; };

    std::unique_ptr<XmlElement> savedGraphState(AppProperties::get()->getXmlValue("nodeGraphState"));
    if (savedGraphState) {
        mainContent->loadState(*savedGraphState);
    }

    deviceManager->addAudioCallback(player.get());
    player->setProcessor(graph.get());

    // Stops the audio device when nothing consumes our output
    powerManager = std::make_unique<AudioPowerManager>(*deviceManager, kNumChannels, kNumChannels);

    NSNotificationCenter* wsnc = NSWorkspace.sharedWorkspace.notificationCenter;
    [wsnc addObserver:self selector:@selector(systemWillSleep:) name:NSWorkspaceWillSleepNotification object:nil];
    [wsnc addObserver:self selector:@selector(systemDidWake:) name:NSWorkspaceDidWakeNotification object:nil];

    [self fillMenu];
}

- (void)systemWillSleep:(NSNotification *)notification {
    if (powerManager) {
        powerManager->systemWillSleep();
    }
}

- (void)systemDidWake:(NSNotification *)notification {
    if (powerManager) {
        powerManager->systemDidWake();
    }
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:self];

    PluginWindow::closeAllCurrentlyOpenWindows();

    // clear window before tearing down device manager & graph
    mainWindow = nullptr;
    mainContent = nullptr;
    pluginListWindow = nullptr;

    powerManager = nullptr;

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
        mainWindow->onVisibilityChanged = [self](bool visible) {
            if (self->powerManager) {
                self->powerManager->setEditorVisible(visible);
            }
        };
    }

    Process::makeForegroundProcess();
    mainWindow->addToDesktop();
    lighthost::ui::applyDarkWindowAppearance(*mainWindow);
    mainWindow->setVisible(true);
    mainWindow->toFront(true);
}

- (void)showEditPlugins {
    if (!pluginListWindow) {
        pluginListWindow = std::make_unique<PluginListWindow>(*knownPluginList, *formatManager);
    }

    Process::makeForegroundProcess();
    pluginListWindow->addToDesktop();
    lighthost::ui::applyDarkWindowAppearance(*pluginListWindow);
    pluginListWindow->setVisible(true);
    pluginListWindow->toFront(true);
}

- (void)toggleRunOnlyWhenNeeded:(NSMenuItem*)sender {
    if (!powerManager) return;

    const bool enabled = !powerManager->getRunOnlyWhenNeeded();
    powerManager->setRunOnlyWhenNeeded(enabled);
    sender.state = enabled ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)fillMenu {
    [appMenu removeAllItems];

    [appMenu addItemWithTitle: @"Edit Graph" action:@selector(showEditGraph)keyEquivalent:@"E"];
    [appMenu addItemWithTitle: @"Plugin Finder" action:@selector(showEditPlugins)keyEquivalent:@"P"];

    [appMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* runItem = [appMenu addItemWithTitle: @"Run Only When Needed" action:@selector(toggleRunOnlyWhenNeeded:) keyEquivalent:@""];
    runItem.state = (powerManager && powerManager->getRunOnlyWhenNeeded()) ? NSControlStateValueOn : NSControlStateValueOff;

    [appMenu addItem:[NSMenuItem separatorItem]];

    //[appMenu addItemWithTitle: @"Delete Plugin States" action:@selector(terminate:) keyEquivalent:@"d"];
    [appMenu addItemWithTitle: @"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
}

@end
