/* eslint-disable camelcase */
import { clsx } from "@cfx-dev/ui-components";
import { Box, Image, Text } from "lr-components";
import { observer } from "mobx-react-lite";
import { useLocation, useNavigate } from "react-router-dom";
import { useLauncherService } from "../../services/launcher/launcher.service";

const logo = new URL("assets/images/logo.webp", import.meta.url).href;

export const NavBar = observer(() => {
  const { pathname } = useLocation();
  const navigate = useNavigate();
  const LauncherService = useLauncherService();

  return (
    <Box
      className="w-full flex items-center justify-between z-[5]"
      rMargin={[40, 40, 0, 0]}
    >
      <Box className="h-full flex items-center" rGap={18} rPadding={[0, 0, 0, 40]}>
        <Image
          src={logo}
          alt="Logo"
          className={clsx("object-cover")}
          rWidth={67}
          rHeight={57}
        />
        <Box className="flex flex-col gap-[.55rem]">
          <Text className="font-normal text-white" rFontSize={16}>
            Trực tuyến
          </Text>
          <Text className="text-[#82FF5F] font-bold" rFontSize={16}>
            {LauncherService.totalPlayersOnline}/{LauncherService.totalPlayersMax}
          </Text>
        </Box>
      </Box>
      {!LauncherService?.user && (
        <Box className="flex flex-col items-end justify-end gap-[.53rem]">
          <Text className="font-normal text-white uppercase" rFontSize={18}>
            Bạn chưa có tài khoản?
          </Text>
          <Text 
            className="text-[#82FF5F] font-bold hover:underline cursor-pointer uppercase" rFontSize={18}
            onClick={() => {
              navigate("/register");
            }}
          >
            Đăng ký ngay
          </Text>
        </Box>
      )}
    </Box>
  );
});
