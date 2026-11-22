#include <wups.h>
#include <mic/mic.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <nsysnet/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <malloc.h>

WUPS_PLUGIN_NAME("GamePad Mic Streamer");
WUPS_PLUGIN_DESCRIPTION("Streams GamePad mic PCM audio to a local PC over UDP");
WUPS_PLUGIN_VERSION("v1.0");
WUPS_PLUGIN_AUTHOR("Custom");
WUPS_PLUGIN_LICENSE("MIT");

// Change this to your PC's local IPv4 address
static const char* SERVER_IP = "192.168.1.100";
static const uint16_t SERVER_PORT = 5005;

static OSThread sMicThread;
static uint8_t* sThreadStack = nullptr;
static volatile bool sRunning = false;
static int sSock = -1;

static int MicStreamThreadEntry(int argc, const char** argv) {
    (void)argc;
    (void)argv;

    MICError err;
    MICHandle handle = MICOpen(MIC_INSTANCE_0, &err);
    if (handle < 0) {
        return 0;
    }

    // 32 kHz mono 16-bit PCM; 1024 samples = 2048 bytes (~32 ms chunks)
    const uint32_t sampleCount = 1024;
    const uint32_t bufferBytes = sampleCount * sizeof(int16_t);
    int16_t* pcmBuffer = static_cast<int16_t*>(memalign(0x40, bufferBytes));
    if (!pcmBuffer) {
        MICClose(handle);
        return 0;
    }

    MICInitData initData = {};
    initData.workMemory = memalign(0x40, 0x10000);
    initData.workMemorySize = 0x10000;
    initData.sampleRate = MIC_SAMPLE_RATE_32000;
    initData.bufferSize = bufferBytes;

    if (MICInit(handle, &initData) == MIC_ERROR_NONE && MICStart(handle) == MIC_ERROR_NONE) {
        struct sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

        while (sRunning) {
            MICStatus status = {};
            int32_t readSamples = MICRead(handle, pcmBuffer, sampleCount, &status);
            if (readSamples > 0 && sSock >= 0) {
                sendto(sSock, pcmBuffer, static_cast<size_t>(readSamples) * sizeof(int16_t),
                       MSG_DONTWAIT, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr));
            }
            OSSleepTicks(OSMillisecondsToTicks(10));
        }
        MICStop(handle);
    }

    MICClose(handle);
    free(pcmBuffer);
    free(initData.workMemory);
    return 0;
}

INITIALIZE_PLUGIN() {
    socket_lib_init();
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
        socketclose(sSock);
        sSock = -1;
    }
    free(sThreadStack);
    sThreadStack = nullptr;
}
