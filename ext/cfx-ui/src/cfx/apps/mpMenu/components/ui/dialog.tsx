import { Box as BoxBase, Loading, Text as TextBase } from "lr-components";
import { observer } from "mobx-react-lite";
import React from "react";

interface DialogProps {
  title: string;
  children?: React.ReactNode;
  loading: boolean;
}

const Box = BoxBase as any;
const Text = TextBase as any;

const Dialog = observer(({ title, children, loading }: DialogProps) => {
  return (
    <>
      <Box className="w-[32rem] relative overflow-hidden">
        <Box
          className="relative z-10 bg-[#ffffff1a] flex items-center rounded-t-[.65rem] border-b border-[#ffffff22]"
          rPadding={24}
          style={{
            backdropFilter: "blur(16px)",
            WebkitBackdropFilter: "blur(16px)",
            boxShadow:
              "0 8px 32px rgba(0, 0, 0, 0.1), inset 0 1px 0 rgba(255, 255, 255, 0.3)",
          }}
        >
          <Text rFontSize={22} className="font-semibold uppercase text-white">
            {title}
          </Text>
        </Box>

        <Box
          className="relative bg-[#ffffff1a] border-[.1rem] border-[#4d4d4d] border-t-none rounded-b-[.65rem]"
          rPadding={24}
          style={{
            backdropFilter: "blur(16px)",
            WebkitBackdropFilter: "blur(16px)",
            boxShadow:
              "0 8px 32px rgba(0, 0, 0, 0.1), inset 0 1px 0 rgba(255, 255, 255, 0.3)",
          }}
        >
          {children}
        </Box>

      </Box>
      {loading && (
        <Box
          className="absolute inset-0 bg-[#00000099] flex flex-col gap-[.55rem] justify-center items-center z-30 rounded-[.65rem]"
          style={{
            backdropFilter: "blur(8px)",
            WebkitBackdropFilter: "blur(8px)"
          }}
        >
          <Loading />
          <Text rFontSize={22} className="font-semibold uppercase text-white">Đang xử lý...</Text>
        </Box>
      )}
    </>
  );
});

export default Dialog;
