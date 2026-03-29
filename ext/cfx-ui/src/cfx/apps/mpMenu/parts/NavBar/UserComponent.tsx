import { observer } from "mobx-react-lite";
import React, { useState } from "react";
import { useLauncherService } from "../../services/launcher/launcher.service";
import { Box as BoxBase, Image, Text as TextBase, Button } from "lr-components";
import { useNavigate } from "react-router-dom";
import { mpMenu } from "../../mpMenu";
import { Icon } from "@iconify/react";
import { clsx } from "@cfx-dev/ui-components";
import { AnimatePresence, motion } from "motion/react";

import { useService } from 'cfx/base/servicesContainer';
import { ISettingsUIService } from 'cfx/common/services/settings/settings.service';
import { useEventHandler } from 'cfx/common/services/analytics/analytics.service';
import { EventActionNames, ElementPlacements } from 'cfx/common/services/analytics/types';

const Exiter = observer(() => {
  return (
    <button
      style={{}}
      className="size-[3.5rem] bg-red-500 border-none outline-none rounded-[.35rem] flex items-center justify-center cursor-pointer hover:opacity-75 transition-all duration-300"
      onClick={mpMenu.exit}
    >
      <Icon icon="ic:round-close" className="size-[2.25rem] text-[#fff]" />
    </button>
  );
});

const Box = BoxBase as any;
const Text = TextBase as any;

const UserComponent = observer(() => {
  const SettingsUIService = useService(ISettingsUIService);
  const eventHandler = useEventHandler();
  const LauncherService = useLauncherService();
  const navigate = useNavigate();

  const [showMenu, setShowMenu] = useState<boolean>(false);

  const handleSettingsClick = React.useCallback(() => {
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

  if (!LauncherService.user) {
    return (
      <AnimatePresence mode="wait">
        <motion.div
          initial={{ opacity: 0, x: 50 }}
          animate={{ opacity: 1, x: 0 }}
          exit={{ opacity: 0, x: 50 }}
          transition={{
            duration: 0.25,
            ease: [0.43, 0.13, 0.23, 0.96],
          }}
          className="w-[25%] h-full"
        >
          <Box className="size-full flex items-center justify-end" rGap={18}>
            <Box className="flex" rGap={12}>
              <button
                className={clsx(
                  "w-[11rem] h-[3.5rem] cursor-pointer hover:bg-[#6b728066] bg-[#6b728033] border-[.1rem] border-[#9ca3af4d] rounded-[.35rem] flex items-center justify-center gap-[.5rem] transition-all duration-300 outline-none"
                )}
                style={{
                  backdropFilter: "blur(10px)",
                  boxShadow:
                    "0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06)",
                }}
                onClick={() => {
                  navigate("/login");
                }}
              >
                <span className="text-white font-medium text-[1.15rem]">
                  Đăng nhập
                </span>
                <Icon
                  icon="material-symbols:login-rounded"
                  className="size-[1.5rem] text-[#fff]"
                />
              </button>
              <button
                className={clsx(
                  "w-[11rem] h-[3.5rem] cursor-pointer hover:bg-[#6b728066] bg-[#6b728033] border-[.1rem] border-[#9ca3af4d] rounded-[.35rem] flex items-center justify-center gap-[.5rem] transition-all duration-300 outline-none"
                )}
                style={{
                  backdropFilter: "blur(10px)",
                  boxShadow:
                    "0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06)",
                }}
                onClick={() => {
                  navigate("/register");
                }}
              >
                <span className="text-white font-medium text-[1.15rem]">
                  Đăng ký
                </span>
                <Icon icon="ic:outline-how-to-reg" className="size-[1.5rem] text-[#fff]" />
              </button>
            </Box>
            <Box
              rWidth={1}
              rHeight={50}
              className="bg-white rounded-full flex-shrink-0"
            />
            <Exiter />
          </Box>
        </motion.div>
      </AnimatePresence>
    );
  }

  return (
    <AnimatePresence mode="wait">
      <motion.div
        initial={{ opacity: 0, x: 50 }}
        animate={{ opacity: 1, x: 0 }}
        exit={{ opacity: 0, x: 50 }}
        transition={{
          duration: 0.25,
          ease: [0.43, 0.13, 0.23, 0.96],
        }}
        className="w-[25%] h-full"
      >
        <Box className="size-full flex items-center justify-end" rGap={18}>
          <Box className="flex items-center justify-end relative" rGap={16}>
            <p className="text-white font-medium text-[1.35rem]">
              Xin chào,{" "}
              <span className="font-bold">{LauncherService.user.username}</span>
            </p>
            <Box
              className="size-[3.25rem] border-[.1rem] border-white rounded-full flex items-center justify-center cursor-pointer relative"
              onClick={() => setShowMenu(!showMenu)}
            >
              <Image
                src="https://cloud.gtaxvn.com/images/default-avatar.png"
                className="object-cover rounded-full"
                rWidth={38}
                rHeight={38}
                draggable={false}
              />

              <AnimatePresence>
                {showMenu && (
                  <motion.div
                    initial={{ opacity: 0, y: -10, scale: 0.95 }}
                    animate={{ opacity: 1, y: 0, scale: 1 }}
                    exit={{ opacity: 0, y: -10, scale: 0.95 }}
                    transition={{ duration: 0.2, ease: "easeOut" }}
                    className="absolute top-full right-0 mt-3 w-[14rem] z-50"
                  >
                    <Box
                      className="w-full backdrop-blur-[16px] border border-[#ffffff33] rounded-[.65rem] overflow-hidden"
                      style={{
                        background: "#ffffff1a",
                        boxShadow:
                          "0 8px 32px rgba(0, 0, 0, 0.1), inset 0 1px 0 rgba(255, 255, 255, 0.3)",
                      }}
                    >
                      <Box
                        className="w-full py-4 px-4 cursor-pointer hover:bg-[#ffffff22] transition-all duration-300 flex items-center gap-3 border-b border-[#ffffff22]"
                        onClick={() => {
                          navigate("/change-password");
                          setShowMenu(false);
                        }}
                      >
                        <Icon
                          icon="material-symbols:lock-outline"
                          className="size-[1.25rem] text-white"
                        />
                        <Text className="text-white font-medium text-[1rem]">
                          Đổi mật khẩu
                        </Text>
                      </Box>
                      {LauncherService?.user?.role === "admin" && (
                        <Box
                          className="w-full py-4 px-4 cursor-pointer hover:bg-[#ffffff22] transition-all duration-300 flex items-center gap-3 border-b border-[#ffffff22]"
                          onClick={handleSettingsClick}
                        >
                          <Icon
                            icon="material-symbols:lock-outline"
                            className="size-[1.25rem] text-white"
                          />
                          <Text className="text-white font-medium text-[1rem]">
                            Cài Đặt
                          </Text>
                        </Box>
                      )}
                      <Box
                        className="w-full py-4 px-4 cursor-pointer hover:bg-[#ff000022] transition-all duration-300 flex items-center gap-3"
                        onClick={() => {
                          LauncherService.logout();
                          setShowMenu(false);
                        }}
                      >
                        <Icon
                          icon="material-symbols:logout"
                          className="size-[1.25rem] text-[#ff4444]"
                        />
                        <Text className="text-[#ff4444] font-medium text-[1rem]">
                          Đăng xuất
                        </Text>
                      </Box>
                    </Box>
                  </motion.div>
                )}
              </AnimatePresence>
            </Box>
          </Box>
          <Box
            rWidth={1}
            rHeight={50}
            className="bg-white rounded-full flex-shrink-0"
          />
          <Exiter />
        </Box>
      </motion.div>
    </AnimatePresence>
  );
});

export default UserComponent;
