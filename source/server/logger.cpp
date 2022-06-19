#include "logger.hpp"
#include "helpers.hpp"
#include "nn/result.h"
#include "nn/util.h"
#include "nx/svc.h"

Logger* Logger::sInstance = nullptr;

void Logger::createInstance() {
#ifdef SERVERIP
    sInstance = new Logger(TOSTRING(SERVERIP), 3080, "MainLogger");
#else
    sInstance = new Logger(0, 3080, "MainLogger");
#endif
}

nn::Result Logger::init(const char* ip, u16 port) {
    sock_ip = ip;

    this->port = port;

    in_addr hostAddress = {0};
    sockaddr serverAddress = {0};

    if (this->socket_log_state != SOCKET_LOG_UNINITIALIZED)
        return -1;

    nn::nifm::Initialize();
    nn::nifm::SubmitNetworkRequest();

    while (nn::nifm::IsNetworkRequestOnHold()) {
    }

// emulators make this return false always, so skip it during init
#ifndef EMU

    if (!nn::nifm::IsNetworkAvailable()) {
        this->socket_log_state = SOCKET_LOG_UNAVAILABLE;
        return -1;
    }

#endif

    if ((this->socket_log_socket = nn::socket::Socket(2, 1, 0)) < 0) {
        this->socket_log_state = SOCKET_LOG_UNAVAILABLE;
        return -1;
    }

    nn::socket::InetAton(this->sock_ip, &hostAddress);

    serverAddress.address = hostAddress;
    serverAddress.port = nn::socket::InetHtons(this->port);
    serverAddress.family = 2;

    nn::Result result;

    if ((result =
             nn::socket::Connect(this->socket_log_socket, &serverAddress, sizeof(serverAddress)))
            .isFailure()) {
        this->socket_log_state = SOCKET_LOG_UNAVAILABLE;
        return result;
    }

    this->socket_log_state = SOCKET_LOG_CONNECTED;

    this->isDisableName = false;

    return 0;
}

void Logger::log(const char* fmt, va_list args) {  // impl for replacing seads system::print
    if (!sInstance)
        return;
    char buf[0x500];
    if (nn::util::VSNPrintf(buf, sizeof(buf), fmt, args) > 0) {
        sInstance->socket_log(buf);
    }
}

s32 Logger::read(char* out) {
    return this->socket_read_char(out);
}

void Logger::log(const char* fmt, ...) {
    if (!sInstance)
        return;
    va_list args;
    va_start(args, fmt);

    size_t bufSize = nn::util::VSNPrintf(nullptr, 0, fmt, args);
    size_t prefixSize = bufSize += 3 /* "[] " */ + strlen(sInstance->sockName);
    if (!sInstance->isDisableName)
        bufSize += prefixSize;
    char buf[bufSize];

    if (sInstance->isDisableName)
        nn::util::VSNPrintf(buf, bufSize, fmt, args);
    else {
        nn::util::VSNPrintf(buf + prefixSize, bufSize - prefixSize, fmt, args);
        nn::util::SNPrintf(buf, prefixSize, "[%s] ", sInstance->sockName);
    }

    sInstance->socket_log(buf);
#ifdef EMU
    svcOutputDebugString(buf, bufSize);
#endif

    va_end(args);
}

bool Logger::pingSocket() {
    return socket_log("ping") > 0;  // if value is greater than zero, than the socket recieved our
                                    // message, otherwise the connection was lost.
}

void tryInitSocket() {
    __asm("STR X20, [X8,#0x18]");
#ifdef DEBUGLOG
    Logger::createInstance();  // creates a static instance for debug logger
#endif
}
