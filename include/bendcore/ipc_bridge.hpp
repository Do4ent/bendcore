// IPC Bridge: React UI ↔ C++ bendcore Backend Communication
// Phase 26 Stage B2: IPC Integration Layer
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <variant>

namespace bendcore::ipc {

// ========== Message Types ==========

enum class MessageType {
    // Reporting system
    ReleaseForProduction,
    GetProductionReports,
    ExportReport,
    DeleteReport,

    // Performance monitoring
    GetPerformanceMetrics,
    ResetMetrics,

    // Standards checking
    GetStandardsForTube,
    GetCurrentStandard,
    SetCurrentStandard,
    GetStandardDetails,
    CheckCompliance,

    // Collaboration (Phase 26C)
    GetComments,
    GetVersions,
    GetChanges,

    // System
    Ping,
    Error,
    Response
};

// ========== JSON-like Message Structure ==========

class JsonValue {
public:
    enum class Type {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object
    };

    JsonValue();
    ~JsonValue();

    // Constructors
    explicit JsonValue(bool value);
    explicit JsonValue(double value);
    explicit JsonValue(int value);
    explicit JsonValue(const std::string& value);
    explicit JsonValue(const char* value);

    // Type checking
    Type type() const;
    bool isNull() const;
    bool isBoolean() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    // Value access
    bool asBoolean() const;
    double asNumber() const;
    std::string asString() const;

    // Array operations
    void pushArray(const JsonValue& value);
    size_t arraySize() const;
    JsonValue& getArray(size_t index);
    const JsonValue& getArray(size_t index) const;

    // Object operations
    void setObject(const std::string& key, const JsonValue& value);
    bool hasObject(const std::string& key) const;
    JsonValue& getObject(const std::string& key);
    const JsonValue& getObject(const std::string& key) const;

    // Serialization
    std::string serialize() const;
    static JsonValue deserialize(const std::string& json);

private:
    Type type_;
    std::variant<
        std::nullptr_t,
        bool,
        double,
        std::string,
        std::vector<JsonValue>,
        std::map<std::string, JsonValue>
    > data;
};

// ========== IPC Message ==========

struct IpcMessage {
    MessageType type;
    JsonValue params;
    std::string requestId;
    int64_t timestamp;

    IpcMessage();
    explicit IpcMessage(MessageType type);

    std::string serialize() const;
    static IpcMessage deserialize(const std::string& json);
};

struct IpcResponse {
    std::string requestId;
    bool success;
    JsonValue result;
    std::string errorMessage;
    int64_t timestamp;

    IpcResponse();

    std::string serialize() const;
    static IpcResponse deserialize(const std::string& json);
};

// ========== IPC Handler Interface ==========

using ResponseCallback = std::function<void(const IpcResponse&)>;
using ErrorCallback = std::function<void(const std::string&)>;

class IpcHandler {
public:
    virtual ~IpcHandler() = default;

    // Handle incoming message
    virtual void handleMessage(const IpcMessage& message, ResponseCallback callback) = 0;

    // Send message to remote
    virtual void sendMessage(const IpcMessage& message, ResponseCallback callback) = 0;

    // Connection management
    virtual bool connect(const std::string& endpoint) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Error handling
    virtual void setErrorCallback(ErrorCallback callback) = 0;
};

// ========== Reporting System IPC Handler ==========

class ReportingIpcHandler {
public:
    explicit ReportingIpcHandler(IpcHandler* handler);
    ~ReportingIpcHandler();

    // Async methods that return via callback
    void releaseForProduction(
        const std::string& tubeId,
        const std::string& approvedBy,
        const std::string& comments,
        ResponseCallback callback
    );

    void getProductionReports(
        const std::string& filter,
        ResponseCallback callback
    );

    void exportReport(
        const std::string& reportId,
        const std::vector<std::string>& formats,
        const std::string& outputPath,
        ResponseCallback callback
    );

    void deleteReport(
        const std::string& reportId,
        ResponseCallback callback
    );

private:
    IpcHandler* handler;
    std::string generateRequestId();
};

// ========== Performance Monitoring IPC Handler ==========

class PerformanceIpcHandler {
public:
    explicit PerformanceIpcHandler(IpcHandler* handler);
    ~PerformanceIpcHandler();

    void getPerformanceMetrics(ResponseCallback callback);
    void resetMetrics(ResponseCallback callback);

    void recordAnalysisTime(double milliseconds);
    void recordCacheHit();
    void recordCacheMiss();
    void recordMemoryUsage(double mb);

private:
    IpcHandler* handler;
    std::string generateRequestId();
};

// ========== Standards Checking IPC Handler ==========

class StandardsIpcHandler {
public:
    explicit StandardsIpcHandler(IpcHandler* handler);
    ~StandardsIpcHandler();

    void getStandardsForTube(
        const std::string& tubeId,
        double od,
        double wall,
        ResponseCallback callback
    );

    void getCurrentStandard(ResponseCallback callback);

    void setCurrentStandard(
        const std::string& standardName,
        ResponseCallback callback
    );

    void getStandardDetails(
        const std::string& standardName,
        ResponseCallback callback
    );

    void checkCompliance(
        const std::string& tubeId,
        double od,
        double wall,
        const std::string& standard,
        ResponseCallback callback
    );

private:
    IpcHandler* handler;
    std::string generateRequestId();
};

// ========== Collaboration IPC Handler (Phase 26C) ==========

class CollaborationIpcHandler {
public:
    explicit CollaborationIpcHandler(IpcHandler* handler);
    ~CollaborationIpcHandler();

    void getComments(const std::string& tubeId, ResponseCallback callback);
    void getVersions(const std::string& tubeId, ResponseCallback callback);
    void getChanges(const std::string& tubeId, ResponseCallback callback);

private:
    IpcHandler* handler;
    std::string generateRequestId();
};

// ========== IPC Bridge Manager ==========

class IpcBridge {
public:
    explicit IpcBridge();
    ~IpcBridge();

    // Initialize bridge
    bool initialize(const std::string& endpoint = "");

    // Access handlers
    ReportingIpcHandler* reporting();
    PerformanceIpcHandler* performance();
    StandardsIpcHandler* standards();
    CollaborationIpcHandler* collaboration();

    // Connection
    bool isConnected() const;
    void disconnect();

    // Error handling
    void setErrorCallback(ErrorCallback callback);

    // Ping for health check
    void ping(ResponseCallback callback);

private:
    std::unique_ptr<IpcHandler> handler;
    std::unique_ptr<ReportingIpcHandler> reportingHandler;
    std::unique_ptr<PerformanceIpcHandler> performanceHandler;
    std::unique_ptr<StandardsIpcHandler> standardsHandler;
    std::unique_ptr<CollaborationIpcHandler> collaborationHandler;
};

}  // namespace bendcore::ipc
