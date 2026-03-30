#include "StdInc.h"
#include <ScreenCapture.h>
#include <curl/curl.h>
#include <json.hpp>
#include "../launcher/LauncherConfig.h"

#include <gdiplus.h>
#include <objidl.h>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>

using json = nlohmann::json;

// ===================== Options & Globals =====================
struct CaptureOptions {
	enum class Mode { Primary, Virtual, Window, CropRect } mode = Mode::Virtual;
	HWND   hwnd{ nullptr };          // khi Mode::Window
	RECT   crop{ 0,0,0,0 };          // khi Mode::CropRect
	double scale{ 0.5 };             // mặc định scale 0.5
	ULONG  quality{ 65 };            // JPEG quality 1..100
	bool   addWatermark{ true };
	std::wstring watermarkText{ L"LR Anti-Cheat" };
	bool   addTimestamp{ true };
	std::vector<RECT> redactRects; // toạ độ theo ảnh gốc trước khi scale
};

// mặc định: Primary + scale 0.5 + quality 65
static CaptureOptions g_capOpt{};

// Cho phép nơi khác tuỳ chỉnh mà không đổi public API
void SetScreenshotOptions(const CaptureOptions& opt) { g_capOpt = opt; }

// ===================== RAII Helpers =====================
struct HDC_Free { void operator()(HDC h) const noexcept { if (h) DeleteDC(h); } };
struct HBMP_Free { void operator()(HBITMAP h) const noexcept { if (h) DeleteObject(h); } };
struct SelectGuard { HDC dc{}; HGDIOBJ old{}; ~SelectGuard() { if (dc && old) SelectObject(dc, old); } };

static std::once_flag g_onceCurl, g_onceGdip, g_onceJpeg;
static CLSID g_jpegClsid{};
static ULONG_PTR g_gdipToken{};

// ===================== Init Routines =====================
static void InitCurlOnce() { curl_global_init(CURL_GLOBAL_DEFAULT); }
static void InitGdiplusOnce() {
	Gdiplus::GdiplusStartupInput in;
	Gdiplus::GdiplusStartup(&g_gdipToken, &in, nullptr);
	// có thể GdiplusShutdown tại process exit nếu cần
}
static void InitJpegClsidOnce() {
	UINT n = 0, sz = 0;
	Gdiplus::GetImageEncodersSize(&n, &sz);
	if (!sz) return;
	std::unique_ptr<BYTE[]> buf(new BYTE[sz]);
	auto* enc = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.get());
	if (Gdiplus::GetImageEncoders(n, sz, enc) != Gdiplus::Ok) return;
	for (UINT i = 0; i < n; ++i) {
		if (wcscmp(enc[i].MimeType, L"image/jpeg") == 0) { g_jpegClsid = enc[i].Clsid; break; }
	}
}

// ===================== Capture Core =====================
static bool ResolveRect(const CaptureOptions& opt, RECT& rc) {
	if (opt.mode == CaptureOptions::Mode::Primary) {
		rc.left = 0; rc.top = 0;
		rc.right = GetSystemMetrics(SM_CXSCREEN);
		rc.bottom = GetSystemMetrics(SM_CYSCREEN);
		return (rc.right > 0 && rc.bottom > 0);
	}
	if (opt.mode == CaptureOptions::Mode::Virtual) {
		rc.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
		rc.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
		rc.right = rc.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
		rc.bottom = rc.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
		return (rc.right - rc.left > 0 && rc.bottom - rc.top > 0);
	}
	if (opt.mode == CaptureOptions::Mode::Window) {
		if (!opt.hwnd || !IsWindow(opt.hwnd)) return false;
		return GetWindowRect(opt.hwnd, &rc);
	}
	// CropRect
	rc = opt.crop;
	return (rc.right > rc.left && rc.bottom > rc.top);
}

static void Decorate(Gdiplus::Bitmap& bmp, const CaptureOptions& opt) {
	trace("decorating\n");
	using namespace Gdiplus;
	Graphics g(&bmp);
	g.SetCompositingMode(CompositingModeSourceOver);
	g.SetCompositingQuality(CompositingQualityHighQuality);
	g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
	g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
	g.SetSmoothingMode(SmoothingModeHighQuality);

	const UINT W = bmp.GetWidth(), H = bmp.GetHeight();

	// Redact (đen bán trong suốt)
	SolidBrush black(Color(200, 0, 0, 0));
	for (auto r : opt.redactRects) {
		Rect rr(
			LONG((r.left) * opt.scale),
			LONG((r.top) * opt.scale),
			LONG((r.right - r.left) * opt.scale),
			LONG((r.bottom - r.top) * opt.scale)
		);
		g.FillRectangle(&black, rr);
	}

	if (opt.addWatermark || opt.addTimestamp) {
		std::wstring text;
		if (opt.addWatermark) text += opt.watermarkText;
		if (opt.addTimestamp) {
			SYSTEMTIME st; GetLocalTime(&st);
			wchar_t buf[64];
			swprintf_s(buf, L"  %04u-%02u-%02u %02u:%02u:%02u",
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
			text += buf;
		}
		if (!text.empty()) {
			FontFamily ff(L"Segoe UI");
			Font font(&ff, 16, FontStyleBold, UnitPixel);
			RectF layout;
			g.MeasureString(text.c_str(), -1, &font, PointF(0, 0), &layout);
			const int pad = 8;
			const float x = std::max(0.f, float(W) - layout.Width - pad);
			const float y = std::max(0.f, float(H) - layout.Height - pad);
			SolidBrush bg(Color(160, 0, 0, 0));
			g.FillRectangle(&bg, RectF(x - 6, y - 2, layout.Width + 12, layout.Height + 4));
			SolidBrush shadow(Color(160, 0, 0, 0));
			SolidBrush white(Color(240, 255, 255, 255));
			g.DrawString(text.c_str(), -1, &font, PointF(x + 1, y + 1), &shadow);
			g.DrawString(text.c_str(), -1, &font, PointF(x, y), &white);
		}
	}
}

// Chụp theo g_capOpt → scale → JPEG (memory)
static bool CaptureScreenToJpegEx(std::vector<uint8_t>& out, const CaptureOptions& opt) {
	std::call_once(g_onceGdip, InitGdiplusOnce);
	std::call_once(g_onceJpeg, InitJpegClsidOnce);
	if (g_jpegClsid == CLSID{}) return false;

	RECT rc{}; if (!ResolveRect(opt, rc)) return false;
	const int srcW = rc.right - rc.left;
	const int srcH = rc.bottom - rc.top;
	if (srcW <= 0 || srcH <= 0) return false;

	std::unique_ptr<std::remove_pointer<HDC>::type, HDC_Free> memDC(CreateCompatibleDC(nullptr));
	if (!memDC) return false;

	HDC hScr = GetDC(nullptr);
	if (!hScr) return false;

	std::unique_ptr<std::remove_pointer<HBITMAP>::type, HBMP_Free>
		hbmp(CreateCompatibleBitmap(hScr, srcW, srcH));
	if (!hbmp) { ReleaseDC(nullptr, hScr); return false; }

	SelectGuard sel{ memDC.get(), SelectObject(memDC.get(), hbmp.get()) };

	bool okPaint = false;
	if (opt.mode == CaptureOptions::Mode::Window && opt.hwnd) {
		// Thử PrintWindow trước (có thể trả về false nếu app không cho)
		okPaint = PrintWindow(opt.hwnd, memDC.get(), PW_RENDERFULLCONTENT);
		if (!okPaint) {
			okPaint = BitBlt(memDC.get(), 0, 0, srcW, srcH, hScr, rc.left, rc.top, SRCCOPY);
		}
	}
	else {
		okPaint = BitBlt(memDC.get(), 0, 0, srcW, srcH, hScr, rc.left, rc.top, SRCCOPY);
	}
	ReleaseDC(nullptr, hScr);
	if (!okPaint) return false;

	Gdiplus::Bitmap srcBmp(hbmp.get(), nullptr);

	// Scale
	const int dstW = std::max(1, int(srcW * opt.scale));
	const int dstH = std::max(1, int(srcH * opt.scale));
	Gdiplus::Bitmap dstBmp(dstW, dstH, PixelFormat32bppARGB);
	{
		Gdiplus::Graphics g(&dstBmp);
		g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
		g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
		g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		g.DrawImage(&srcBmp, Gdiplus::Rect(0, 0, dstW, dstH), 0, 0, srcW, srcH, Gdiplus::UnitPixel);
	}

	// Decorate
	if (opt.addWatermark || opt.addTimestamp || !opt.redactRects.empty()) {
		Decorate(dstBmp, opt);
	}

	// Encode JPEG → IStream
	Gdiplus::EncoderParameters ep; ep.Count = 1;
	Gdiplus::EncoderParameter& p = ep.Parameter[0];
	p.Guid = Gdiplus::EncoderQuality;
	p.Type = Gdiplus::EncoderParameterValueTypeLong;
	p.NumberOfValues = 1;
	ULONG q = std::clamp(opt.quality, 1ul, 100ul);
	p.Value = &q;

	IStream* stm = nullptr;
	if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stm))) return false;
	const auto st = dstBmp.Save(stm, &g_jpegClsid, &ep);
	if (st != Gdiplus::Ok) { stm->Release(); return false; }

	STATSTG ss{}; if (FAILED(stm->Stat(&ss, STATFLAG_NONAME))) { stm->Release(); return false; }
	const SIZE_T len = static_cast<SIZE_T>(ss.cbSize.QuadPart);
	out.resize(len);
	LARGE_INTEGER z{}; ULARGE_INTEGER np{};
	if (FAILED(stm->Seek(z, STREAM_SEEK_SET, &np))) { stm->Release(); return false; }
	ULONG rd = 0; if (FAILED(stm->Read(out.data(), (ULONG)len, &rd)) || rd != len) { stm->Release(); return false; }
	stm->Release();
	return true;
}

// ===================== HTTP Helper (curl_mime) =====================
static bool PostMultipart(
	const char* url,
	const std::vector<std::string>& headers,
	const char* jpegName, const void* jpegData, size_t jpegLen,
	// optional binary
	const char* binName = nullptr, const void* binData = nullptr, size_t binLen = 0,
	// optional payload_json (Discord)
	const char* payloadJson = nullptr
) {
	std::call_once(g_onceCurl, InitCurlOnce);

	CURL* curl = curl_easy_init();
	if (!curl) return false;

	curl_mime* mime = curl_mime_init(curl);
	if (!mime) { curl_easy_cleanup(curl); return false; }

	// JPEG part
	{
		curl_mimepart* p = curl_mime_addpart(mime);
		curl_mime_name(p, "file");
		curl_mime_filename(p, jpegName ? jpegName : "sc.jpeg");
		curl_mime_data(p, (const char*)jpegData, jpegLen);
		curl_mime_type(p, "image/jpeg");
	}

	// Optional binary
	if (binName && binData && binLen > 0) {
		curl_mimepart* p = curl_mime_addpart(mime);
		curl_mime_name(p, "binary");
		curl_mime_filename(p, binName);
		curl_mime_data(p, (const char*)binData, binLen);
		curl_mime_type(p, "application/octet-stream");
	}

	// Optional payload_json (Discord)
	if (payloadJson) {
		curl_mimepart* p = curl_mime_addpart(mime);
		curl_mime_name(p, "payload_json");
		curl_mime_data(p, payloadJson, CURL_ZERO_TERMINATED);
	}

	struct curl_slist* hdr = nullptr;
	for (auto& h : headers) hdr = curl_slist_append(hdr, h.c_str());


	std::string resp;
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
	curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t sz, size_t nm, void* ud)->size_t {
		auto* s = static_cast<std::string*>(ud);
		s->append(ptr, sz * nm);
		return sz * nm;
		});

	CURLcode rc = curl_easy_perform(curl);

	curl_slist_free_all(hdr);
	curl_mime_free(mime);
	curl_easy_cleanup(curl);

	return (rc == CURLE_OK);
}

// ===================== Public API (unchanged signatures) =====================

// Discord webhook: gửi JPEG + embeds
bool GetScreenShot(std::string webhookUrl, std::string fileName, std::string desc, int color, bool notify)
{
	std::vector<uint8_t> jpeg;
	if (!CaptureScreenToJpegEx(jpeg, g_capOpt)) return false;

	// payload_json cho Discord
	json imageEmbed = {
		{"image", { {"url", "attachment://sc.jpeg"} }},
		{"author", { {"name", "Launcher Screen Shot"}, {"icon_url", "https://sp.lorax.vn/storage/v1/object/public/images/logo.png"} }},
		{"title", fileName},
		{"color", color},
		{"description", desc}
	};
	json payload; payload["embeds"] = json::array({ imageEmbed });
	if (notify) payload["content"] = "@everyone";
	const std::string body = payload.dump();

	std::vector<std::string> headers;
	headers.emplace_back("Expect:");

	return PostMultipart(
		webhookUrl.c_str(), headers,
		"sc.jpeg", jpeg.data(), jpeg.size(),
		/*bin*/ nullptr, nullptr, 0,
		/*payload_json*/ body.c_str()
	);
}

// FxAPI upload: gửi JPEG + (tuỳ chọn) file nhị phân + headers yêu cầu
bool GetScreenShotV2(std::string checksum, std::string filePath, std::string hwid,
	std::string notificationType, std::string detectionType,
	bool sendFile, bool isBlockScreenshot)
{
	std::vector<uint8_t> jpeg;
	if (!CaptureScreenToJpegEx(jpeg, g_capOpt)) return false;

	// Optional binary
	std::vector<uint8_t> bin;
	const char* binName = nullptr;
	if (sendFile) {
		HANDLE fh = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (fh != INVALID_HANDLE_VALUE) {
			DWORD sz = GetFileSize(fh, NULL);
			if (sz > 0 && sz < (64u << 20)) { // giới hạn 64MB
				bin.resize(sz);
				DWORD rd = 0; ReadFile(fh, bin.data(), sz, &rd, NULL);
				bin.resize(rd);
				binName = filePath.c_str();
			}
			CloseHandle(fh);
		}
	}

	// Headers theo API
	std::vector<std::string> headers;
	headers.emplace_back("Expect:");
	headers.emplace_back(fmt::sprintf("Checksum:%s", checksum));
	headers.emplace_back(fmt::sprintf("LauncherName:%s", ToNarrow(PRODUCT_NAME)));
	headers.emplace_back(fmt::sprintf("FilePath:%s", filePath));
	headers.emplace_back(fmt::sprintf("HWIDTokens:%s", hwid));
	headers.emplace_back(fmt::sprintf("FxAPI-Notification-Type:%s", notificationType));
	headers.emplace_back(fmt::sprintf("FxAPI-Detection-Type:%s", detectionType));
	headers.emplace_back(fmt::sprintf("FxAPI-Blocked-Screenshot:%s", isBlockScreenshot ? "1" : "0"));
	headers.emplace_back("Secret:RVcfgXAMCh9jyHKN9TBHjeBS15d7ysjyeJC43jd0rmrQdzuzgwHj5cYn8uHqXS9e");

	const char* Url = AY_OBFUSCATE("https://fxapi.lorax.vn/upload");

	std::vector<FxFile> files = {};
	json meta;
	meta["Checksum"] = checksum;
	meta["LauncherName"] = ToNarrow(PRODUCT_NAME);
	meta["FilePath"] = filePath;
	meta["HWIDTokens"] = hwid;
	meta["FxAPI-Notification-Type"] = notificationType;
	meta["FxAPI-Detection-Type"] = detectionType;
	meta["FxAPI-Blocked-Screenshot"] = isBlockScreenshot ? "1" : "0";
	meta["Secret"] = "RVcfgXAMCh9jyHKN9TBHjeBS15d7ysjyeJC43jd0rmrQdzuzgwHj5cYn8uHqXS9e";

	files.push_back(FxFile{
			"sc.jpeg",
			"image/jpeg",
			jpeg
		});

	SendToFxAPI_V2("UPLOAD", meta.dump(), files, [](bool success, std::string data) {});

	/*return PostMultipart(
		Url, headers,
		"sc.jpeg", jpeg.data(), jpeg.size(),
		binName, (binName ? bin.data() : nullptr), (binName ? bin.size() : 0),
		 nullptr
	);*/
}
