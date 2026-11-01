#include <utilist/protocol/messages.hpp>
#include <utilist/protocol/framing.hpp>
#include <utilist/protocol/json_helper.hpp>
#include <utilist/models/utility_descriptor.hpp>
#include <cassert>
#include <iostream>

void test_framing() {
    using namespace utilist::protocol;
    std::cout << "[TEST] Running test_framing..." << std::endl;

    std::string sample = "{\"test\":123}";
    auto framed = FrameBuffer::frameMessage(sample);
    assert(framed.size() == sizeof(uint32_t) + sample.size());

    FrameBuffer buffer;
    // Chunked append
    buffer.appendData(framed.data(), 5);
    assert(!buffer.extractNextMessage().has_value());

    buffer.appendData(framed.data() + 5, framed.size() - 5);
    auto extracted = buffer.extractNextMessage();
    assert(extracted.has_value());
    assert(*extracted == sample);
    assert(!buffer.extractNextMessage().has_value());

    std::cout << "[PASS] test_framing passed." << std::endl;
}

void test_json_protocol() {
    using namespace utilist::protocol;
    std::cout << "[TEST] Running test_json_protocol..." << std::endl;

    Request req;
    req.requestId = "req-42";
    req.type = "command";
    req.command = "activate";
    req.payloadJson = "{\"intensity\":5}";

    std::string serialized = JsonHelper::serializeRequest(req);
    auto parsedOpt = JsonHelper::parseRequest(serialized);
    assert(parsedOpt.has_value());
    assert(parsedOpt->requestId == "req-42");
    assert(parsedOpt->type == "command");
    assert(parsedOpt->command == "activate");

    Response res;
    res.requestId = "req-42";
    res.type = "response";
    res.success = false;
    res.error = ErrorInfo{"ERR_BUSY", "Utility is busy"};
    
    std::string resSerialized = JsonHelper::serializeResponse(res);
    auto resParsedOpt = JsonHelper::parseResponse(resSerialized);
    assert(resParsedOpt.has_value());
    assert(resParsedOpt->requestId == "req-42");
    assert(!resParsedOpt->success);
    assert(resParsedOpt->error.has_value());
    assert(resParsedOpt->error->code == "ERR_BUSY");

    std::cout << "[PASS] test_json_protocol passed." << std::endl;
}

void test_manifest_validation() {
    using namespace utilist::models;
    std::cout << "[TEST] Running test_manifest_validation..." << std::endl;

    std::string validJson = R"({
        "schema_version": 1,
        "id": "example.heartbeat",
        "name": "Heartbeat Utility",
        "description": "Test utility",
        "version": "1.0.0",
        "executable": "HeartbeatUtility.exe",
        "protocol_version": 1,
        "capabilities": ["status", "activate", "deactivate"]
    })";

    auto desc = UtilityDescriptor::fromJson(validJson, "C:/Utils/Heartbeat");
    assert(desc.has_value());
    assert(desc->id == "example.heartbeat");
    assert(desc->hasCapability("status"));
    assert(!desc->hasCapability("pause"));

    auto errors = UtilityDescriptor::validate(*desc);
    assert(errors.empty());

    // Test invalid manifest (path traversal)
    std::string invalidJson = R"({
        "schema_version": 1,
        "id": "invalid_id_format",
        "name": "Test",
        "description": "Test",
        "version": "1.0.0",
        "executable": "../escape.exe",
        "protocol_version": 1,
        "capabilities": ["unknown_capability"]
    })";

    auto invalidDesc = UtilityDescriptor::fromJson(invalidJson);
    assert(invalidDesc.has_value());
    auto validationErrors = UtilityDescriptor::validate(*invalidDesc);
    assert(!validationErrors.empty());

    std::cout << "[PASS] test_manifest_validation passed." << std::endl;
}

int main() {
    std::cout << "Starting UTILIST shared unit tests..." << std::endl;
    test_framing();
    test_json_protocol();
    test_manifest_validation();
    std::cout << "All UTILIST shared unit tests PASSED successfully." << std::endl;
    return 0;
}
