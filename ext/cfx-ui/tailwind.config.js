/** @type {import('tailwindcss').Config} */
module.exports = {
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    extend: {
      colors: {
        primary: "#00ffc1",
      },
      extend: {
        scale: {
          "-100": "-1",
        },
      },
    },
  },
  darkMode: "class",
  plugins: [],
};
