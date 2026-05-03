//
//  DeviceSelectorDialog.cpp
//  LightHostX
//
//  Created by dur-randir on 03.05.2026.
//

#include "DeviceSelectorDialog.hpp"

//==============================================================================
// DeviceSelectorDialog 實現
// 顯示 JUCE AudioDeviceSelectorComponent 並應用動態縮放
//==============================================================================

/// 建構函數：初始化對話框，創建音訊設備選擇器並應用縮放
DeviceSelectorDialog::DeviceSelectorDialog(AudioDeviceManagerSP dm, int maxIn, int maxOut)
    : mgr(dm), initialMaxIn(maxIn), initialMaxOut(maxOut)
{
    updateSelectorComponent();
}

/// 解構函數
DeviceSelectorDialog::~DeviceSelectorDialog()
{
    if (sel) {
        sel->setLookAndFeel(nullptr);
    }
}

void DeviceSelectorDialog::updateSelectorComponent()
{
    // 如果已經存在舊的 selector 元件，先從畫面上移除
    // 避免重複疊加或殘留舊狀態
    if (sel)
    {
        sel->setLookAndFeel(nullptr);
        removeChildComponent(sel.get());
    }
    
    // naturalItemHeight：AudioDeviceSelectorComponent 原始 item 高度
    naturalItemHeight = 0;
    
    // computedContentHeight：重建時歸零，由 async block 重新量測
    computedContentHeight = 0;
    
    // 建立新的 AudioDeviceSelectorComponent
    //
    // 參數說明：
    // mgr           : AudioDeviceManager
    // 0             : 最小輸入通道數
    // initialMaxIn  : 最大輸入通道數
    // 0             : 最小輸出通道數
    // initialMaxOut : 最大輸出通道數
    // false ×4     : 不顯示 MIDI、音訊設定、進階選項
    sel = std::make_unique<AudioDeviceSelectorComponent>(
        *mgr, 0, initialMaxIn, 0, initialMaxOut, false, false, false, false
    );

    //auto& selectorLookAndFeel = getScaledSelectorLookAndFeel();
    //sel->setLookAndFeel(&selectorLookAndFeel);
    
    // 加入畫面並設為可見
    addAndMakeVisible(sel.get());
    
    sel->setSize(420, 2000);
    int maxBottom = 0;
    for (int i = 0; i < sel->getNumChildComponents(); ++i)
    {
        auto* c = sel->getChildComponent(i);
        if (c->isVisible())
            maxBottom = jmax(maxBottom, c->getBottom());
    }
    computedContentHeight = (maxBottom > 30) ? maxBottom : 300;
}

// ---- private helpers ----

String DeviceSelectorDialog::getCurrentDeviceName() const
{
    String name = "<Unknown>";
    if (auto *d = mgr->getCurrentAudioDevice()) {
        name = d->getName();
    }
    return name;
}

void DeviceSelectorDialog::getPreferredSize(int &outWidth, int &outHeight) const
{
    outWidth = 420;

    // 優先使用動態量測、否則用估算値
    int contentH = (computedContentHeight > 30)
                       ? computedContentHeight
                       : 300;

    outHeight = contentH + 12;

    // 上限：不超過主螢幕可用區高度 − 50px
    auto &displays = Desktop::getInstance().getDisplays();
    if (auto *primary = displays.getPrimaryDisplay())
    {
        int screenH = primary->userArea.getHeight();
        outHeight = jmin(outHeight, screenH - 50);
    }
}
