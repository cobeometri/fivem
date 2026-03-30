import { observer } from "mobx-react-lite";
import { makeAutoObservable } from "mobx";
import { AnimatePresence, motion } from "motion/react";
import { Icon } from "@iconify/react";
import { mpMenu } from "../../mpMenu";

// State
class ExternalLinkState {
  url: string | null = null;
  visible = false;

  constructor() {
    makeAutoObservable(this);
  }

  show(url: string) {
    this.url = url;
    this.visible = true;
  }

  hide() {
    this.visible = false;
    this.url = null;
  }

  confirm() {
    if (this.url) {
      mpMenu.openUrlDirect(this.url);
    }
    this.hide();
  }
}

export const externalLinkState = new ExternalLinkState();

export const ExternalLinkDialog = observer(function ExternalLinkDialog() {
  const { visible, url } = externalLinkState;

  return (
    <AnimatePresence>
      {visible && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          transition={{ duration: 0.2 }}
          style={{
            position: "fixed",
            inset: 0,
            zIndex: 9999,
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            background: "rgba(0, 0, 0, 0.7)",
            backdropFilter: "blur(8px)",
          }}
          onClick={() => externalLinkState.hide()}
        >
          <motion.div
            initial={{ opacity: 0, scale: 0.9, y: 20 }}
            animate={{ opacity: 1, scale: 1, y: 0 }}
            exit={{ opacity: 0, scale: 0.9, y: 20 }}
            transition={{ duration: 0.25, ease: "easeOut" }}
            onClick={(e) => e.stopPropagation()}
            style={{
              width: 460,
              background: "linear-gradient(180deg, #1A1B1F 0%, #111214 100%)",
              border: "1px solid rgba(255, 255, 255, 0.08)",
              borderRadius: 16,
              padding: "32px",
              display: "flex",
              flexDirection: "column",
              gap: 20,
            }}
          >
            {/* Header */}
            <div style={{ display: "flex", alignItems: "center", gap: 12 }}>
              <div
                style={{
                  width: 40,
                  height: 40,
                  borderRadius: 10,
                  background: "linear-gradient(180deg, #F19800 0%, #EC6B14 100%)",
                  display: "flex",
                  alignItems: "center",
                  justifyContent: "center",
                }}
              >
                <Icon icon="material-symbols:open-in-new" width={22} color="#fff" />
              </div>
              <div>
                <div style={{ color: "#fff", fontSize: 16, fontWeight: 600 }}>
                  Liên kết bên ngoài
                </div>
                <div style={{ color: "rgba(255,255,255,0.4)", fontSize: 12 }}>
                  Bạn sắp rời khỏi Grand Roleplay
                </div>
              </div>
            </div>

            {/* URL */}
            <div
              style={{
                background: "rgba(255, 255, 255, 0.04)",
                border: "1px solid rgba(255, 255, 255, 0.06)",
                borderRadius: 10,
                padding: "12px 16px",
                wordBreak: "break-all",
                color: "#F19800",
                fontSize: 13,
                fontFamily: "monospace",
              }}
            >
              {url}
            </div>

            {/* Warning */}
            <div
              style={{
                color: "rgba(255, 255, 255, 0.45)",
                fontSize: 12,
                lineHeight: 1.5,
              }}
            >
              Liên kết này có thể không an toàn. Chỉ truy cập nếu bạn tin tưởng nguồn gửi.
            </div>

            {/* Buttons */}
            <div style={{ display: "flex", gap: 10 }}>
              <button
                onClick={() => externalLinkState.hide()}
                style={{
                  flex: 1,
                  padding: "12px 0",
                  borderRadius: 10,
                  border: "1px solid rgba(255, 255, 255, 0.1)",
                  background: "transparent",
                  color: "rgba(255, 255, 255, 0.6)",
                  fontSize: 14,
                  fontWeight: 500,
                  cursor: "pointer",
                }}
              >
                Huỷ bỏ
              </button>
              <button
                onClick={() => externalLinkState.confirm()}
                style={{
                  flex: 1,
                  padding: "12px 0",
                  borderRadius: 10,
                  border: "none",
                  background: "linear-gradient(180deg, #F19800 0%, #EC6B14 100%)",
                  color: "#fff",
                  fontSize: 14,
                  fontWeight: 600,
                  cursor: "pointer",
                }}
              >
                Truy cập
              </button>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
});
