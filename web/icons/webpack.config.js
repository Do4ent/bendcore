// Icon catalog page (src/ui/icons/README.md): every icon of the registry at 32, 24 and 16 px. No
// WASM module: the page shows the icon registry only. The shell's server (web/app) serves the same
// page as /icons.html, where the Playwright scenario tests/e2e-app/ribbon.spec.ts takes its picture.
//
//   npx webpack serve --config web/icons/webpack.config.js      (npm run dev:icons, port 3210)
const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const base = require('../../webpack.web.js');

const root = path.resolve(__dirname, '../..');
const isProduction = process.env.NODE_ENV === 'production';

module.exports = {
  ...base,
  context: root,
  entry: { icons: './src/ui/app/iconCatalogEntry.tsx' },
  output: { ...base.output, path: path.join(root, 'dist/icons') },
  devServer: {
    ...base.devServer,
    port: Number(process.env.PORT || 3210),
    host: '127.0.0.1',
    hot: false,
  },
  plugins: [
    new HtmlWebpackPlugin({ template: path.join(__dirname, 'index.html'), filename: 'index.html', chunks: ['icons'] }),
    new MiniCssExtractPlugin({ filename: isProduction ? '[name].[contenthash].css' : '[name].css' }),
  ],
};
