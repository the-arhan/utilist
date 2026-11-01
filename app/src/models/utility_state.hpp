#pragma once

#include <utilist/models/utility_descriptor.hpp>
#include <string>
#include <cstdint>

namespace utilist::app {

enum class ProcessState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Error
};

inline const char* processStateToString(ProcessState state) {
    switch (state) {
        case ProcessState::Stopped: return "Stopped";
        case ProcessState::Starting: return "Starting";
        case ProcessState::Running: return "Running";
        case ProcessState::Stopping: return "Stopping";
        case ProcessState::Error: return "Error";
    }
    return "Unknown";
}

struct ManagedUtility {
    models::UtilityDescriptor descriptor;
    bool enabled{true};
    ProcessState processState{ProcessState::Stopped};
    bool featureActive{false};
    std::string stateDescription{"Stopped"};
    std::string configuredShortcut;
    std::string lastError;
    uint32_t processId{0};
    uint64_t uptimeSeconds{0};
};

} // namespace utilist::app
