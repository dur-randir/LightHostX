//
//  AudioPowerManager.hpp
//  LightHostX
//
//  Opens the audio device only while someone needs our output:
//    - another process is reading the output (loopback) device, or
//    - the Edit Graph window is shown, or
//    - "Run only when needed" is switched off.
//  Starts immediately when needed; stops after a grace period otherwise.
//  Also persists the chosen audio device setup.
//

#ifndef AudioPowerManager_hpp
#define AudioPowerManager_hpp

#include "JuceHeader.h"
#include "ConsumerMonitor.hpp"

class AudioPowerManager : private ChangeListener, private Timer
{
public:
    static constexpr int kGracePeriodMs = 60 * 1000;
    static constexpr int kWakeRetryMs   = 5 * 1000;

    // numInputs/numOutputs must match what deviceManager.initialise() was called with.
    AudioPowerManager(AudioDeviceManager& deviceManager, int numInputs, int numOutputs);
    ~AudioPowerManager() override;

    void setEditorVisible(bool visible);

    bool getRunOnlyWhenNeeded() const { return runOnlyWhenNeeded; }
    void setRunOnlyWhenNeeded(bool enabled);

private:
    AudioDeviceManager& deviceManager;
    const int numInputs, numOutputs;
    ConsumerMonitor monitor;

    // Fallback wake setup when the user never explicitly chose a device.
    AudioDeviceManager::AudioDeviceSetup sleptSetup;
    bool sleeping = false;
    bool editorVisible = false;
    bool runOnlyWhenNeeded = true;

    bool isNeeded() const;
    String intendedOutputName() const;
    void update();
    void wake();
    void sleep();

    void changeListenerCallback(ChangeBroadcaster*) override;
    // Doubles as the idle-shutdown timer (while awake) and the wake-retry timer (while asleep).
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPowerManager)
};

#endif /* AudioPowerManager_hpp */
