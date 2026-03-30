links { "ws2_32" }

return function()
	filter {}

	add_dependencies { 'legitimacy', 'vendor:openssl_crypto' }

	files {
		"D:/fivem/code/client/shared/HwidChecker.cpp",
		"D:/fivem/code/client/shared/HwidChecker.h",
	}

	includedirs {
		"D:/fivem/code/client/shared/",
		"D:/fivem/vendor/curl/include/",
	}

	if _OPTIONS["game"] == 'ny' then
		add_dependencies {
			'vendor:enet',
		}
	else
		add_dependencies {
			"vendor:citizen_enet",
			"vendor:citizen_util",
		}
	end
end
