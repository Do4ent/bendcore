// Kernel viewer page (docs/ui/KERNEL_VIEWPORT.md): the web configuration (webpack.web.js) with
// its own entry and template; the bendcore_kernel_wasm build output is served under /kernel/.
//
//   BENDCORE_KERNEL_WASM_DIR=<build>/wasm_kernel npx webpack serve --config web/kernel-viewer/webpack.config.js
const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const base = require('../../webpack.web.js');
const { workerEntry, workerFileName, workerOptimization } = require('../wasm-worker.config.js');

const root = path.resolve(__dirname, '../..');
const wasmDir = path.resolve(process.env.BENDCORE_KERNEL_WASM_DIR || path.join(root, 'build-wasm-kernel/wasm_kernel'));
const isProduction = process.env.NODE_ENV === 'production';

// ts-loader must keep import() as is: the kernel glue is loaded at run time (webpackIgnore),
// and the commonjs module setting of tsconfig.json would turn it into require().
const rules = base.module.rules.map((rule) =>
  String(rule.test) === String(/\.tsx?$/)
    ? { ...rule, use: { ...rule.use, options: { ...rule.use.options, compilerOptions: { module: 'esnext' } } } }
    : rule,
);

module.exports = {
  ...base,
  context: root,
  entry: { kernelViewer: './src/ui/kernel/kernelViewerEntry.tsx', 'wasm-worker': workerEntry },
  output: { ...base.output, filename: workerFileName(base.output.filename), path: path.join(root, 'dist/kernel-viewer') },
  module: { ...base.module, rules },
  optimization: workerOptimization(base.optimization),
  devServer: {
    ...base.devServer,
    port: Number(process.env.PORT || 3100),
    host: '127.0.0.1',
    hot: false,
    static: [{ directory: wasmDir, publicPath: '/kernel' }],
  },
  plugins: [
    new HtmlWebpackPlugin({ template: path.join(__dirname, 'index.html'), filename: 'index.html', chunks: ['kernelViewer'] }),
    new MiniCssExtractPlugin({ filename: isProduction ? '[name].[contenthash].css' : '[name].css' }),
  ],
};
