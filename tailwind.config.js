/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    './src/**/*.{ts,tsx}',
    './index.html',
  ],
  theme: {
    extend: {
      colors: {
        // BendCore color scheme
        'ribbon-tube': '#1F6FC5',
        'ribbon-create': '#2E9E5B',
        'ribbon-delete': '#D93A3A',
        'ribbon-import': '#0E97A3',
        'ribbon-links': '#8A4FD8',
        'ribbon-edit': '#E8820C',
        'ribbon-update': '#F5B517',
        'ribbon-freeze': '#2D9CDB',
        'ribbon-auxiliary': '#C9920A',
        'ribbon-neutral': '#757575',
        // Semantic color names
        'ochre': {
          600: '#C9920A',
        },
      },
    },
  },
  plugins: [],
};
