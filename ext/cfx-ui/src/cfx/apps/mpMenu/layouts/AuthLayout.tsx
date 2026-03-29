import { AnimatePresence, motion } from "motion/react";
import { Box, Image } from "lr-components";

const authBackground = new URL("assets/images/auth-banner.webp", import.meta.url).href;

export const AuthLayout = ({ children }: { children: React.ReactNode }) => {
  return (
    <AnimatePresence mode="wait">
      <Box className="w-screen min-h-screen bg-[#000000BF] absolute inset-0 flex justify-center items-center z-[999]">
        <motion.div
          key="auth-layout"
          initial={{ opacity: 0, scale: .95 }}
          animate={{ opacity: 1, scale: 1 }}
          exit={{ opacity: 0, scale: .95 }}
          transition={{
            duration: 0.25,
            ease: [0.43, 0.13, 0.23, 0.96]
          }}
          className="w-[55rem] h-[47.5rem] rounded-[2.5rem] relative overflow-hidden flex items-center"
          style={{
            filter: 'drop-shadow(0 0 20.4px rgba(48, 60, 78, 0.25))'
          }}
        >
          <Box className="w-[38.5%] h-full overflow-hidden"
          >
            <Image
              src={authBackground}
              alt="Auth Background"
              className="size-full object-fill"
            />
          </Box>
          <Box className="w-[61.5%] h-full bg-[#090B0F]">
            {children}
          </Box>
        </motion.div>
      </Box>
    </AnimatePresence>
  );
};