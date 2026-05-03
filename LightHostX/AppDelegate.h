//
//  AppDelegate.h
//  LightHostX
//
//  Created by dur-randir on 10.08.2024.
//

#import <Cocoa/Cocoa.h>
#import "JuceHeader.h"
#import "AppProperties.hpp"
#import "PluginWindow.hpp"
#import "MainWindow.hpp"
#import "MainWindowContent.hpp"
#import "PluginListWindow.hpp"

using IOProcessor = AudioProcessorGraph::AudioGraphIOProcessor;

@interface AppDelegate : NSObject<NSApplicationDelegate>
{
    ScopedJuceInitialiser_GUI* juceMessageThread;
    NSUserDefaults* settings;
    NSMenu* appMenu;
    LookAndFeel_V4 lookAndFeel;

    std::shared_ptr<AudioDeviceManager> deviceManager;
    AudioPluginFormatManager formatManager;
    KnownPluginList knownPluginList;
    AudioProcessorGraph graph;
    AudioProcessorPlayer player;
    AudioProcessorGraph::Node::Ptr inputNode;
    AudioProcessorGraph::Node::Ptr outputNode;
    
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<MainWindowContent> mainContent;
    std::unique_ptr<PluginListWindow> pluginListWindow;
}
@end
