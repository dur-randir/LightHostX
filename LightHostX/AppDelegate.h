//
//  AppDelegate.h
//  LightHostX
//
//  Created by dur-randir on 10.08.2024.
//

#import <Cocoa/Cocoa.h>
#import "JuceHeader.h"
#import "PluginWindow.hpp"

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
    ScopedJuceInitialiser_GUI* juceMessageThread;
    NSUserDefaults* settings;
    std::unique_ptr<LookAndFeel_V4> lookAndFeel;
    NSMenu* appMenu;
    DialogWindow* wPrefs;
    DialogWindow* wPlugins;
    
    std::unique_ptr<AudioDeviceManager> deviceManager;
    std::unique_ptr<AudioPluginFormatManager> formatManager;
    std::unique_ptr<KnownPluginList> knownPluginList;
    std::unique_ptr<KnownPluginList> activePluginList;
    std::unique_ptr<AudioProcessorGraph> graph;
    std::unique_ptr<AudioProcessorPlayer> player;
    AudioProcessorGraph::Node::Ptr inputNode;
    AudioProcessorGraph::Node::Ptr outputNode;
}
@end
