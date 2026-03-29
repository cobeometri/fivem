import { Notification } from "../../services/launcher/type";
import { Box, Text } from "lr-components";
import { motion } from "motion/react";
import { Icon } from "@iconify/react";
import { useLauncherService } from "../../services/launcher/launcher.service";

interface NotificationComponentProps {
  data: Notification;
}

const iconMap: Record<Notification["type"], string> = {
  info: "material-symbols:info-rounded",
  error: "material-symbols:error-rounded",
  success: "material-symbols:check-circle-rounded",
  warning: "material-symbols:warning-rounded",
};

const colorMap: Record<Notification["type"], string> = {
  info: "#3b82f6",
  error: "#ef4444",
  success: "#10b981",
  warning: "#f59e0b",
};

function NotificationComponent({ data }: NotificationComponentProps) {
  const LauncherService = useLauncherService();

  const handleClose = () => {
    LauncherService.removeNotification(data.id);
  };

  return (
    <motion.div
      key={data.id}
      layout
      initial={{ y: "-100%", opacity: 0 }}
      animate={{ y: 0, opacity: 1 }}
      exit={{ y: "-100%", opacity: 0 }}
      transition={{
        duration: 0.3,
        ease: [0.43, 0.13, 0.23, 0.96],
        layout: {
          type: "spring",
          stiffness: 300,
          damping: 30
        }
      }}
      className="mb-4"
    >
      <Box
        rWidth={350}
        rHeight={80}
        className="flex items-center gap-[.9rem] overflow-hidden border border-[#FFFFFF54]"
        rPadding={[16, 16]}
        rBorderRadius={16}
        style={{
          background: "linear-gradient(180deg, #25262A 0%, rgba(0, 0, 0, 0.34) 100%)"
        }}
      >
        <Box
          className="size-[3rem] flex items-center justify-center rounded-full flex-shrink-0"
          style={{
            background: `${colorMap[data.type]}22`,
            border: `1px solid ${colorMap[data.type]}44`,
          }}
        >
          <Icon
            icon={iconMap[data.type]}
            className="size-[1.5rem]"
            style={{ color: colorMap[data.type] }}
          />
        </Box>

        <Box className="flex-1 min-w-0">
          <Text
            rFontSize={16}
            className="font-medium text-white mb-1 capitalize mt-[.25rem]"
          >
            {data.type === "info" && "Thông tin"}
            {data.type === "error" && "Lỗi"}
            {data.type === "success" && "Thành công"}
            {data.type === "warning" && "Cảnh báo"}
          </Text>
          <Text
            rFontSize={13}
            className="text-[#ffffffcc] line-clamp-2 leading-relaxed"
          >
            {data.message}
          </Text>
        </Box>

        <Box className="flex-shrink-0">
          <button
            className="size-[1.75rem] rounded-full bg-[#ffffff1a] border border-[#ffffff22] flex items-center justify-center hover:bg-[#ffffff33] transition-all duration-200"
            onClick={handleClose}
          >
            <Icon
              icon="material-symbols:close-rounded"
              className="size-[1rem] text-[#ffffffcc]"
            />
          </button>
        </Box>
      </Box>
    </motion.div>
  );
}

export default NotificationComponent;
