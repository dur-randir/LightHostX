//
//  MainWindow.hpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#ifndef MainWindow_hpp
#define MainWindow_hpp

#include "JuceHeader.h"
#include "MainWindowContent.hpp"

class IconMenu;
class MainWindow : public DocumentWindow
{
public:
    MainWindow(MainWindowContent* content);
    void closeButtonPressed() override;
    bool keyPressed(const KeyPress& key) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

#endif /* MainWindow_hpp */
