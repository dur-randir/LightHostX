//
//  MainWindow.cpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#include "MainWindow.hpp"

MainWindow::MainWindow(MainWindowContent* content)
        : DocumentWindow("LightHostX",
            Colour::fromRGB(26, 26, 26),
            DocumentWindow::minimiseButton | DocumentWindow::closeButton
          )
{
    // Don't create a new one, use the persistent one owned by IconMenu
    content->setSize(900, 560);

    // Pass ownership flag false so DocumentWindow doesn't delete it
    setContentNonOwned(content, true);

    setUsingNativeTitleBar(true);
    setResizable(true, false);
    setResizeLimits(600, 400, 4096, 4096);
    centreWithSize(900, 560);
    setTopLeftPosition(250, 150); // 預設視窗左上角座標
}

void MainWindow::closeButtonPressed() {
    this->setVisible(false);
}

bool MainWindow::keyPressed(const KeyPress& key)
{
    if (key.getKeyCode() == KeyPress::escapeKey) {
        closeButtonPressed();
        return true;
    }
    return false;
}
