import { Box, Image, Text } from "lr-components";
import { observer } from "mobx-react-lite";

import { mpMenu } from "../../mpMenu";
import styles from "./YoutubeFrame.module.scss";
import { Icon } from "@iconify/react";
import { clsx } from "@cfx-dev/ui-components";

const YoutubeFrame = observer(() => {

  return (
    <Box
      style={{
        backgroundImage: 'url("https://cloud.fivemvn.com/launcher-sgc/youtube.webp")',
        backgroundSize: "100% 100%",
        backgroundPosition: "center center",
        backgroundRepeat: "no-repeat",
      }}
      className={clsx(
        "w-full h-[28rem] rounded-[.65rem] flex justify-center items-center cursor-pointer group relative overflow-hidden",
        styles["server-container"]
      )}
    >
      <Box
        className="absolute inset-0 bg-[#0000004d] z-[1] group-hover:opacity-100 opacity-0 transition-all duration-300"
      />
      <Image
        src="https://cloud.fivemvn.com/launcher-sgc/youtube.webp"
        alt="element2"
        className="absolute bottom-0 left-0 w-full h-[17rem] z-[2]"
      />
      <Box className={styles["main-server-group-badge"]}>
        <Icon icon="logos:youtube-icon" className="size-[1.5rem]" />
        <Text className={clsx(styles["main-server-group-badge-title"], "mt-[.15rem]")}>Youtube</Text>
      </Box>
      <Box className={styles["main-server-group-info"]}>
        <h1 className={styles["main-server-group-info-title"]}>Kênh Youtube</h1>
        <p className={styles["main-server-group-info-subtitle"]}>
          Khám phá những video gameplay hấp dẫn và cập nhật tin tức mới nhất từ server.
        </p>
      </Box>
      <button
        className="z-[3] size-[5rem] rounded-full bg-[#ffffff1a] border-[.1rem] border-[#ffffff33] group-hover:scale-110 group-hover:bg-[#6b728066] transition-all duration-300 flex items-center justify-center
      "
        onClick={() => {
          mpMenu.invokeNative("openUrl", "https://discord.gg/grandrp");
        }}
      >
        <Icon icon="solar:play-bold" className="size-[2.75rem] text-[#fff]" />
      </button>
    </Box>
  );
});

export default YoutubeFrame;
