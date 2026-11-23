//
//  ConsumerMonitor.cpp
//  LightHostX
//

// CoreAudio must come before JuceHeader (see ConsumerMonitor.hpp)
#include <CoreAudio/CoreAudio.h>
#include "ConsumerMonitor.hpp"
#include <os/log.h>
#include <optional>
#include <cstring>
#include <unistd.h>
#include <algorithm>

namespace
{
    os_log_t logger()
    {
        static os_log_t l = os_log_create("com.randir.LightHostX", "ConsumerMonitor");
        return l;
    }

    constexpr AudioObjectPropertyAddress kProcessListAddr {
        kAudioHardwarePropertyProcessObjectList, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    constexpr AudioObjectPropertyAddress kDeviceListAddr {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    constexpr AudioObjectPropertyAddress kProcRunningInputAddr {
        kAudioProcessPropertyIsRunningInput, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    // Read with input scope; listen with wildcard scope so a change notified under any scope is caught.
    constexpr AudioObjectPropertyAddress kProcInputDevicesAddr {
        kAudioProcessPropertyDevices, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain };
    constexpr AudioObjectPropertyAddress kProcDevicesListenAddr {
        kAudioProcessPropertyDevices, kAudioObjectPropertyScopeWildcard, kAudioObjectPropertyElementMain };

    std::vector<char> getRaw(AudioObjectID obj, const AudioObjectPropertyAddress& addr)
    {
        UInt32 size = 0;
        if (AudioObjectGetPropertyDataSize(obj, &addr, 0, nullptr, &size) != noErr || size == 0) {
            return {};
        }

        std::vector<char> data(size);
        if (AudioObjectGetPropertyData(obj, &addr, 0, nullptr, &size, data.data()) != noErr) {
            return {};
        }

        data.resize(size);
        return data;
    }

    template <typename T>
    std::vector<T> getArray(AudioObjectID obj, const AudioObjectPropertyAddress& addr)
    {
        auto raw = getRaw(obj, addr);
        std::vector<T> result(raw.size() / sizeof(T));
        std::memcpy(result.data(), raw.data(), result.size() * sizeof(T));
        return result;
    }

    template <typename T>
    std::optional<T> getScalar(AudioObjectID obj, const AudioObjectPropertyAddress& addr)
    {
        T value {};
        UInt32 size = sizeof(T);
        if (AudioObjectGetPropertyData(obj, &addr, 0, nullptr, &size, &value) != noErr || size != sizeof(T)) {
            return std::nullopt;
        }
        return value;
    }

    String getCFStringProperty(AudioObjectID obj, AudioObjectPropertySelector selector)
    {
        const AudioObjectPropertyAddress addr { selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        if (auto str = getScalar<CFStringRef>(obj, addr); str && *str != nullptr) {
            String result = String::fromCFString(*str);
            CFRelease(*str);
            return result;
        }
        return {};
    }

    int getNumOutputChannels(AudioObjectID device)
    {
        const AudioObjectPropertyAddress addr {
            kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain };

        auto raw = getRaw(device, addr);
        if (raw.size() < offsetof(AudioBufferList, mBuffers)) {
            return 0;
        }

        auto* list = reinterpret_cast<const AudioBufferList*>(raw.data());
        int total = 0;
        for (UInt32 i = 0; i < list->mNumberBuffers; ++i) {
            total += (int) list->mBuffers[i].mNumberChannels;
        }
        return total;
    }

    // Mirrors CoreAudioIODeviceType::scanForDevices() so names match exactly,
    // including JUCE's de-duplication suffixes.
    AudioObjectID resolveOutputDevice(const String& name)
    {
        if (name.isEmpty()) {
            return kAudioObjectUnknown;
        }

        StringArray names;
        std::vector<AudioObjectID> ids;

        for (auto dev : getArray<AudioObjectID>(kAudioObjectSystemObject, kDeviceListAddr)) {
            auto devName = getCFStringProperty(dev, kAudioDevicePropertyDeviceNameCFString);
            if (devName.isEmpty() || getNumOutputChannels(dev) <= 0) {
                continue;
            }

            names.add(devName);
            ids.push_back(dev);
        }

        names.appendNumbersToDuplicates(false, true);

        auto idx = names.indexOf(name);
        return idx >= 0 ? ids[(size_t) idx] : kAudioObjectUnknown;
    }

    // The device itself plus, for aggregate / multi-output devices, its active sub-devices.
    // Needed in both directions:
    //  - our output may be an aggregate (e.g. headphones + BlackHole) while the consumer reads BlackHole;
    //  - a consumer's input device may be an aggregate containing our output. (In practice the HAL
    //    reports physical devices in kAudioProcessPropertyDevices even for private aggregates, so this
    //    is mostly defensive.)
    std::set<AudioObjectID> expandDevice(AudioObjectID device)
    {
        std::set<AudioObjectID> result { device };

        const AudioObjectPropertyAddress subAddr {
            kAudioAggregateDevicePropertyActiveSubDeviceList, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };

        if (AudioObjectHasProperty(device, &subAddr)) {
            for (auto sub : getArray<AudioObjectID>(device, subAddr)) {
                result.insert(sub);
            }
        }

        return result;
    }

    bool intersects(const std::set<AudioObjectID>& a, const std::set<AudioObjectID>& b)
    {
        return std::any_of(a.begin(), a.end(), [&b](AudioObjectID id) { return b.count(id) != 0; });
    }
}

static_assert(std::is_same_v<AudioObjectID, std::uint32_t>);

//==============================================================================
struct ConsumerMonitor::CoreAudioListener
{
    // Called on a CoreAudio notification thread; coalesce and hop to the message thread.
    static OSStatus proc(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* clientData)
    {
        static_cast<ConsumerMonitor*>(clientData)->triggerAsyncUpdate();
        return noErr;
    }
};

//==============================================================================
ConsumerMonitor::ConsumerMonitor()
{
    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &kProcessListAddr, CoreAudioListener::proc, this);
    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &kDeviceListAddr,  CoreAudioListener::proc, this);
}

ConsumerMonitor::~ConsumerMonitor()
{
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &kProcessListAddr, CoreAudioListener::proc, this);
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &kDeviceListAddr,  CoreAudioListener::proc, this);

    syncProcessListeners({});
    cancelPendingUpdate();
}

void ConsumerMonitor::setTargetDeviceName(const String& outputDeviceName)
{
    if (targetName != outputDeviceName) {
        os_log(logger(), "target device: '%{public}s'", outputDeviceName.toRawUTF8());
    }

    targetName = outputDeviceName;
    reevaluate();
}

void ConsumerMonitor::handleAsyncUpdate()
{
    reevaluate();
}

void ConsumerMonitor::reevaluate()
{
    auto processes = getArray<AudioObjectID>(kAudioObjectSystemObject, kProcessListAddr);
    syncProcessListeners(processes);

    const bool now = evaluate(processes);
    if (now == consumerPresent) {
        return;
    }

    consumerPresent = now;
    os_log(logger(), "consumer present: %{public}s", now ? "yes" : "no");

    if (onConsumerPresenceChanged) {
        onConsumerPresenceChanged(now);
    }
}

void ConsumerMonitor::syncProcessListeners(const std::vector<AudioObjectID>& processes)
{
    std::set<AudioObjectID> current(processes.begin(), processes.end());

    for (auto it = watchedProcesses.begin(); it != watchedProcesses.end(); ) {
        if (current.count(*it) == 0) {
            // Object may already be gone; errors are expected and harmless.
            AudioObjectRemovePropertyListener(*it, &kProcRunningInputAddr, CoreAudioListener::proc, this);
            AudioObjectRemovePropertyListener(*it, &kProcDevicesListenAddr, CoreAudioListener::proc, this);
            it = watchedProcesses.erase(it);
        } else {
            ++it;
        }
    }

    for (auto process : current) {
        if (watchedProcesses.count(process) != 0) {
            continue;
        }

        AudioObjectAddPropertyListener(process, &kProcRunningInputAddr, CoreAudioListener::proc, this);
        AudioObjectAddPropertyListener(process, &kProcDevicesListenAddr, CoreAudioListener::proc, this);
        watchedProcesses.insert(process);
    }
}

bool ConsumerMonitor::evaluate(const std::vector<AudioObjectID>& processes) const
{
    const auto target = resolveOutputDevice(targetName);
    if (target == kAudioObjectUnknown) {
        return false;
    }

    const auto targetDevices = expandDevice(target);

    const pid_t selfPid = getpid();
    const AudioObjectPropertyAddress pidAddr {
        kAudioProcessPropertyPID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };

    for (auto proc : processes) {
        auto pid = getScalar<pid_t>(proc, pidAddr);
        if (!pid || *pid == selfPid)
            continue;

        auto running = getScalar<UInt32>(proc, kProcRunningInputAddr);
        if (!running || *running == 0) {
            continue;
        }

        for (auto dev : getArray<AudioObjectID>(proc, kProcInputDevicesAddr)) {
            if (intersects(expandDevice(dev), targetDevices)) {
                if (!consumerPresent) {
                    os_log(logger(), "consumer: pid %d (%{public}s)", (int) *pid,
                           getCFStringProperty(proc, kAudioProcessPropertyBundleID).toRawUTF8());
                }
                return true;
            }
        }
    }

    return false;
}
