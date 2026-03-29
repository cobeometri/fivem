import { clsx } from "@cfx-dev/ui-components";
import { Box, Image, Text } from "lr-components";
import { observer } from "mobx-react-lite";
// import { useEffect, useState } from "react";
import { ArticleItem } from "../../services/launcher/type";
import { mpMenu } from "../../mpMenu";

import styles from "./News.module.scss";

interface NewsItemProps {
  size?: "sm" | "lg";
  data?: ArticleItem;
}

// const NewsItem = observer(({ size = "sm", data }: NewsItemProps) => {
//   if (!data) {
//     return null;
//   }

//   return (
//     // eslint-disable-next-line jsx-a11y/click-events-have-key-events
//     <div
//       className="w-full h-full relative hover:brightness-150 transition-all"
//       role="button"
//       tabIndex={0}
//       onClick={() => {
//         mpMenu.invokeNative(
//           "openUrl",
//           `https://gta5online.com.vn/news/${data.slug}`
//         );
//       }}
//     >
//       <Image
//         src={`https://api.gta5online.com.vn${data.cover.url}`}
//         alt="News"
//         className="w-full h-full object-cover"
//         fallbackSrc="https://api.gta5online.com.vn/uploads/GIFT_GTA_5_Online_692c4d33c0.gif"
//       />
//       <Box
//         className="absolute bottom-0 w-full"
//         rPadding={[17, 11]}
//         style={{
//           background:
//             // eslint-disable-next-line @stylistic/max-len
//             "linear-gradient(0deg, #95A6F9 -5.27%, rgba(57, 69, 130, 0.60) 20.7%, rgba(34, 42, 79, 0.40) 61.07%, rgba(29, 40, 59, 0.00) 100%), linear-gradient(0deg, rgba(0, 0, 0, 0.80) 0%, rgba(0, 0, 0, 0.48) 81.58%)",
//           textShadow: "0 2px 3px rgba(255, 182, 41, 0.25)",
//         }}
//       >
//         <Text rFontSize={size === "lg" ? 20 : 14} className="font-bold">
//           {data.title}
//         </Text>
//         <Text
//           rFontSize={size === "lg" ? 14 : 10}
//           className="mt-2 font-[400] line-clamp-2"
//         >
//           {data.description}
//         </Text>
//       </Box>
//       <Box className="absolute top-0 right-0 p-2 bg-black bg-opacity-50 rounded-bl-lg">
//         <Text>{new Date(data.updatedAt).toLocaleString()}</Text>
//       </Box>
//     </div>
//   );
// });

const News = observer(() => {
  // const [articles, setArticles] = useState<ArticleItem[]>([]);

  // useEffect(() => {
  //   const fetchArticles = async () => {
  //     // eslint-disable-next-line @typescript-eslint/no-shadow
  //     const articles = await LauncherService.getArticles();
  //     setArticles(articles);
  //   };

  //   fetchArticles();

  //   return () => {
  //     setArticles([]);
  //   };
  // }, [LauncherService, setArticles]);

  return (
    <Box
      position="relative"
      className="w-full flex flex-col items-center gap-[1.35rem]"
    >
      <Box className="w-full flex items-center justify-between">
        <Text rFontSize={24} className="font-semibold uppercase">
          thông tin mới nhất
        </Text>
        <Text
          className={clsx(
            styles["text-link"],
            "uppercase font-normal cursor-pointer"
          )}
          rFontSize={17}
          onClick={() => {
            mpMenu.invokeNative(
              "openUrl",
              "https://discord.gg/saigoncorner"
            );
          }}
        >
          xem thêm
        </Text>
      </Box>
      <Box className="w-full h-[25rem] flex items-center gap-[1.35rem]">
        <Box
          style={{
            backgroundImage: 'url("https://cloud.fivemvn.com/launcher-sgc/server-1.png")',
            backgroundSize: "100% 100%",
            backgroundPosition: "center center",
            backgroundRepeat: "no-repeat",
          }}
          className={clsx(
            "w-1/2 h-full rounded-[.65rem] flex justify-center items-center cursor-pointer group relative overflow-hidden",
            styles["server-container"]
          )}
          onClick={() => {
            mpMenu.invokeNative(
              "openUrl",
              "https://discord.gg/saigoncorner"
            );
          }}
        >
          <Box
            className="absolute inset-0 bg-[#0000004d] z-[1] group-hover:opacity-100 opacity-0 transition-all duration-300"
          />
          <Image
            src="https://cloud.fivemvn.com/launcher-sgc/server-2.png"
            alt="element2"
            className="absolute bottom-0 left-0 w-full h-[17rem] z-[2]"
          />
          <Box className={styles["main-server-group-badge"]}>
            <Text className={styles["main-server-group-badge-title"]}>
              20/09/2025
            </Text>
          </Box>
          <Box className={styles["main-server-group-info"]}>
            <h1 className={styles["main-server-group-info-title"]}>
              Bản cập nhật #1
            </h1>
            <p className={styles["main-server-group-info-subtitle"]}>
              Khám phá những tính năng được bổ sung.
            </p>
          </Box>
        </Box>
        <Box
          style={{
            backgroundImage: 'url("https://cloud.fivemvn.com/launcher-sgc/server-2.png")',
            backgroundSize: "100% 100%",
            backgroundPosition: "center center",
            backgroundRepeat: "no-repeat",
          }}
          className={clsx(
            "w-1/2 h-full rounded-[.65rem] flex justify-center items-center cursor-pointer group relative overflow-hidden",
            styles["server-container"]
          )}
          onClick={() => {
            mpMenu.invokeNative(
              "openUrl",
              "https://discord.gg/saigoncorner"
            );
          }}
        >
          <Box
            className="absolute inset-0 bg-[#0000004d] z-[1] group-hover:opacity-100 opacity-0 transition-all duration-300"
          />
          <Image
            src="https://cloud.fivemvn.com/launcher-sgc/server-2.png"
            alt="element2"
            className="absolute bottom-0 left-0 w-full h-[17rem] z-[2]"
          />
          <Box className={styles["main-server-group-badge"]}>
            <Text className={styles["main-server-group-badge-title"]}>
              21/09/2025
            </Text>
          </Box>
          <Box className={styles["main-server-group-info"]}>
            <h1 className={styles["main-server-group-info-title"]}>
              Bản cập nhật #2
            </h1>
            <p className={styles["main-server-group-info-subtitle"]}>
              Khám phá những tính năng được bổ sung.
            </p>
          </Box>
        </Box>
      </Box>
    </Box>
  );
});

export default News;
