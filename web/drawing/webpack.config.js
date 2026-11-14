// Drawing editor page (docs/ui/DRAWING_UI.md): the web configuration (webpack.web.js) with its own
// entry and template; the bendcore_drawing_wasm build output (module, JS API, font) is served
// under /drawing-wasm/.
//
//   BENDCORE_DRAWING_WASM_DIR=<build>/wasm_drawing npx webpack serve --config web/drawing/webpack.config.js
const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const base = require('../../webpack.web.js');
const { workerEntry, workerFileName, workerOptimization } = require('../wasm-worker.config.js');

const root = path.resolve(__dirname, '../..');
const wasmDir = path.resolve(
  process.env.BENDCORE_DRAWING_WASM_DIR || path.join(root, 'build-wasm-drawing/wasm_drawing'),
);
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
  entry: { drawingEditor: './src/ui/drawing/drawingEditorEntry.tsx', 'wasm-worker': workerEntry },
  output: { ...base.output, filename: workerFileName(base.output.filename), path: path.join(root, 'dist/drawing') },
  module: { ...base.module, rules },
  optimization: workerOptimization(base.optimization),
  devServer: {
    ...base.devServer,
    port: Number(process.env.PORT || 3150),
    host: '127.0.0.1',
    hot: false,
    static: [{ directory: wasmDir, publicPath: '/drawing-wasm' }],
  },
  plugins: [
    new HtmlWebpackPlugin({ template: path.join(__dirname, 'index.html'), filename: 'index.html', chunks: ['drawingEditor'] }),
    new MiniCssExtractPlugin({ filename: isProduction ? '[name].[contenthash].css' : '[name].css' }),
  ],
};
