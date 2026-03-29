import { clsx } from "@cfx-dev/ui-components";
import { Box as BoxBase, Image, Text as TextBase } from "lr-components";
import { observer } from "mobx-react-lite";
import { Icon } from "@iconify/react";
import { useLauncherService } from "cfx/apps/mpMenu/services/launcher/launcher.service";
import { useNavigate } from "react-router-dom";

import { IServerView } from "cfx/common/services/servers/types";
import { useServiceOptional } from "cfx/base/servicesContainer";
import { IServersConnectService } from "cfx/common/services/servers/serversConnect.service";

import styles from "./Server.module.scss";

interface ServerProps {
  index: number;
  server?: IServerView;
}

const Box = BoxBase as any;
const Text = TextBase as any;

const Server = observer(({ index, server }: ServerProps) => {
  const navigate = useNavigate();
  const ServersConnectService = useServiceOptional(IServersConnectService);
  const LauncherService = useLauncherService();

  const userData = LauncherService?.user;
  const addNotification = LauncherService?.addNotification;

  return (
    <Box
      rWidth={530}
      rHeight={855}
      className={clsx(
        "relative rounded-[.65rem] group overflow-hidden",
        !server && "grayscale",
        styles["server-container"]
      )}
      style={{
        backgroundImage: `url("https://cloud.fivemvn.com/launcher-sgc/${index === 0 ? "server-1" : "server-2"
          }.png")`,
        backgroundSize: "100% 100%",
        backgroundPosition: "center center",
        backgroundRepeat: "no-repeat",
      }}
    >
      <Box className="absolute inset-0 bg-[#0000004d] z-[1] group-hover:opacity-100 opacity-0 transition-all duration-300" />
      <Image
        src="https://cloud.fivemvn.com/launcher-sgc/element2.png"
        alt="element2"
        className="absolute bottom-0 left-0 w-full h-[20rem] z-[2]"
      />
      <Box
        className={clsx(
          styles["main-server-group-badge"],
          server && styles["active"]
        )}
      >
        <Text className={styles["main-server-group-badge-title"]}>
          {server ? server.hostname : "Không hoạt động"}
        </Text>
      </Box>
      <Box className="absolute bottom-[1.65rem] left-1/2 -translate-x-1/2 z-[3]">
        <button
          className={clsx(
            styles["main-play-button"],
            server && styles["active"]
          )}
          onClick={() => {
            if (!server) return;
            if (!userData) {
              addNotification({
                message: "Đăng nhập để tham gia máy chủ",
                type: "info"
              });
              navigate("/login");
              return;
            }
            ServersConnectService?.connectTo(server);
          }}
        >
          <Text className={styles["main-play-button-text"]}>
            {server ? "Tham gia ngay" : "Đang tạm dừng"}
          </Text>
          <Icon
            icon={server ? "icon-park-outline:play" : "healthicons:cancel-24px"}
            className={clsx(
              "size-[1.85rem] -mt-[.15rem]",
              server ? "text-[#00d0ff]" : "text-[#fff]"
            )}
          />
        </button>
      </Box>
      {/* <Box
        className="w-full h-full object-cover absolute flex justify-center items-center z-10"
        rWidth={300}
        rHeight={54}
        rLeft={28}
        rTop={0}
        style={{
          backgroundImage: `url(${
            new URL(`assets/images/server_title_bg.png`, import.meta.url).href
          })`,
          backgroundSize: "cover",
        }}
      >
        <Text rFontSize={24} className="font-bold">
          {server ? server.hostname : "No Server Available"}
        </Text>
      </Box> */}
      {/* <Box
        rWidth={554}
        rHeight={840}
        style={{
          backgroundImage:
            index === 0
              ? `url(${
                  new URL(`assets/images/server_bg_1.png`, import.meta.url).href
                })`
              : `url(${
                  new URL(`assets/images/server_bg_2.png`, import.meta.url).href
                })`,
          backgroundSize: "cover",
          filter: "drop-shadow(0px 0px 28px #394067)",
        }}
        className="absolute"
        rBottom={0}
      >
        <Image
          src={new URL(`assets/images/play_now.png`, import.meta.url).href}
          alt="Server Thumbnail"
          className={clsx(
            "w-full h-full object-cover absolute ",
            server && "hover:cursor-pointer hover:brightness-200"
          )}
          rWidth={400}
          rHeight={61}
          rBottom={28}
          rRight={28}
          onClick={() => {
            if (!server) {
              return;
            }
            ServersConnectService?.connectTo(server);
          }}
        />
      </Box> */}
    </Box>
  );
});

export default Server;
