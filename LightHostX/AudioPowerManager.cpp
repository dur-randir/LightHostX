//
//  AudioPowerManager.cpp
//  LightHostX
//

#include "AudioPowerManager.hpp"
#include "AppProperties.hpp"
#include <os/log.h>

namespace
{
    os_log_t logger()
    {
        static os_log_t l = os_log_create("com.randir.LightHostX", "AudioPower");
        return l;
    }

    constexpr const char* kRunOnlyWhenNeededKey = "runOnlyWhenNeeded";
    constexpr const char* kAudioDeviceStateKey  = "audioDeviceState";
}

AudioPowerManager::AudioPowerManager(AudioDeviceManager& dm, int ins, int outs)
    : deviceManager(dm), numInputs(ins), numOutputs(outs)
{
    runOnlyWhenNeeded = AppProperties::get()->getBoolValue(kRunOnlyWhenNeededKey, true);

    deviceManager.addChangeListener(this);

    monitor.onConsumerPresenceChanged = [this](bool) { update(); };
    monitor.setTargetDeviceName(intendedOutputName());

    update();
}

AudioPowerManager::~AudioPowerManager()
{
    stopTimer();
    monitor.onConsumerPresenceChanged = nullptr;
    deviceManager.removeChangeListener(this);
}

void AudioPowerManager::setEditorVisible(bool visible)
{
    if (editorVisible == visible) return;

    editorVisible = visible;
    update();
}

void AudioPowerManager::setRunOnlyWhenNeeded(bool enabled)
{
    if (runOnlyWhenNeeded == enabled) return;

    runOnlyWhenNeeded = enabled;
    AppProperties::get()->setValue(kRunOnlyWhenNeededKey, enabled);
    update();
}

bool AudioPowerManager::isNeeded() const
{
    return !runOnlyWhenNeeded || editorVisible || monitor.hasConsumer();
}

// The output the user chose (persisted setup), not whatever JUCE fell back to
// if that device went missing — otherwise we'd lose track of the loopback.
String AudioPowerManager::intendedOutputName() const
{
    if (auto xml = deviceManager.createStateXml())
    {
        auto name = xml->getStringAttribute("audioDeviceName");
        if (name.isEmpty()) {
            name = xml->getStringAttribute("audioOutputDeviceName");
        }
        if (name.isNotEmpty()) {
            return name;
        }
    }

    return sleeping ? sleptSetup.outputDeviceName
                    : deviceManager.getAudioDeviceSetup().outputDeviceName;
}

void AudioPowerManager::update()
{
    if (isNeeded()) {
        if (sleeping) {
            stopTimer();
            wake(); // re-arms the timer as a retry on failure
        } else if (isTimerRunning()) {
            os_log(logger(), "needed again, cancelling idle shutdown");
            stopTimer();
        }
    } else {
        if (sleeping) {
            stopTimer(); // cancel pending wake retries
        } else if (! isTimerRunning()) {
            os_log(logger(), "no consumer, stopping audio in %d s", kGracePeriodMs / 1000);
            startTimer(kGracePeriodMs);
        }
    }
}

void AudioPowerManager::timerCallback()
{
    stopTimer();

    if (sleeping) {
        if (isNeeded()) {
            wake();
        }
    } else if (!isNeeded()) {
        sleep();
    }
}

void AudioPowerManager::sleep()
{
    sleptSetup = deviceManager.getAudioDeviceSetup();
    sleeping = true;

    deviceManager.closeAudioDevice();
    os_log(logger(), "audio stopped");
}

void AudioPowerManager::wake()
{
    sleeping = false;

    String error;

    // Prefer the user's explicit choice; no silent fallback to the system default device.
    if (auto xml = deviceManager.createStateXml()) {
        error = deviceManager.initialise(numInputs, numOutputs, xml.get(), false);
    } else {
        error = deviceManager.setAudioDeviceSetup(sleptSetup, false);
    }

    if (error.isNotEmpty() || deviceManager.getCurrentAudioDevice() == nullptr) {
        os_log_error(logger(), "failed to start audio device: %{public}s; retrying in %d s",
                     error.toRawUTF8(), kWakeRetryMs / 1000);
        sleeping = true;
        startTimer(kWakeRetryMs);
        return;
    }

    os_log(logger(), "audio started");
    monitor.setTargetDeviceName(intendedOutputName());
}

void AudioPowerManager::changeListenerCallback(ChangeBroadcaster*)
{
    // Persist the user's explicit device choice (set via the device selector).
    if (auto xml = deviceManager.createStateXml()) {
        AppProperties::get()->setValue(kAudioDeviceStateKey, xml.get());
    }

    // Defensive: if something reopened the device behind our back, track reality.
    // Only then call update(): failed wakes also broadcast changes, and reacting to
    // those would turn the retry timer into a tight loop.
    const bool reopened = sleeping && deviceManager.getCurrentAudioDevice() != nullptr;
    if (reopened) {
        os_log(logger(), "audio device reopened externally");
        sleeping = false;
    }

    monitor.setTargetDeviceName(intendedOutputName());

    if (reopened) {
        update();
    }
}
