#include <wups.h>
#include <mic/mic.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/cache.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <malloc.h>

WUPS_PLUGIN_NAME("GamePad Mic Streamer");
WUPS_PLUGIN_DESCRIPTION("Streams GamePad mic PCM audio to a local PC over UDP");
WUPS_PLUGIN_VERSION("v1.0");
WUPS_PLUGIN_AUTHOR("Custom");
WUPS_PLUGIN_LICENSE("MIT");

// Change this to your PC's local IPv4 address
static const char* SERVER_IP = "192.168.1.167";
static const uint16_t SERVER_PORT = 5005;

static OSThread sMicThread;
static uint8_t* sThreadStack = nullptr;
static volatile bool sRunning = false;
static int sSock = -1;

static int MicStreamThreadEntry(int argc, const char** argv) {
    (void)argc;
    (void)argv;

    // Cafe OS requires sampleMaxCount >= 0x2800 and a 0x40-aligned buffer of sampleMaxCount * 2 bytes
    const size_t maxSamples = 0x2800;
    const size_t ringBufferBytes = maxSamples * sizeof(int16_t);
    int16_t* sampleBuffer = static_cast<int16_t*>(memalign(0x40, ringBufferBytes));
    if (!sampleBuffer) {
        return 0;
    }
    std::memset(sampleBuffer, 0, ringBufferBytes);
    DCFlushRange(sampleBuffer, ringBufferBytes);

    MICWorkMemory workMem = {};
    workMem.sampleMaxCount = maxSamples;
    workMem.sampleBuffer = sampleBuffer;

    MICError err = MIC_ERROR_OK;
    MICHandle handle = MICInit(MIC_INSTANCE_0, 0, &workMem, &err);
    if (handle < 0 || err != MIC_ERROR_OK) {
        free(sampleBuffer);
        return 0;
    }

    if (MICOpen(handle) == MIC_ERROR_OK) {
        struct sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

        // Send up to 512 samples (1024 bytes) per UDP packet
        const size_t chunkSamples = 512;
        int16_t sendBuf[chunkSamples];

        while (sRunning) {
            MICStatus status = {};
            if (MICGetStatus(handle, &status) == MIC_ERROR_OK && status.availableData > 0) {
                DCInvalidateRange(sampleBuffer, ringBufferBytes);

                size_t remaining = static_cast<size_t>(status.availableData);
                size_t pos = static_cast<size_t>(status.bufferPos);

                while (remaining > 0 && sRunning) {
                    size_t toSend = (remaining > chunkSamples) ? chunkSamples : remaining;

                    for (size_t i = 0; i < toSend; ++i) {
                        sendBuf[i] = sampleBuffer[(pos + i) % maxSamples];
                    }

                    if (sSock >= 0) {
                        sendto(sSock, sendBuf, toSend * sizeof(int16_t), MSG_DONTWAIT,
                               reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr));
                    }

                    MICSetDataConsumed(handle, static_cast<int>(toSend));
                    pos = (pos + toSend) % maxSamples;
                    remaining -= toSend;
                }
            }
            OSSleepTicks(OSMillisecondsToTicks(10));
        }
        MICClose(handle);
    }

    MICUninit(handle);
    free(sampleBuffer);
    return 0;
}

INITIALIZE_PLUGIN() {
    sSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    const size_t stackSize = 0x8000;
    sThreadStack = static_cast<uint8_t*>(memalign(0x10, stackSize));
    if (sThreadStack && OSCreateThread(&sMicThread, MicStreamThreadEntry, 0, nullptr,
                                       sThreadStack + stackSize, stackSize, 16, OS_THREAD_ATTRIB_AFFINITY_ANY)) {
        sRunning = true;
        OSResumeThread(&sMicThread);
    }
}

DEINITIALIZE_PLUGIN() {
    if (sRunning) {
        sRunning = false;
        OSJoinThread(&sMicThread, nullptr);
    }
    if (sSock >= 0) {
        close(sSock);
        sSock = -1;
    }
    free(sThreadStack);
    sThreadStack = nullptr;
}
