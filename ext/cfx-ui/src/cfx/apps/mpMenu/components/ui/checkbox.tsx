import { Box as BoxBase, useWindowSize } from "lr-components";
import React from "react";
import { Icon } from "@iconify/react";

const Box = BoxBase as any;

type CheckboxProps = {
  label?: string;
  className?: string;
  checked?: boolean;
} & React.DetailedHTMLProps<
  React.InputHTMLAttributes<HTMLInputElement>,
  HTMLInputElement
>;

function Checkbox({ label, className, checked, ...rest }: CheckboxProps) {
  const { ratioWidth } = useWindowSize();
  return (
    <Box className={`flex items-center ${className}`} rGap={10}>
      <input
        type="checkbox"
        className="sr-only"
        checked={checked}
        id={`checkbox-${Math.random()}`}
        {...rest}
      />
      <label
        htmlFor={`checkbox-${Math.random()}`}
        className="flex items-center cursor-pointer"
        style={{ gap: `${ratioWidth * 8}px` }}
        onClick={() => {
          if (rest.onChange) {
            rest.onChange({ target: { checked: !checked } } as any);
          }
        }}
      >
        <Box
          className="relative flex items-center justify-center transition-all duration-300"
          style={{
            width: `${ratioWidth * 20}px`,
            height: `${ratioWidth * 20}px`,
            background: checked ? "#ffffff33" : "#D9D9D90D",
            border: `1px solid ${checked ? "#ffffff55" : "#FFFFFF1A"}`
          }}
        >
          {checked ? (
            <Icon
              icon="mdi:check"
              className="text-white"
              style={{
                fontSize: `${ratioWidth * 14}px`,
                fontWeight: "bold",
              }}
            />
          ) : null}
        </Box>
        {label && (
          <span
            className="text-white font-medium"
            style={{
              fontSize: `${ratioWidth * 16}px`,
            }}
          >
            {label}
          </span>
        )}
      </label>
    </Box>
  );
}

export default Checkbox;
