//
//  DeviceSelectorWindow.cpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#include "DeviceSelectorWindow.hpp"

//==============================================================================
// DeviceSelectorWindow 實現
// 可調整大小的視窗，包含 DeviceSelectorDialog
//==============================================================================

/// 建構函數：創建視窗並初始化對話框組件
DeviceSelectorWindow::DeviceSelectorWindow(const String &title, AudioDeviceManagerSP dm, int maxIn, int maxOut, std::function<void(const String &)> cb)
    : DocumentWindow(title, Colours::lightgrey,
        DocumentWindow::closeButton | DocumentWindow::minimiseButton, true),
        onConfirmCallback(std::move(cb)
    )
{
    // 創建並設定對話框組件
    auto *dlg = new DeviceSelectorDialog(dm, maxIn, maxOut);
    dlg->onScaleChanged = [this]
    { updateWindowSize(); };

    // 設定視窗風格
    setContentOwned(dlg, true);   // 視窗擁有對話框生命週期
    setUsingNativeTitleBar(true); // 使用 Windows 原生標題欄
    setResizable(false, false);   // 禁用調整大小（由計時器控制）
    setBackgroundColour(Colour::fromRGB(236, 236, 236));

    // 初始化視窗大小
    updateWindowSize();
    setTopLeftPosition(250, 150); // 預設視窗左上角座標
}

/// 關閉視窗並執行清理
void DeviceSelectorWindow::closeWindow()
{
    // 當用戶點擊窗口的X按鈕時，獲取當前選擇的設備並調用回調
    if (auto *dlg = dynamic_cast<DeviceSelectorDialog *>(getContentComponent()))
    {
        String name = dlg->getCurrentDeviceName();
        if (onConfirmCallback)
            onConfirmCallback(name);
    }

    removeFromDesktop();
    delete this; // 自我銷毀（由 addToDesktop 管理的視窗）
}

/// DocumentWindow 虛函數：當用戶點擊視窗關閉按鈕時調用
void DeviceSelectorWindow::closeButtonPressed()
{
    closeWindow();
}

void DeviceSelectorWindow::updateWindowSize()
{
    if (auto *dlg = dynamic_cast<DeviceSelectorDialog *>(getContentComponent()))
    {
        int w = 0, h = 0;
        dlg->getPreferredSize(w, h);
        setSize(w, h);
    }
}
