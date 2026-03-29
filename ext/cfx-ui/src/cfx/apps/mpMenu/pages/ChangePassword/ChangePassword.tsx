import { Box, Image, Text } from "lr-components";
import { observer } from "mobx-react-lite";
import React, { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";

import Input from "../../components/ui/input";
import { AuthLayout } from "../../layouts/AuthLayout";

import { Controller, useForm } from "react-hook-form";
import { yupResolver } from "@hookform/resolvers/yup";
import * as yup from "yup";
import { useLauncherService } from "../../services/launcher/launcher.service";
import { FaRegEye, FaRegEyeSlash } from "react-icons/fa";

const closePopupIcon = new URL("assets/images/close-popup-icon.webp", import.meta.url).href;
const passwordIcon = new URL("assets/images/password-icon.webp", import.meta.url).href;

const schema = yup
  .object({
    currentPassword: yup
      .string()
      .min(6, "Mật khẩu phải có ít nhất 6 ký tự")
      .required("Mật khẩu không được để trống"),
    newPassword: yup
      .string()
      .min(6, "Mật khẩu phải có ít nhất 6 ký tự")
      .required("Mật khẩu không được để trống"),
    passwordConfirmation: yup
      .string()
      .oneOf([yup.ref("newPassword"), ""], "Mật khẩu xác nhận không khớp")
      .required("Mật khẩu xác nhận không được để trống"),
  })
  .required();

const ChangePassword = observer(() => {
  const [showCurrentPassword, setShowCurrentPassword] = useState(false);
  const [showNewPassword, setShowNewPassword] = useState(false);
  const [showConfirmPassword, setShowConfirmPassword] = useState(false);
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
      currentPassword: "",
      newPassword: "",
      passwordConfirmation: "",
    },
  });

  const onSubmit = (data) => {
    LauncherService.changePassword({
      currentPassword: data.currentPassword,
      newPassword: data.newPassword,
    }).then((success) => {
      if (success) {
        navigate("/login");
      }
    });
  };

  useEffect(() => {
    if (!LauncherService.user) {
      navigate("/login");
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
          Đổi mật khẩu
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
            name="currentPassword"
            render={({ field }) => (
              <Input
                label="Mật khẩu hiện tại"
                placeholder="Nhập mật khẩu hiện tại..."
                value={field.value}
                onChange={field.onChange}
                type={showCurrentPassword ? "text" : "password"}
                error={errors.currentPassword?.message}
                startContent={
                  <Image src={passwordIcon} alt="Password" rWidth={16} rHeight={16} />
                }
                endContent={
                  <Box
                    className="cursor-pointer"
                    onClick={() => setShowCurrentPassword(!showCurrentPassword)}
                  >
                    {showCurrentPassword ? <FaRegEyeSlash /> : <FaRegEye />}
                  </Box>
                }
              />
            )}
          />
          <Controller
            control={control}
            name="newPassword"
            render={({ field }) => (
              <Input
                label="Mật khẩu mới"
                placeholder="Nhập mật khẩu mới..."
                value={field.value}
                onChange={field.onChange}
                error={errors.newPassword?.message}
                type={showNewPassword ? "text" : "password"}
                startContent={
                  <Image src={passwordIcon} alt="Password" rWidth={16} rHeight={16} />
                }
                endContent={
                  <Box
                    className="cursor-pointer"
                    onClick={() => setShowNewPassword(!showNewPassword)}
                  >
                    {showNewPassword ? <FaRegEyeSlash /> : <FaRegEye />}
                  </Box>
                }
              />
            )}
          />
          <Controller
            control={control}
            name="passwordConfirmation"
            render={({ field }) => (
              <Input
                label="Xác nhận mật khẩu mới"
                placeholder="Nhập lại mật khẩu mới..."
                value={field.value}
                onChange={field.onChange}
                error={errors.passwordConfirmation?.message}
                type={showConfirmPassword ? "text" : "password"}
                startContent={
                  <Image src={passwordIcon} alt="Password" rWidth={16} rHeight={16} />
                }
                endContent={
                  <Box
                    className="cursor-pointer"
                    onClick={() => setShowConfirmPassword(!showConfirmPassword)}
                  >
                    {showConfirmPassword ? <FaRegEyeSlash /> : <FaRegEye />}
                  </Box>
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
              Lưu thay đổi
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
            navigate("/");
          }}
        >
          <span className="italic">Không muốn đổi?{" "}</span>
          <span className="text-[#37FF00] hover:underline cursor-pointer font-medium ml-[.35rem]">
            Quay về trang chủ
          </span>
        </Text>
      </form>
    </AuthLayout>
  );
});

export default ChangePassword;