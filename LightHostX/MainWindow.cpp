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

    // We're an LSUIElement app (no Dock icon / Cmd-Tab entry), so a normal-level
    // window gets buried under other apps' windows on deactivation with no way back.
    // Float it (NSFloatingWindowLevel) so it stays visible when focus moves elsewhere.
    // Any window opened from the editor must be always-on-top too, or it'll open behind.
    setAlwaysOnTop(true);
}

void MainWindow::closeButtonPressed() {
    this->setVisible(false);
}

void MainWindow::visibilityChanged() {
    DocumentWindow::visibilityChanged();

    if (onVisibilityChanged) {
        onVisibilityChanged(isVisible() && !isMinimised());
    }
}

void MainWindow::minimisationStateChanged(bool isNowMinimised) {
    DocumentWindow::minimisationStateChanged(isNowMinimised);

    if (onVisibilityChanged) {
        onVisibilityChanged(isVisible() && !isNowMinimised);
    }
}

bool MainWindow::keyPressed(const KeyPress& key)
{
    if (key.getKeyCode() == KeyPress::escapeKey) {
        closeButtonPressed();
        return true;
    }
    return false;
}
