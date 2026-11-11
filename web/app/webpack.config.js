// Application shell (docs/ui/APP_SHELL.md): the web configuration (webpack.web.js) with its own
// entry and template. It serves the build output of every WASM module at the URL its editor
// loads it from:
//
//   /kernel/         BENDCORE_KERNEL_WASM_DIR    (bendcore_kernel_wasm)
//   /modeling/       BENDCORE_MODELING_WASM_DIR  (bendcore_modeling_wasm)
//   /assembly-wasm/  BENDCORE_ASSEMBLY_WASM_DIR  (bendcore_assembly_wasm)
//   /drawing-wasm/   BENDCORE_DRAWING_WASM_DIR   (bendcore_drawing_wasm, with the font)
//
// The default of each is <repository>/build-wasm-all/wasm_<module> (one emcmake build of all
// four targets, docs/ui/APP_SHELL.md).
//
//   npx webpack serve --config web/app/webpack.config.js
const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const base = require('../../webpack.web.js');
const { workerEntry, workerFileName, workerOptimization } = require('../wasm-worker.config.js');

const root = path.resolve(__dirname, '../..');
const dir = (env, name) => path.resolve(process.env[env] || path.join(root, 'build-wasm-all', name));
const isProduction = process.env.NODE_ENV === 'production';

// ts-loader must keep import() as is: the module glue is loaded at run time (webpackIgnore),
// and the commonjs module setting of tsconfig.json would turn it into require().
const rules = base.module.rules.map((rule) =>
  String(rule.test) === String(/\.tsx?$/)
    ? { ...rule, use: { ...rule.use, options: { ...rule.use.options, compilerOptions: { module: 'esnext' } } } }
    : rule,
);

module.exports = {
  ...base,
  context: root,
  entry: { app: './src/ui/app/appEntry.tsx', 'wasm-worker': workerEntry },
  output: { ...base.output, filename: workerFileName(base.output.filename), path: path.join(root, 'dist/app') },
  module: { ...base.module, rules },
  optimization: workerOptimization(base.optimization),
  devServer: {
    ...base.devServer,
    port: Number(process.env.PORT || 3200),
    host: '127.0.0.1',
    hot: false,
    static: [
      { directory: dir('BENDCORE_KERNEL_WASM_DIR', 'wasm_kernel'), publicPath: '/kernel' },
      { directory: dir('BENDCORE_MODELING_WASM_DIR', 'wasm_modeling'), publicPath: '/modeling' },
      { directory: dir('BENDCORE_ASSEMBLY_WASM_DIR', 'wasm_assembly'), publicPath: '/assembly-wasm' },
      { directory: dir('BENDCORE_DRAWING_WASM_DIR', 'wasm_drawing'), publicPath: '/drawing-wasm' },
    ],
  },
  plugins: [
    new HtmlWebpackPlugin({ template: path.join(__dirname, 'index.html'), filename: 'index.html', chunks: ['app'] }),
    new MiniCssExtractPlugin({ filename: isProduction ? '[name].[contenthash].css' : '[name].css' }),
  ],
};
