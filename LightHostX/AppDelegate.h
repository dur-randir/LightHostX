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
#import "AudioPowerManager.hpp"
#import "LightHostLookAndFeel.h"

@interface AppDelegate : NSObject<NSApplicationDelegate>
{
    ScopedJuceInitialiser_GUI* juceMessageThread;
    NSMenu* appMenu;

    std::shared_ptr<AudioDeviceManager> deviceManager;
    std::shared_ptr<AudioProcessorGraph> graph;
    std::unique_ptr<AudioProcessorPlayer> player;
    std::unique_ptr<AudioPluginFormatManager> formatManager;
    std::unique_ptr<KnownPluginList> knownPluginList;
    std::unique_ptr<lighthost::ui::LightHostLookAndFeel> lookAndFeel;

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<MainWindowContent> mainContent;
    std::unique_ptr<PluginListWindow> pluginListWindow;

    std::unique_ptr<AudioPowerManager> powerManager;
}
@end
