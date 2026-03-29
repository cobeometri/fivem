import { Box, Image, Text } from "lr-components";
import { observer } from "mobx-react-lite";
import React, { useEffect } from "react";
import { useNavigate } from "react-router-dom";

import Input from "../../components/ui/input";
import { AuthLayout } from "../../layouts/AuthLayout";

import { Controller, useForm } from "react-hook-form";
import { yupResolver } from "@hookform/resolvers/yup";
import * as yup from "yup";
import { useLauncherService } from "../../services/launcher/launcher.service";

const closePopupIcon = new URL("assets/images/close-popup-icon.webp", import.meta.url).href;
const emailIcon = new URL("assets/images/email-icon.webp", import.meta.url).href;

const schema = yup
  .object({
    email: yup
      .string()
      .required("Email không được để trống")
      .test(
        'is-email',
        'Định dạng email không hợp lệ',
        function (value) {
          if (!value) return true;
          if (value.includes('@')) {
            const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
            return emailRegex.test(value);
          }
        }
      ),
  })
  .required();

const ForgotPassword = observer(() => {
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
      email: "",
    },
  });

  const onSubmit = async (data) => {
    await LauncherService.forgotPassword(data.email).then((success) => {
      if (success) {
        navigate("/login");
      }
    });
  };

  useEffect(() => {
    if (LauncherService.user) {
      navigate("/");
    }
  }, [LauncherService.user, navigate]);

  return (
    <AuthLayout>
      <Box
        className="w-full flex items-center justify-between bg-[#8AA0CD05] relative"
        rMargin={[0, 0, 34, 0]}
        rPadding={[0, 27, 0, 27]}
        rHeight={63}
      >
        <Text variant="span" className="text-white font-medium italic" rFontSize={20}>
          Đặt lại mật khẩu
        </Text>
        <Image
          src={closePopupIcon}
          alt="Close Popup Icon"
          className="cursor-pointer"
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
            name="email"
            render={({ field }) => (
              <Input
                label="Email đã đăng ký"
                placeholder="Nhập email của bạn..."
                value={field.value}
                onChange={field.onChange}
                error={errors.email?.message}
                type="text"
                startContent={
                  <Image src={emailIcon} alt="Email" rWidth={16} rHeight={16} />
                }
              />
            )}
          />
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
              Xác nhận
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
            navigate("/login");
          }}
        >
          <span className="italic">Đã nhớ mật khẩu?{" "}</span>
          <span className="text-[#37FF00] hover:underline cursor-pointer font-medium ml-[.35rem]">
            Đăng nhập ngay
          </span>
        </Text>
      </form>
    </AuthLayout>
  );
});

export default ForgotPassword;