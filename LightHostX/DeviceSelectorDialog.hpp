//
//  DeviceSelectorDialog.hpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#ifndef DeviceSelectorDialog_hpp
#define DeviceSelectorDialog_hpp

#include "JuceHeader.h"

using AudioDeviceManagerSP = std::shared_ptr<AudioDeviceManager>;

//==============================================================================
// 音訊裝置選擇對話內容組件（DeviceSelectorDialog）
// 說明：
//   • 包含 JUCE 的 AudioDeviceSelectorComponent 並應用動態縮放
//   • 根據 DPI 和語言設定自動縮放列表項高度和字體大小
//   • 100% 基線 = 原始 JUCE AudioDeviceSelectorComponent（無人工置中）
//   • 作為 DeviceSelectorWindow 的內容組件運作
//   • 視窗關閉按鈕由父視窗管理
//==============================================================================
class DeviceSelectorDialog : public juce::Component
{
public:
    /// 建構函式
    /// @param dm     音訊裝置管理器
    /// @param maxIn  最大輸入聲道數（0 表示隱藏輸入選項）
    /// @param maxOut 最大輸出聲道數（0 表示隱藏輸出選項）
    DeviceSelectorDialog(AudioDeviceManagerSP dm, int maxIn, int maxOut);

    /// 解構函式
    ~DeviceSelectorDialog() override;

    /// 重新建立 AudioDeviceSelectorComponent 實例並應用縮放
    void updateSelectorComponent();

    /// 取得當前選擇的音訊設備名稱
    String getCurrentDeviceName() const;

    /// 取得此對話框的偏好大小（用於窗口尺寸計算）
    /// 會在首次 layout 完成後包含動態量測的實際高度
    void getPreferredSize(int &outWidth, int &outHeight) const;

private:
    /// JUCE 音訊裝置選擇器組件
    std::unique_ptr<AudioDeviceSelectorComponent> sel;

    /// 音訊裝置管理器參考
    AudioDeviceManagerSP mgr;

    /// 初始時的最大輸入/輸出聲道數
    int initialMaxIn, initialMaxOut;

    /// 選擇器的原始列表項高度
    int naturalItemHeight = 0;

    /// 動態量測的實際內容高度（px），0 表示尚未量測
    int computedContentHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceSelectorDialog)
};

#endif /* DeviceSelectorDialog_hpp */
