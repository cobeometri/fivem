import { clsx } from "@cfx-dev/ui-components";
import { Box, useWindowSize } from "lr-components";
import React from "react";

interface InputProps {
  label?: string;
  className?: string;
  error?: string;
  startContent?: React.ReactNode;
  endContent?: React.ReactNode;
}

function Input({
  label,
  className,
  error,
  startContent,
  endContent,
  placeholder,
  ...rest
}: InputProps &
  React.DetailedHTMLProps<
    React.InputHTMLAttributes<HTMLInputElement>,
    HTMLInputElement
  >) {
  const { ratioWidth } = useWindowSize();

  return (
    <Box className={`flex flex-col ${className} relative`} rGap={14}>
      {label && (
        <label
          htmlFor={label}
          className="text-white font-[510]"
          style={{
            fontSize: `${ratioWidth * 16}px`,
          }}
        >
          {label}
        </label>
      )}
      <Box className="relative">
        {startContent && (
          <Box
            className="absolute left-0 top-1/2 -translate-y-1/2 text-[#656970]"
            style={{
              left: `${ratioWidth * 14}px`
            }}
          >
            {startContent}
          </Box>
        )}
        <input
          type="text"
          className={clsx(
            "w-full font-normal outline-none transition-all duration-300 italic bg-[#D9D9D90D]",
            error ? "placeholder:text-red-500" : "placeholder:text-[#656970]"
          )}
          id={label}
          placeholder={error || placeholder}
          style={{
            border: `1px solid ${error ? "#ef4444" : "#FFFFFF1A"}`,
            fontSize: `${ratioWidth * 15}px`,
            padding: `${ratioWidth * 10}px ${
              ratioWidth * (endContent ? 40 : 12)
            }px ${ratioWidth * 10}px ${ratioWidth * (startContent ? 36.5 : 12)}px`,
            color: "white",
          }}
          {...rest}
        />
        {endContent && (
          <Box
            className="absolute top-1/2 -translate-y-1/2 text-[#ffffff80] hover:text-white transition-colors duration-300 cursor-pointer"
            style={{
              right: `${ratioWidth * 12}px`,
              fontSize: `${ratioWidth * 18}px`,
            }}
          >
            {endContent}
          </Box>
        )}
      </Box>
    </Box>
  );
}

export default Input;