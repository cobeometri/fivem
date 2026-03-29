import { Box, Image, Text } from "lr-components";
import { observer } from "mobx-react-lite";
import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { motion, AnimatePresence } from "motion/react";

import Input from "../../components/ui/input";
import { AuthLayout } from "../../layouts/AuthLayout";

import { Controller, useForm } from "react-hook-form";
import { yupResolver } from "@hookform/resolvers/yup";
import * as yup from "yup";
import { useLauncherService } from "../../services/launcher/launcher.service";
import { FaRegEye, FaRegEyeSlash } from "react-icons/fa";
import Checkbox from "../../components/ui/checkbox";

const loginActiveIcon = new URL("assets/images/login-active-icon.webp", import.meta.url).href;
const registerIcon = new URL("assets/images/register-icon.webp", import.meta.url).href;
const registerActiveIcon = new URL("assets/images/register-active-icon.webp", import.meta.url).href;
const authBtnActiveBg = new URL("assets/images/auth-btn-active.webp", import.meta.url).href;
const closePopupIcon = new URL("assets/images/close-popup-icon.webp", import.meta.url).href;

const userIcon = new URL("assets/images/user-icon.webp", import.meta.url).href;
const passwordIcon = new URL("assets/images/password-icon.webp", import.meta.url).href;

const schema = yup.object({
  identifier: yup.string().required("Tên tài khoản hoặc email không được để trống"),
  password: yup
    .string()
    .min(6, "Mật khẩu phải có ít nhất 6 ký tự")
    .required("Mật khẩu không được để trống"),
  rememberMe: yup.boolean(),
});

const Login = observer(() => {
  const [showPassword, setShowPassword] = useState(false);
  const LauncherService = useLauncherService();
  const navigate = useNavigate();
  const {
    handleSubmit,
    formState: { errors },
    control,
  } = useForm({
    resolver: yupResolver(schema),
    mode: "all",
    defaultValues: {
      identifier: "",
      password: "",
      rememberMe: LauncherService.getToken() !== null,
    },
  });

  const onSubmit = (data) => {
    LauncherService.login(data, data.rememberMe);
  };

  useEffect(() => {
    if (LauncherService.user) {
      navigate("/");
    }
  }, [LauncherService.user, navigate]);

  return (
    <AuthLayout>
      <Box
        className="w-full flex items-center bg-[#8AA0CD05] relative"
        rMargin={[0, 0, 34, 0]}
        rHeight={63}
      >
        <button
          type="button"
          className="flex items-center justify-center gap-[.8rem] w-[14.5rem] h-full bg-[#00000003] relative cursor-pointer"
        >
          <Box
            className="absolute inset-0 size-full"
            style={{
              background: `url(${authBtnActiveBg}) no-repeat center center`,
              backgroundSize: '100% 100%'
            }}
          />
          <Image
            src={loginActiveIcon}
            alt="Login Active Icon"
            className="object-contain"
            rWidth={24}
            rHeight={24}
          />
          <Text variant="span" className="text-white font-medium" rFontSize={16}>
            Đăng nhập
          </Text>
        </button>
        <button
          type="button"
          className="flex items-center justify-center gap-[.8rem] w-[14.5rem] h-full bg-[#00000003] relative group cursor-pointer"
          onClick={() => navigate("/register")}
        >
          <Box
            className="absolute inset-0 size-full opacity-0 group-hover:opacity-100 transition-all duration-300"
            style={{
              background: `url(${authBtnActiveBg}) no-repeat center center`,
              backgroundSize: '100% 100%'
            }}
          />
          <Box className="relative" rWidth={24} rHeight={24}>
            <Image
              src={registerIcon}
              alt="Register Icon"
              className="absolute inset-0 object-contain opacity-100 group-hover:opacity-0 transition-opacity duration-300"
              rWidth={24}
              rHeight={24}
            />
            <Image
              src={registerActiveIcon}
              alt="Register Active Icon"
              className="absolute inset-0 object-contain opacity-0 group-hover:opacity-100 transition-opacity duration-300"
              rWidth={24}
              rHeight={24}
            />
          </Box>
          <Text variant="span" className="text-white font-medium" rFontSize={16}>
            Đăng ký
          </Text>
        </button>
        <Image
          src={closePopupIcon}
          alt="Close Popup Icon"
          className="absolute top-[52.5%] -translate-y-1/2 right-[1.25rem] cursor-pointer"
          rWidth={38}
          rHeight={38}
          onClick={() => navigate("/")}
        />
      </Box>
      <form onSubmit={handleSubmit(onSubmit)}>
        <Box
          className="flex flex-col" rGap={18}
          rPadding={[0, 27, 0, 27]}>
          <Controller
            control={control}
            name="identifier"
            render={({ field }) => (
              <Input
                label="Tên đăng nhập hoặc email"
                placeholder="Nhập tên đăng nhập hoặc email..."
                value={field.value}
                onChange={field.onChange}
                error={errors.identifier?.message}
                startContent={
                  <Image src={userIcon} alt="User" rWidth={16} rHeight={16} />
                }
              />
            )}
          />
          <Controller
            control={control}
            name="password"
            render={({ field }) => (
              <Input
                label="Mật khẩu"
                placeholder="Nhập mật khẩu..."
                value={field.value}
                onChange={field.onChange}
                error={errors.password?.message}
                type={showPassword ? "text" : "password"}
                startContent={
                  <Image src={passwordIcon} alt="Password" rWidth={16} rHeight={16} />
                }
                endContent={
                  <Box
                    className="cursor-pointer"
                    onClick={() => setShowPassword(!showPassword)}
                  >
                    {showPassword ? <FaRegEyeSlash /> : <FaRegEye />}
                  </Box>
                }
              />
            )}
          />
          <Box className="flex items-center justify-between">
            <Controller
              control={control}
              name="rememberMe"
              render={({ field }) => (
                <Checkbox
                  label="Ghi nhớ đăng nhập"
                  checked={field.value}
                  onChange={(e) => field.onChange(e.target.checked)}
                />
              )}
            />
            <Text
              rFontSize={16}
              className="text-white hover:underline cursor-pointer italic"
              onClick={() => {
                navigate("/forgot-password");
              }}
            >
              Quên mật khẩu?
            </Text>
          </Box>
        </Box>

        <Box className="flex items-center justify-center" rMargin={[32, 0, 0, 0]} rGap={12}>
          <button
            type="submit"
            className="w-[15.8rem] h-[3.85rem] flex items-center justify-center rounded-[10rem]"
            style={{
              background: 'linear-gradient(180deg, #F19800 0%, #EC6B14 100%)'
            }}
          >
            <Text variant="span" className="text-white font-normal italic" rFontSize={24}>
              Đăng nhập
            </Text>
          </button>
        </Box>
        <Box rPadding={[0, 27, 0, 27]} className="flex items-center" rGap={10} rMargin={[32, 0, 0, 0]}>
          <Box className="flex-1 h-[.05rem] bg-[#FFFFFF80]" />
          <Text variant="span" className="text-[#FFFFFF80] font-normal italic" rFontSize={14}>Hoặc</Text>
          <Box className="flex-1 h-[.05rem] bg-[#FFFFFF80]" />
        </Box>
        <Text
          rFontSize={16}
          rMargin={[27, 0, 0, 0]}
          className="text-center text-white font-[510]"
          onClick={() => {
            navigate("/register");
          }}
        >
          <span className="italic">Chưa có tài khoản?{" "}</span>
          <span className="text-[#37FF00] hover:underline cursor-pointer font-medium ml-[.35rem]">
            Đăng ký ngay
          </span>
        </Text>
        <Text rPadding={[0, 27, 0, 27]} variant="p" className="italic text-center text-[#ffffff80] font-[510]" rFontSize={12} rMargin={[24, 0, 0, 0]}>
          Khi đăng nhập vào Grand Roleplay, bạn xác nhận đã đọc và đồng ý với quy tắc cộng đồng, đồng thời xác nhận bạn trên 18 tuổi. Xem chi tiết quy tắc <span className="text-white cursor-pointer">tại đây.</span>
        </Text>
      </form>
    </AuthLayout>
  );
});

export default Login;