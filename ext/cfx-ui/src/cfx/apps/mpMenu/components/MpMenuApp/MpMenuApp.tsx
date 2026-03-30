/* eslint-disable import/no-extraneous-dependencies */
import { useState, useCallback, useEffect } from "react";
import { Box as BoxBase, Image, Text } from "lr-components";
import { observer } from "mobx-react-lite";
import { Outlet, useNavigate } from "react-router-dom";
import { AnimatePresence, motion } from "motion/react";
import { Icon } from "@iconify/react";

import { LegacyConnectingModal } from "cfx/apps/mpMenu/parts/LegacyConnectingModal/LegacyConnectingModal";
import { LegacyUiMessageModal } from "cfx/apps/mpMenu/parts/LegacyUiMessageModal/LegacyUiMessageModal";
import { NavBar } from "cfx/apps/mpMenu/parts/NavBar/NavBar";
import { SettingsFlyout } from "cfx/apps/mpMenu/parts/SettingsFlyout/SettingsFlyout";
import { ThemeManager } from "cfx/apps/mpMenu/parts/ThemeManager/ThemeManager";
import { mpMenu } from "../../mpMenu";
import { IServersConnectService } from "cfx/common/services/servers/serversConnect.service";

import { useLauncherService } from "../../services/launcher/launcher.service";
import NotificationComponent from "../../parts/Notification/NotificationComponent";
import { ExternalLinkDialog, externalLinkState } from "../../parts/ExternalLinkDialog/ExternalLinkDialog";
import { useService } from 'cfx/base/servicesContainer';
import { ISettingsUIService } from 'cfx/common/services/settings/settings.service';
import { useEventHandler } from 'cfx/common/services/analytics/analytics.service';
import { EventActionNames, ElementPlacements } from 'cfx/common/services/analytics/types';

const Box = BoxBase as any;
const sideBarWrapper = new URL("assets/images/sidebar-wrapper.webp", import.meta.url).href;
const defaultAvatar = new URL("assets/images/default-user.webp", import.meta.url).href;
const userAvatar = new URL("assets/images/user-avatar.webp", import.meta.url).href;
const discordIcon = new URL("assets/images/discord-icon.webp", import.meta.url).href;
const facebookIcon = new URL("assets/images/facebook-icon.webp", import.meta.url).href;
const grandText1 = new URL("assets/images/grand-1.webp", import.meta.url).href;
const grandText2 = new URL("assets/images/grand-2.webp", import.meta.url).href;
const linkIcon = new URL("assets/images/link-icon.webp", import.meta.url).href;
const exitIcon = new URL("assets/images/exit-icon.webp", import.meta.url).href;
const playBtnShadow = new URL("assets/images/play-btn-shadow.webp", import.meta.url).href;
const playIcon = new URL("assets/images/play-icon.webp", import.meta.url).href;
const serverIcon = new URL("assets/images/server-icon.webp", import.meta.url).href;
const popUpDivider = new URL("assets/images/popup-divider.webp", import.meta.url).href;

export const MpMenuApp = observer(function MpMenuApp() {
  const LauncherService = useLauncherService();
  const ServersConnectService = useService(IServersConnectService);
  const SettingsUIService = useService(ISettingsUIService);
  const eventHandler = useEventHandler();

  const [showMenu, setShowMenu] = useState<boolean>(false);
  const [showAvatarInput, setShowAvatarInput] = useState<boolean>(false);
  const [avatarUrl, setAvatarUrl] = useState<string>(localStorage.getItem("grandrp_avatar") || "");
  const [avatarInput, setAvatarInput] = useState<string>("");

  const userAvatarSrc = avatarUrl || (LauncherService?.user ? userAvatar : defaultAvatar);

  const saveAvatar = () => {
    const url = avatarInput.trim();
    if (url) {
      localStorage.setItem("grandrp_avatar", url);
      setAvatarUrl(url);
    } else {
      localStorage.removeItem("grandrp_avatar");
      setAvatarUrl("");
    }
    setShowAvatarInput(false);
    setAvatarInput("");
  };

  const userData = LauncherService?.user;
  const addNotification = LauncherService?.addNotification;
  const serverList = LauncherService?.servers;
  const loading = LauncherService?.loading;
  const navigate = useNavigate();

  useEffect(() => {
    if (!loading && !userData) {
      navigate("/login");
    }
  }, [loading, userData]);

  useEffect(() => {
    mpMenu.setOpenUrlHandler((url: string) => {
      externalLinkState.show(url);
    });
  }, []);

  const handleSettingsClick = useCallback(() => {
    SettingsUIService.open();
    eventHandler({
      action: EventActionNames.SiteNavClick,
      properties: {
        text: '#BottomNav_Settings',
        link_url: '/',
        element_placement: ElementPlacements.Nav,
        position: 0,
      },
    });
  }, [eventHandler, SettingsUIService]);

  return (
    <>
      <ThemeManager />

      <SettingsFlyout />

      <LegacyConnectingModal />
      <LegacyUiMessageModal />
      <ExternalLinkDialog />

      {/* Avatar URL Dialog */}
      <AnimatePresence>
        {showAvatarInput && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            style={{
              position: "fixed", inset: 0, zIndex: 9999,
              display: "flex", alignItems: "center", justifyContent: "center",
              background: "rgba(0,0,0,0.7)", backdropFilter: "blur(8px)",
            }}
            onClick={() => setShowAvatarInput(false)}
          >
            <motion.div
              initial={{ opacity: 0, scale: 0.9 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 0.9 }}
              onClick={(e) => e.stopPropagation()}
              style={{
                width: 420,
                background: "linear-gradient(180deg, #1A1B1F 0%, #111214 100%)",
                border: "1px solid rgba(255,255,255,0.08)",
                borderRadius: 16, padding: 28,
                display: "flex", flexDirection: "column", gap: 16,
              }}
            >
              <div style={{ color: "#fff", fontSize: 16, fontWeight: 600 }}>
                Đổi avatar
              </div>
              {avatarInput && (
                <img
                  src={avatarInput}
                  alt="Preview"
                  style={{
                    width: 80, height: 80, borderRadius: "50%",
                    objectFit: "cover", alignSelf: "center",
                    border: "2px solid rgba(241,152,0,0.5)",
                  }}
                  onError={(e) => { (e.target as HTMLImageElement).style.display = "none"; }}
                />
              )}
              <input
                type="text"
                value={avatarInput}
                onChange={(e) => setAvatarInput(e.target.value)}
                placeholder="Dán link hình ảnh (URL)..."
                style={{
                  width: "100%", padding: "10px 14px", borderRadius: 10,
                  border: "1px solid rgba(255,255,255,0.1)",
                  background: "rgba(255,255,255,0.04)", color: "#fff",
                  fontSize: 13, outline: "none",
                }}
              />
              <div style={{ color: "rgba(255,255,255,0.35)", fontSize: 11 }}>
                Để trống để dùng avatar mặc định
              </div>
              <div style={{ display: "flex", gap: 10 }}>
                <button
                  onClick={() => setShowAvatarInput(false)}
                  style={{
                    flex: 1, padding: "10px 0", borderRadius: 10,
                    border: "1px solid rgba(255,255,255,0.1)",
                    background: "transparent", color: "rgba(255,255,255,0.6)",
                    fontSize: 14, cursor: "pointer",
                  }}
                >
                  Huỷ
                </button>
                <button
                  onClick={saveAvatar}
                  style={{
                    flex: 1, padding: "10px 0", borderRadius: 10,
                    border: "none",
                    background: "linear-gradient(180deg, #F19800 0%, #EC6B14 100%)",
                    color: "#fff", fontSize: 14, fontWeight: 600, cursor: "pointer",
                  }}
                >
                  Lưu
                </button>
              </div>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>

      <Box
        className="w-screen h-screen flex items-center relative"
        rGap={20}
      >
        {/* <Image
          src={new URL("assets/images/effect_bg.png", import.meta.url).href}
          alt="Logo"
          className="w-full absolute bottom-0"
          rHeight={12}
        /> */}
        <Box className="w-[93.75%] h-full flex flex-col justify-between relative"
          rPadding={[0, 0, 40, 0]}
        >
          <NavBar />
          <Box className="flex flex-col -mt-[4.85rem]" rGap={32} rPadding={[0, 0, 0, 10]}>
            {Array.from({ length: 3 }).map((_, index) => {
              const server = serverList?.[index];
              const isActive = index === 0;

              return (
                <Box
                  key={server?.id || index}
                  className={`flex items-center relative w-fit ${isActive ? 'cursor-pointer' : 'pointer-events-none opacity-[.57]'
                    }`}
                  onClick={() => {
                    if (!isActive) return;
                  }}
                >
                  {isActive && (
                    <Box
                      className="absolute top-1/2 -translate-y-1/2 left-0 bg-[#FF8000] rounded-[10rem]"
                      rWidth={7}
                      rHeight={33}
                    />
                  )}
                  <Image
                    src={serverIcon}
                    alt={`Server ${index + 1} Icon`}
                    className="object-cover"
                    rMargin={[0, 0, 0, 20]}
                    rWidth={52}
                    rHeight={52}
                  />
                  <Text
                    className="text-white font-[510] capitalize"
                    rFontSize={16}
                    rMargin={[0, 0, 0, 10]}
                  >
                    {server?.hostname || server?.projectName || `Server ${index + 1} - Offline`}
                  </Text>
                </Box>
              );
            })}
          </Box>
          <Box
            className="w-full flex items-center justify-between"
            rPadding={[0, 112, 24, 62]}
          >
            <Box className="flex flex-col">
              <Box className="flex flex-col -mb-[.1rem]">
                <Box className="flex items-center -mb-[.25rem]" rGap={10}>
                  <Image
                    src={grandText1}
                    alt="Grand Text 1"
                    className="object-contain"
                    rWidth={233}
                    rHeight={76}
                  />
                  <Image
                    src={linkIcon}
                    alt="Link Icon"
                    className="object-cover"
                    rWidth={51}
                    rHeight={51}
                  />
                </Box>
                <Image
                  src={grandText2}
                  alt="Grand Text 2"
                  className="object-contain"
                  rWidth={333}
                  rHeight={76}
                />
              </Box>
              <Text
                variant="p"
                className="text-[#FFF5DD78] font-normal max-w-[46rem]"
                rFontSize={16}
              >
                Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book.
              </Text>
            </Box>
            <button
              className="mt-[8rem] cursor-pointer flex items-center justify-center gap-[.85rem] outline-none w-[16rem] h-[4.4rem] rounded-[10rem]"
              style={{
                background: "linear-gradient(180deg, #F19800 0%, #EC6B14 100%)",
              }}
              onClick={() => {
                if (!serverList?.[0]) return;
                if (!userData) {
                  addNotification({
                    message: "Đăng nhập để tham gia máy chủ",
                    type: "info"
                  });
                  navigate("/login");
                  return;
                }
                ServersConnectService?.connectTo(serverList[0]);
              }}
            >
              <Image
                src={playIcon}
                alt="Play Icon"
                className="object-cover"
                rWidth={31}
                rHeight={31}
              />
              <Text
                className="text-white font-[510]"
                rFontSize={32}
              >
                Chơi ngay
              </Text>
            </button>
          </Box>
          <Image
            src={playBtnShadow}
            alt="Play Button Shadow"
            className="absolute bottom-0 right-0 pointer-events-none"
            rWidth={448}
            rHeight={140}
          />
        </Box>
        <Box
          className="w-[6.25%] h-full flex flex-col items-center"
          rGap={31.5}
          rPadding={[39, 0, 40, 0]}
          style={{
            background: `url(${sideBarWrapper}) no-repeat center center`,
            backgroundSize: '100% 100%'
          }}
        >
          <Box
            rWidth={64}
            rHeight={64}
            className={`relative ${LauncherService?.user ? 'cursor-pointer' : ''}`}
            onClick={() => {
              if (!LauncherService?.user) return;
              setShowMenu(!showMenu);
            }}
          >
            <Image
              src={userAvatarSrc}
              alt="User Avatar"
              className="size-full object-cover"
              draggable={false}
            />
            <AnimatePresence>
              {showMenu && (
                <motion.div
                  initial={{ opacity: 0, y: -10, scale: 0.95 }}
                  animate={{ opacity: 1, y: 0, scale: 1 }}
                  exit={{ opacity: 0, y: -10, scale: 0.95 }}
                  transition={{ duration: 0.2, ease: "easeOut" }}
                  className="absolute top-[58%] right-[2.25rem] w-[14rem] z-50"
                >
                  <Box
                    className="w-full border border-[#FFFFFF54] overflow-hidden"
                    rBorderRadius={16}
                    rPadding={[16, 12, 16, 12]}
                    style={{
                      background: "linear-gradient(180deg, #25262A 0%, rgba(0, 0, 0, 0.54) 100%)"
                    }}
                  > 
                    <Box
                      className="w-full flex items-center justify-center"
                      rMargin={[0, 0, 10, 0]}
                    >
                      <Text className="font-[510] text-white capitalize" rFontSize={16}>{LauncherService?.user?.username}</Text>
                    </Box>
                    <Image 
                      src={popUpDivider}
                      alt="Pop Up Divider"
                      className="w-full object-cover"
                      rHeight={1}
                      rMargin={[0, 0, 13, 0]}
                    />
                    <Box
                      className="w-full bg-[#00000080] group hover:bg-[#595959] transition-all duration-300 flex items-center justify-center cursor-pointer"
                      rHeight={34}
                      rBorderRadius={8}
                      rMargin={[0, 0, 6, 0]}
                      onClick={() => {
                        navigate("/change-password");
                        setShowMenu(false);
                      }}
                    >
                      <Text className="text-[#ffffff80] font-normal group-hover:text-[#FDCA70] transition-all duration-300" rFontSize={16}>
                        Đổi mật khẩu
                      </Text>
                    </Box>
                    <Box
                      className="w-full bg-[#00000080] group hover:bg-[#595959] transition-all duration-300 flex items-center justify-center cursor-pointer"
                      rHeight={34}
                      rBorderRadius={8}
                      rMargin={[0, 0, 6, 0]}
                      onClick={() => {
                        setShowAvatarInput(true);
                        setShowMenu(false);
                        setAvatarInput(avatarUrl);
                      }}
                    >
                      <Text className="text-[#ffffff80] font-normal group-hover:text-[#FDCA70] transition-all duration-300" rFontSize={16}>
                        Đổi avatar
                      </Text>
                    </Box>
                    {LauncherService?.user?.role === "admin" && (
                      <Box
                        className="w-full bg-[#00000080] group hover:bg-[#595959] transition-all duration-300 flex items-center justify-center cursor-pointer"
                        rHeight={34}
                        rBorderRadius={8}
                        rMargin={[0, 0, 6, 0]}
                        onClick={handleSettingsClick}
                      >
                        <Text className="text-[#ffffff80] font-normal group-hover:text-[#FDCA70] transition-all duration-300" rFontSize={16}>
                          Cài Đặt
                        </Text>
                      </Box>
                    )}
                    <Box
                      className="w-full bg-[#00000080] group hover:bg-[#595959] transition-all duration-300 flex items-center justify-center cursor-pointer"
                      rHeight={34}
                      rBorderRadius={8}
                      onClick={() => {
                        LauncherService.logout();
                        setShowMenu(false);
                      }}
                    >
                      <Text className="text-[#ffffff80] font-normal group-hover:text-[#FDCA70] transition-all duration-300" rFontSize={16}>
                        Đăng xuất
                      </Text>
                    </Box>
                  </Box>
                </motion.div>
              )}
            </AnimatePresence>
          </Box>
          <Box className="flex flex-col" rGap={14}>
            <Image
              src={discordIcon}
              alt="Discord Icon"
              className="object-cover cursor-pointer hover:brightness-200 hover:scale-110 transition-all duration-300"
              rWidth={48}
              rHeight={48}
              onClick={() => {
                mpMenu.invokeNative("openUrl", "https://discord.gg/grandrp");
              }}
            />
            <Image
              src={facebookIcon}
              alt="Facebook Icon"
              className="object-cover cursor-pointer hover:brightness-200 hover:scale-110 transition-all duration-300"
              rWidth={48}
              rHeight={48}
              onClick={() => {
                mpMenu.invokeNative("openUrl", "https://www.facebook.com/cobeometri/");
              }}
            />
          </Box>
          <Image
            src={exitIcon}
            alt="Exit Icon"
            className="mt-auto object-cover cursor-pointer hover:brightness-200 hover:scale-110 transition-all duration-300"
            rWidth={69}
            rHeight={69}
            onClick={() => mpMenu.exit()}
          />
        </Box>
        {/* <Box
          className="size-full flex items-center gap-[1.35rem] mt-[6.5rem]"
          rPadding={[0, 40]}
          rGap={28}
        >
          <Box className="w-[54rem] flex flex-col items-start" rGap={32}>
            <YoutubeFrame />
            <News />
          </Box>
          {Array.from({ length: 2 }, (_, index) => (
            <Server index={index} key={index} server={serverList[index]} />
          ))}
          <Box className="flex flex-col" rGap={28}>
            <Image
              src={new URL("assets/images/facebook.png", import.meta.url).href}
              rWidth={80}
              rHeight={80}
              className="hover:cursor-pointer hover:brightness-200"
              onClick={() => {
                mpMenu.invokeNative(
                  "openUrl",
                  globalData?.fanpage_url ||
                    "https://www.facebook.com/cobeometri/"
                );
              }}
            />
            <Image
              src={new URL("assets/images/discord.png", import.meta.url).href}
              rWidth={80}
              rHeight={80}
              className="hover:cursor-pointer hover:brightness-200"
              onClick={() => {
                mpMenu.invokeNative(
                  "openUrl",
                  globalData?.discord_url ||
                    "https://www.facebook.com/cobeometri/"
                );
              }}
            />
            <Image
              src={new URL("assets/images/youtube.png", import.meta.url).href}
              rWidth={80}
              rHeight={80}
              className="hover:cursor-pointer hover:brightness-200"
              onClick={() => {
                mpMenu.invokeNative(
                  "openUrl",
                  globalData?.youtube_url ||
                    "https://www.facebook.com/cobeometri/"
                );
              }}
            />
            <Image
              src={new URL("assets/images/tiktok.png", import.meta.url).href}
              rWidth={80}
              rHeight={80}
              className="hover:cursor-pointer hover:brightness-200"
              onClick={() => {
                mpMenu.invokeNative(
                  "openUrl",
                  globalData?.tiktok_url ||
                    "https://www.facebook.com/cobeometri/"
                );
              }}
            />
          </Box>
        </Box> */}

        <Outlet />
      </Box>

      <Box className="fixed top-[1.5rem] left-1/2 -translate-x-1/2 z-[9999] flex flex-col items-end">
        <AnimatePresence mode="popLayout">
          {LauncherService.notifications.map((notification) => (
            <NotificationComponent data={notification} key={notification.id} />
          ))}
        </AnimatePresence>
      </Box>
    </>
  );
});
