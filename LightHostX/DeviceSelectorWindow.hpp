//
//  DeviceSelectorWindow.hpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#ifndef DeviceSelectorWindow_hpp
#define DeviceSelectorWindow_hpp

#include "JuceHeader.h"
#include "DeviceSelectorDialog.hpp"

//==============================================================================
// 音訊裝置選擇視窗（DeviceSelectorWindow）
// 說明：
//   • 文檔視窗，包含 DeviceSelectorDialog 組件
//   • 提供原生標題欄、關閉按鈕和窗口控制
//   • 100% 縮放時的大小約為 500x400
//   • 其他縮放時：視窗大小 = 基礎大小 × DPI × 語言縮放因子
//   • 監控縮放因子變化並自動調整視窗大小
//==============================================================================
class DeviceSelectorWindow : public DocumentWindow
{
public:
    /// 建構函式，建立並初始化選擇器視窗
    /// @param title      視窗標題（例如 "音訊輸入" 或 "音訊輸出"）
    /// @param dm         音訊裝置管理器
    /// @param maxIn      最大輸入聲道數
    /// @param maxOut     最大輸出聲道數
    /// @param cb         使用者選擇裝置後調用的回調函式
    DeviceSelectorWindow(const String &title, AudioDeviceManagerSP dm,
                         int maxIn, int maxOut,
                         std::function<void(const String &)> cb);

    /// 解構函式
    ~DeviceSelectorWindow() override {}

    /// 關閉視窗並執行清理操作
    void closeWindow();

    /// DocumentWindow 虛函數：當用戶點擊視窗關閉按鈕時調用
    void closeButtonPressed() override;

    /// 根據當前縮放因子更新視窗大小
    void updateWindowSize();

private:
    /// 使用者選擇裝置時調用的回調函式
    std::function<void(const String &)> onConfirmCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceSelectorWindow)
};

#endif /* DeviceSelectorWindow_hpp */
