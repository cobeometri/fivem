-- sticky component build config (EOSSDK removed)
links {
	"Gdiplus",
	"iphlpapi",
	"ws2_32",
	"ntdll",
	"psapi",
	"Wininet",
}

add_dependencies {
	'vendor:curl-crt',
}
