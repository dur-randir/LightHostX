//
//  ConsumerMonitor.hpp
//  LightHostX
//
//  Watches CoreAudio process objects (macOS 14+) to tell whether any *other*
//  process is currently reading (running input) from our output device —
//  typically a loopback device such as BlackHole.
//
//  Note: CoreAudio headers are deliberately not included here; they clash with
//  juce::AudioBuffer under `using namespace juce`.
//

#ifndef ConsumerMonitor_hpp
#define ConsumerMonitor_hpp

#include "JuceHeader.h"
#include <cstdint>
#include <set>
#include <vector>

class ConsumerMonitor : private AsyncUpdater
{
public:
    ConsumerMonitor();
    ~ConsumerMonitor() override;

    // Name as reported by JUCE's CoreAudio device type (AudioDeviceSetup::outputDeviceName).
    // Re-evaluates synchronously; fires onConsumerPresenceChanged if the result changed.
    void setTargetDeviceName(const String& outputDeviceName);

    bool hasConsumer() const { return consumerPresent; }

    // Called on the message thread whenever consumer presence flips.
    std::function<void(bool)> onConsumerPresenceChanged;

private:
    using ObjectID = std::uint32_t; // == AudioObjectID

    struct CoreAudioListener; // C listener trampoline, defined in .cpp

    String targetName;
    bool consumerPresent = false;
    std::set<ObjectID> watchedProcesses;

    void handleAsyncUpdate() override;
    void reevaluate();
    void syncProcessListeners(const std::vector<ObjectID>& processes);
    bool evaluate(const std::vector<ObjectID>& processes) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConsumerMonitor)
};

#endif /* ConsumerMonitor_hpp */
